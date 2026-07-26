"""
AgroMate Multimodal Vision Analyser

Utilizes Gemini 2.5 Flash and OpenAI GPT-4o-mini Vision APIs to analyze
crop leaf scans. Provides high-accuracy plant identification, disease diagnosis,
pest detection, and nutrient deficiency analysis with organic & chemical treatments.
"""

import base64
import io
import json
import logging
import re
from typing import Optional, Dict, Any
import httpx
from PIL import Image, ImageEnhance, ImageFilter

from ..config import get_settings

logger = logging.getLogger(__name__)

# Words/phrases in LLM responses that indicate an unhelpful rejection
_REJECTION_INDICATORS = [
    "unidentifiable", "unidentified", "cannot identify", "unable to identify",
    "image quality too poor", "poor image quality", "cannot determine",
    "unable to determine", "no plant detected", "not possible to identify",
    "cannot be identified", "not identifiable", "unclear image",
    "image is too", "blurry image", "not clear enough",
]

# --- Prompt Templates ---

PROMPT_IDENTIFY = """
You are an expert botanist and agronomist. Identify the plant species in the attached image.
Provide a clear, accurate, and localized answer.
IMPORTANT: Even if the image is low-resolution, blurry, or partially obscured, you MUST still provide your best guess. Never say "Unknown" or refuse to identify. A low-confidence guess is better than no answer.
Respond ONLY with a valid JSON object matching this schema:
{
  "plant_name": "Common name of the plant crop",
  "scientific_name": "Scientific / Latin name",
  "family": "Botanical family name",
  "confidence": 0.95,
  "common_names": ["Name 1", "Name 2"]
}
Do not write any markdown blocks, introductory, or explanatory text. Return raw JSON.
"""

PROMPT_DIAGNOSE = """
You are an expert plant pathologist. Analyze the health of the plant/crop in the attached image.
Known crop name (if provided): {plant_name}.
If the plant has a disease, infection, mold, rot, or pest issue, set is_healthy to false and provide treatment recommendations.
If it looks healthy, set is_healthy to true and disease_name to "Healthy".
For treatments, prioritize organic and natural remedies (highest priority, detailed), and include standard chemical treatments as a fallback.
IMPORTANT: Even if the image is low-resolution or slightly blurry, you MUST still analyze the plant health. Never refuse to diagnose. Always provide your best assessment.
Respond ONLY with a valid JSON object matching this schema:
{{
  "plant_name": "Name of the crop plant",
  "disease_name": "Disease or condition name (or 'Healthy' if healthy)",
  "is_healthy": true or false,
  "confidence": 0.95,
  "severity": "none", "mild", "moderate" or "severe",
  "affected_area_percent": 15.0,
  "treatment": "Brief standard chemical treatment recommendation",
  "organic_treatment": "Detailed organic/natural remedy recommendation (Highest Priority)"
}}
Do not write any markdown blocks, introductory, or explanatory text. Return raw JSON.
"""


PROMPT_PEST = """
You are an expert entomologist and integrated pest management (IPM) specialist. Identify agricultural pests (insects, bugs, caterpillars, worms) visible in the image.
State the pest name, count, and treatments. Prioritize organic/natural remedies (highest priority), followed by biological control and then chemical treatments as a last resort.
Respond ONLY with a valid JSON object matching this schema:
{
  "pest_name": "Pest name (or 'None' if no pests are detected)",
  "confidence": 0.9,
  "count": 1,
  "bounding_boxes": [],
  "chemical_treatment": "Brief chemical treatment recommendation",
  "organic_treatment": "Detailed organic/natural remedy recommendation (Highest Priority)",
  "prevention": "IPM prevention guidelines",
  "severity": "none", "mild", "moderate", or "severe"
}
Do not write any markdown blocks, introductory, or explanatory text. Return raw JSON.
"""

