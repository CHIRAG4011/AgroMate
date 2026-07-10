"""
AgroMate Multimodal Vision Analyser

Utilizes Gemini 2.5 Flash and OpenAI GPT-4o-mini Vision APIs to analyze
crop leaf scans. Provides high-accuracy plant identification, disease diagnosis,
pest detection, and nutrient deficiency analysis with organic & chemical treatments.
"""

import base64
import json
import logging
import re
from typing import Optional, Dict, Any
import httpx

from ..config import get_settings

logger = logging.getLogger(__name__)

# --- Prompt Templates ---

PROMPT_IDENTIFY = """
You are an expert botanist and agronomist. Identify the plant species in the attached image.
Provide a clear, accurate, and localized answer.
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
Respond ONLY with a valid JSON object matching this schema:
{
  "plant_name": "Name of the crop plant",
  "disease_name": "Disease or condition name (or 'Healthy' if healthy)",
  "is_healthy": true or false,
  "confidence": 0.95,
  "severity": "none", "mild", "moderate" or "severe",
  "affected_area_percent": 15.0,
  "treatment": "Brief standard chemical treatment recommendation",
  "organic_treatment": "Detailed organic/natural remedy recommendation (Highest Priority)"
}
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


class VisionAnalyser:
    """Multimodal vision analyser utilizing Gemini and OpenAI APIs."""

    def __init__(self):
        self.settings = get_settings()

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
        # 1. Select appropriate prompt
        if scan_type == "identify":
            prompt = PROMPT_IDENTIFY
        elif scan_type == "diagnose":
            prompt = PROMPT_DIAGNOSE.format(plant_name=plant_name or "Unknown")
        elif scan_type == "pests":
            prompt = PROMPT_PEST
        elif scan_type == "nutrients":
            prompt = PROMPT_NUTRIENTS
        else:
            logger.error(f"Invalid vision scan type: {scan_type}")
            return None

        # 2. Try Gemini Multimodal API first
        if self.settings.gemini_api_key:
            result = await self._call_gemini_vision(image_data, prompt)
            if result:
                return result

        # 3. Try OpenAI Vision API second
        if self.settings.openai_api_key:
            result = await self._call_openai_vision(image_data, prompt)
            if result:
                return result

        logger.warning("No LLM Vision APIs available or successful for scan analysis")
        return None

    async def _call_gemini_vision(self, image_data: bytes, prompt: str) -> Optional[Dict[str, Any]]:
        """Call Gemini API for image analysis."""
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
                else:
                    logger.error(f"Gemini Vision API error: {response.status_code} - {response.text}")
        except Exception as e:
            logger.error(f"Gemini Vision analysis failed: {e}")
        return None

    async def _call_openai_vision(self, image_data: bytes, prompt: str) -> Optional[Dict[str, Any]]:
        """Call OpenAI GPT-4o-mini Vision API for image analysis."""
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
                "max_tokens": 500
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
                else:
                    logger.error(f"OpenAI Vision API error: {response.status_code} - {response.text}")
        except Exception as e:
            logger.error(f"OpenAI Vision analysis failed: {e}")
        return None

    def _parse_json(self, text: str) -> Optional[Dict[str, Any]]:
        """Clean and parse JSON from the LLM response text."""
        try:
            # Strip markdown code block wrapping if present
            cleaned = text.strip()
            if cleaned.startswith("```"):
                cleaned = re.sub(r"^```(?:json)?\n", "", cleaned)
                cleaned = re.sub(r"\n```$", "", cleaned)
            cleaned = cleaned.strip()

            return json.loads(cleaned)
        except Exception as e:
            logger.error(f"Failed to parse vision response JSON: {e}. Text: {text}")
            return None