PROMPT_NUTRIENTS = """
You are a crop nutrition and soil science expert. Analyze the leaf symptoms in the image (yellowing, purpling, chlorosis, necrosis, margin burn).
Diagnose any nutrient deficiencies (Nitrogen, Phosphorus, Potassium, Iron, Magnesium, Calcium, Zinc, etc.).
Prioritize organic/natural options (compost, manure, bone meal, etc.) over synthetic fertilizers.
Respond ONLY with a valid JSON object matching this schema:
{
  "deficiency_detected": true or false,
  "primary_deficiency": "Nutrient deficiency name (e.g. 'Nitrogen (N)' or 'None')",
  "confidence": 0.9,
  "symptoms_matched": "Visual symptoms observed (e.g. interveinal chlorosis, marginal scorch)",
  "treatment": "Synthetic fertilizer recommendation",
  "organic_option": "Organic correction option (Highest Priority)"
}
Do not write any markdown blocks, introductory, or explanatory text. Return raw JSON.
"""

PROMPT_AUTO = """
You are an expert agronomist, botanist, and plant pathologist.
The attached image is captured by a crop leaf-monitoring camera at 320x240 resolution. It is a close-up shot of a plant leaf or crop.
Even if the image is low-resolution, close-up, or slightly blurry, you must still identify the plant and diagnose its health status to the best of your ability.
Never reject the image or say "Image quality too poor", "No plant detected", or "Unknown" unless the image is completely blank, dark, or unrelated to plants.
Identify the crop (e.g., Tomato, Rose, Pepper, Potato, etc.) and determine if the image shows a healthy plant, a disease, a pest, or a nutrient deficiency.

Respond ONLY with a valid JSON object matching this schema:
{
  "scan_type": "identify",
  "plant_name": "Common name of the crop/plant",
  "scientific_name": "Scientific name",
  "confidence": 0.95,
  "is_healthy": true,
  "issue_detected": "Name of the disease or pest (use 'Healthy' if healthy)",
  "severity": "none",
  "treatment": "Recommended organic and chemical treatment guidelines"
}
Do not write any comments, markdown blocks, introductory, or explanatory text. Return raw JSON.
"""

# Anti-rejection instructions appended to ALL prompts before sending
_ANTI_REJECTION_SUFFIX = """

CRITICAL RULES — MUST FOLLOW:
1. You MUST always provide your best identification attempt. Never refuse to answer.
2. NEVER set plant_name to "Unknown", "Unidentifiable", "N/A", or any variant.
3. NEVER say the image quality is too poor, blurry, or unclear. The image has been preprocessed.
4. Even if you are only partially sure, provide your best honest guess and set the confidence value accordingly (e.g. 0.3 for low confidence, 0.6 for moderate, 0.9 for high).
5. Focus on leaf shape, color patterns, vein structure, texture, and any visible features to make your identification.
6. A low-confidence identification is ALWAYS better than a refusal.
"""

# Sentinel returned by API calls when rate-limited/quota exceeded — signals caller to skip to next API
_RATE_LIMITED = object()


class VisionAnalyser:
    """Multimodal vision analyser utilizing Gemini and OpenAI APIs."""

    def __init__(self):
        self.settings = get_settings()

    @staticmethod
    def _enhance_image_for_api(image_data: bytes) -> bytes:
        """
        Enhance raw image bytes before sending to the LLM Vision API.
        Adaptive: heavy enhancement for low-res robot images, light for normal photos.
        """
        try:
            img = Image.open(io.BytesIO(image_data))
            if img.mode != "RGB":
                img = img.convert("RGB")

            w, h = img.size
            is_low_res = w < 640 or h < 480

            if is_low_res:
                # HEAVY enhancement for robot camera (320x240)
                scale = max(640 / w, 480 / h, 2.0)
                new_w, new_h = int(w * scale), int(h * scale)
                img = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
                logger.info(f"Upscaled low-res image from {w}x{h} to {new_w}x{new_h}")

                img = img.filter(ImageFilter.SHARPEN)
                img = img.filter(ImageFilter.DETAIL)
                img = ImageEnhance.Contrast(img).enhance(1.3)
                img = ImageEnhance.Brightness(img).enhance(1.15)
                img = ImageEnhance.Color(img).enhance(1.25)
                img = img.filter(ImageFilter.UnsharpMask(radius=2, percent=120, threshold=3))
            else:
                # LIGHT enhancement for normal phone photos — don't distort
                img = img.filter(ImageFilter.SHARPEN)
                img = ImageEnhance.Contrast(img).enhance(1.08)
                img = ImageEnhance.Color(img).enhance(1.05)

            buf = io.BytesIO()
            img.save(buf, format="JPEG", quality=92)
            enhanced = buf.getvalue()
            logger.info(f"Enhanced image ({'low-res' if is_low_res else 'normal'}): {len(image_data)} -> {len(enhanced)} bytes")
            return enhanced
        except Exception as e:
            logger.warning(f"Image enhancement failed, using original: {e}")
            return image_data

    @staticmethod
    def _is_rejection_response(result: Dict[str, Any], scan_type: str = "identify") -> bool:
        """Check if the LLM response is an unhelpful rejection instead of a real answer."""
        if not result:
            return True

        # For pest/nutrient scans, plant_name may be absent — check the relevant field instead
        if scan_type == "pests":
            pest_name = str(result.get("pest_name", "")).lower().strip()
            # A valid pest response has a pest_name (even "None" is valid = no pests found)
            return not pest_name
        elif scan_type == "nutrients":
            deficiency = str(result.get("primary_deficiency", "")).lower().strip()
            # A valid nutrient response has primary_deficiency (even "None" is valid)
            return not deficiency

        # For identify/diagnose/auto — check plant_name
        plant_name = str(result.get("plant_name", "")).lower().strip()
        disease_name = str(result.get("disease_name", "")).lower().strip()
        issue = str(result.get("issue_detected", "")).lower().strip()

        # Check if plant_name is a rejection
        rejection_names = [
            "unknown", "unidentifiable", "unidentified", "n/a", "none",
            "not identified", "cannot identify", "unable to identify",
            "",
        ]
        if plant_name in rejection_names:
            return True

        # Check all text fields for rejection phrases
        all_text = f"{plant_name} {disease_name} {issue}".lower()
        for indicator in _REJECTION_INDICATORS:
            if indicator in all_text:
                return True

        return False

    def _resolve_common_name(self, result: Dict[str, Any]) -> Dict[str, Any]:
        """Resolve scientific name to common name if plant_name is missing/generic."""
        plant_name = result.get("plant_name")
        sci_name = result.get("scientific_name")
        if sci_name and (not plant_name or plant_name == "Unknown" or plant_name == sci_name):
            from .plant_detector import get_common_name_by_scientific
            resolved = get_common_name_by_scientific(sci_name)
            if resolved:
                result["plant_name"] = resolved
        return result

    async def analyze_image(
        self,
        image_data: bytes,
        scan_type: str,
        plant_name: Optional[str] = None
    ) -> Optional[Dict[str, Any]]:
        """
        Analyze scan image using LLM Vision API.
        
        Args:
            image_data: Raw JPEG/PNG image bytes
            scan_type: "identify", "diagnose", "pests", or "nutrients"
            plant_name: Optional plant name (used to refine diagnosis)
            
        Returns:
            Dict containing parsed JSON result or None if API calls fail
        """
        # 0. Enhance image for better LLM recognition
        enhanced_data = self._enhance_image_for_api(image_data)

        # 1. Select appropriate prompt and append anti-rejection suffix
        if scan_type == "identify":
            prompt = PROMPT_IDENTIFY
        elif scan_type == "diagnose":
            prompt = PROMPT_DIAGNOSE.format(plant_name=plant_name or "Unknown")
        elif scan_type == "pests":
            prompt = PROMPT_PEST
        elif scan_type == "nutrients":
            prompt = PROMPT_NUTRIENTS
        elif scan_type == "auto":
            prompt = PROMPT_AUTO
        else:
            logger.error(f"Invalid vision scan type: {scan_type}")
            return None

        # Append anti-rejection instructions to every prompt
        prompt = prompt + _ANTI_REJECTION_SUFFIX

        # 2. Try Gemini Multimodal API first (one shot only — no retry to preserve quota)
        if self.settings.gemini_api_key:
            result = await self._call_gemini_vision(enhanced_data, prompt)
            if result is _RATE_LIMITED:
                logger.warning("Gemini rate-limited — falling through to OpenAI/local")
            elif result and not self._is_rejection_response(result, scan_type):
                return self._resolve_common_name(result)
            elif result:
                logger.warning(f"Gemini returned weak response for scan_type={scan_type}: {result.get('plant_name')}")
                return self._resolve_common_name(result)  # Return best-effort rather than wasting a retry call

        # 3. Try OpenAI Vision API second (one shot only)
        if self.settings.openai_api_key:
            result = await self._call_openai_vision(enhanced_data, prompt)
            if result is _RATE_LIMITED:
                logger.warning("OpenAI rate-limited — falling through to local fallback")
            elif result and not self._is_rejection_response(result, scan_type):
                return self._resolve_common_name(result)
            elif result:
                logger.warning(f"OpenAI returned weak response for scan_type={scan_type}: {result.get('plant_name')}")
                return self._resolve_common_name(result)  # Return best-effort

        logger.warning("No LLM Vision APIs available — local fallback will handle this")
        return None

    async def _call_gemini_vision(self, image_data: bytes, prompt: str):
        """Call Gemini API for image analysis. Returns dict, _RATE_LIMITED, or None."""
        try:
            b64_image = base64.b64encode(image_data).decode("utf-8")

            payload = {
                "contents": [
                    {
                        "parts": [
                            {"text": prompt},
                            {
                                "inlineData": {
                                    "mimeType": "image/jpeg",
                                    "data": b64_image
                                }
                            }
                        ]
                    }
                ],
                "generationConfig": {
                    "responseMimeType": "application/json"
                }
            }

            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={self.settings.gemini_api_key}"

            async with httpx.AsyncClient(timeout=35.0) as client:
                response = await client.post(
                    url,
                    headers={"Content-Type": "application/json"},
                    json=payload
                )

                if response.status_code == 200:
                    data = response.json()
                    text_response = data["candidates"][0]["content"]["parts"][0]["text"]
                    return self._parse_json(text_response)
                elif response.status_code in (429, 503):
                    logger.warning(f"Gemini rate-limited ({response.status_code}). Skipping to next API.")
                    return _RATE_LIMITED
                else:
                    logger.error(f"Gemini Vision API error: {response.status_code} - {response.text[:200]}")
        except Exception as e:
            logger.error(f"Gemini Vision analysis failed: {e}")
        return None

    async def _call_openai_vision(self, image_data: bytes, prompt: str):
        """Call OpenAI GPT-4o-mini Vision API for image analysis. Returns dict, _RATE_LIMITED, or None."""
        try:
            b64_image = base64.b64encode(image_data).decode("utf-8")

            payload = {
                "model": "gpt-4o-mini",
                "response_format": {"type": "json_object"},
                "messages": [
                    {
                        "role": "user",
                        "content": [
                            {"type": "text", "text": prompt},
                            {
                                "type": "image_url",
                                "image_url": {
                                    "url": f"data:image/jpeg;base64,{b64_image}"
                                }
                            }
                        ]
                    }
                ],
                "max_tokens": 600
            }

            url = "https://api.openai.com/v1/chat/completions"
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.settings.openai_api_key}"
            }

            async with httpx.AsyncClient(timeout=35.0) as client:
                response = await client.post(url, headers=headers, json=payload)

                if response.status_code == 200:
                    data = response.json()
                    text_response = data["choices"][0]["message"]["content"]
                    return self._parse_json(text_response)
                elif response.status_code in (429, 503):
                    logger.warning(f"OpenAI rate-limited ({response.status_code}). Skipping to local fallback.")
                    return _RATE_LIMITED
                else:
                    logger.error(f"OpenAI Vision API error: {response.status_code} - {response.text[:200]}")
        except Exception as e:
            logger.error(f"OpenAI Vision analysis failed: {e}")
        return None

    def _parse_json(self, text: str) -> Optional[Dict[str, Any]]:
        """Clean and parse JSON from the LLM response text."""
        try:
            cleaned = text.strip()
            # Try to find a JSON block wrapped in ```json ... ```
            match = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", cleaned, re.DOTALL)
            if match:
                cleaned = match.group(1)
            else:
                # Try to extract the first occurrence of { to the last occurrence of }
                match = re.search(r"(\{.*\})", cleaned, re.DOTALL)
                if match:
                    cleaned = match.group(1)
            
            return json.loads(cleaned.strip())
        except Exception as e:
            logger.error(f"Failed to parse vision response JSON: {e}. Text: {text}")
            return None
