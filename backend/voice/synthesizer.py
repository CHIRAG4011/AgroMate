"""
AgroMate Voice Synthesizer (Text-to-Speech) — V2

High-quality TTS using Edge-TTS with support for all 23 Indian languages.
Optional ElevenLabs integration for premium multilingual voices.
Generates speech audio optimized for BLE transfer to ESP32 or web playback.
"""

import logging
import io
import asyncio
import re
from typing import Optional
from dataclasses import dataclass, field

from ..config import get_settings
from ..utils.audio_utils import compress_for_ble, resample_audio
from .languages import SUPPORTED_LANGUAGES, get_tts_voice

logger = logging.getLogger(__name__)


@dataclass
class SynthesisResult:
    """TTS synthesis result."""
    audio_data: bytes  # PCM or MP3 audio bytes
    format: str  # "mp3" or "pcm" or "wav"
    sample_rate: int
    duration_ms: float
    language: str
    voice_used: str

    def to_dict(self) -> dict:
        return {
            "format": self.format,
            "sample_rate": self.sample_rate,
            "duration_ms": round(self.duration_ms, 1),
            "language": self.language,
            "voice_used": self.voice_used,
            "size_bytes": len(self.audio_data),
        }


@dataclass
class SectionedSynthesisResult:
    """TTS synthesis result with multiple sections for detailed reports."""
    sections: list  # list of SynthesisResult
    total_duration_ms: float
    language: str
    section_titles: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "total_sections": len(self.sections),
            "total_duration_ms": round(self.total_duration_ms, 1),
            "language": self.language,
            "sections": [
                {
                    "index": i,
                    "title": self.section_titles[i] if i < len(self.section_titles) else f"Section {i+1}",
                    "duration_ms": s.duration_ms,
                    "size_bytes": len(s.audio_data),
                }
                for i, s in enumerate(self.sections)
            ],
        }


class VoiceSynthesizer:
    """
    Text-to-speech engine using Edge-TTS.

    Generates natural-sounding speech in 23 Indian languages,
    compressed for efficient BLE transfer to the ESP32 robot.
    """

    def __init__(self):
        self.settings = get_settings()
        self._edge_tts_available = False

    async def initialize(self):
        """Initialize the TTS engine. (Reload Triggered)"""
        try:
            import edge_tts
            self._edge_tts_available = True
            logger.info("Edge-TTS engine initialized (23 languages)")
        except ImportError:
            logger.warning("edge-tts not installed. TTS unavailable.")

    def detect_text_language(self, text: str) -> str:
        """Detect language of text using Unicode character block count."""
        if not text:
            return "en"
        
        counts = {
            "hi": 0,  # Devanagari
            "bn": 0,  # Bengali
            "pa": 0,  # Gurmukhi
            "gu": 0,  # Gujarati
            "or": 0,  # Odia
            "ta": 0,  # Tamil
            "te": 0,  # Telugu
            "kn": 0,  # Kannada
            "ml": 0,  # Malayalam
            "ur": 0,  # Urdu
            "en": 0,  # English
        }
        
        for char in text:
            val = ord(char)
            if 0x0900 <= val <= 0x097F:
                counts["hi"] += 1
            elif 0x0980 <= val <= 0x09FF:
                counts["bn"] += 1
            elif 0x0A00 <= val <= 0x0A7F:
                counts["pa"] += 1
            elif 0x0A80 <= val <= 0x0AFF:
                counts["gu"] += 1
            elif 0x0B00 <= val <= 0x0B7F:
                counts["or"] += 1
            elif 0x0B80 <= val <= 0x0BFF:
                counts["ta"] += 1
            elif 0x0C00 <= val <= 0x0C7F:
                counts["te"] += 1
            elif 0x0C80 <= val <= 0x0CFF:
                counts["kn"] += 1
            elif 0x0D00 <= val <= 0x0D7F:
                counts["ml"] += 1
            elif 0x0600 <= val <= 0x06FF:
                counts["ur"] += 1
            elif (65 <= val <= 90) or (97 <= val <= 122):
                counts["en"] += 1
                
        max_lang = max(counts, key=counts.get)
        if counts[max_lang] > 0:
            return max_lang
        return "en"

    async def synthesize(
        self,
        text: str,
        language: str = "en",
        voice: str = None,
        gender: str = "female",
        rate: str = None,
        pitch: str = "+0Hz",
        compress_for_robot: bool = False,
        engine: str = "gemini",
    ) -> Optional[SynthesisResult]:
        """
        Synthesize speech from text.

        Args:
            text: Text to speak
            language: Language code (en, hi, gu, mr, pa, bn, ta, te, kn, ml, etc.)
            voice: Optional specific voice name override
            gender: "female" or "male"
            rate: Speaking rate adjustment (e.g., "+20%", "-10%")
            pitch: Pitch adjustment (e.g., "+5Hz", "-3Hz")
            compress_for_robot: Whether to compress for BLE transfer
            engine: Voice engine ("edge-tts" or "elevenlabs")

        Returns:
            SynthesisResult with audio data
        """
        if not text.strip():
            return None

        # Fast and clear default speech rate (+22% is energetic, clear, and avoids sluggish pauses)
        if rate is None:
            rate = getattr(self.settings, "tts_rate", "+22%") or "+22%"

        # Auto language detection
        if language == "auto" or not language or language == "en":
            # If default is English, check if the actual text contains local scripts
            detected = self.detect_text_language(text)
            if detected != "en" or language == "auto":
                language = detected
                logger.info(f"Auto-detected TTS language for speech synthesis: {language}")
        
        result = None

        # Check if ElevenLabs engine is requested and available
        if engine == "elevenlabs" and self.settings.elevenlabs_api_key:
            el_voice = voice
            if not el_voice:
                el_voice = self.settings.elevenlabs_voice_id or ("weA4Q36twV5kwSaTEL0Q" if gender == "female" else "pNInz6obpgHs517G4SMt")
            result = await self._synthesize_elevenlabs(text, el_voice, language)

        # Check if Gemini engine is requested and available (or fallback from ElevenLabs)
        if not result and (engine == "elevenlabs" or engine == "gemini") and self.settings.gemini_api_key:
            voice_name = voice or ("Kore" if gender == "female" else "Puck")
            result = await self._synthesize_gemini(text, voice_name, language)

        # Try Edge-TTS
        if not result:
            if voice is None:
                voice = get_tts_voice(language, gender)
            if self._edge_tts_available:
                result = await self._synthesize_edge_tts(text, voice, rate, pitch, language)

        # Fallback to pyttsx3 (offline)
        if not result:
            result = await self._synthesize_pyttsx3(text, language)

        # Handle compression and raw PCM decoding for the robot
        if result and compress_for_robot:
            if result.format != "pcm":
                from ..utils.audio_utils import decode_to_pcm
                pcm_data = decode_to_pcm(result.audio_data, result.format)
                result.audio_data = pcm_data
                result.format = "pcm"
                result.sample_rate = 16000
            
            result.audio_data = compress_for_ble(result.audio_data)

        return result

    async def _synthesize_gemini(
        self,
        text: str,
        voice_name: str,
        language: str,
    ) -> Optional[SynthesisResult]:
        """Synthesize using Gemini 2.5 Flash Preview TTS API."""
        try:
            import base64
            import httpx

            clean_text = self._clean_for_tts(text)

            payload = {
                "contents": [{"parts": [{"text": clean_text}]}],
                "generationConfig": {
                    "responseModalities": ["AUDIO"],
                    "speechConfig": {
                        "voiceConfig": {
                            "prebuiltVoiceConfig": {
                                "voiceName": voice_name or "Puck"
                            }
                        }
                    }
                }
            }

            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-preview-tts:generateContent?key={self.settings.gemini_api_key}"

            async with httpx.AsyncClient(timeout=30.0) as client:
                response = await client.post(url, json=payload)
                
                if response.status_code == 200:
                    data = response.json()
                    parts = data["candidates"][0]["content"]["parts"]
                    for p in parts:
                        if "inlineData" in p:
                            audio_b64 = p["inlineData"]["data"]
                            audio_data = base64.b64decode(audio_b64)
                            
                            # Resample from 24kHz to 16kHz for BLE/robot compatibility
                            audio_data_resampled = resample_audio(audio_data, 24000, 16000)
                            
                            # Estimate duration (16kHz, 16-bit mono = 32000 bytes per second)
                            duration_ms = (len(audio_data_resampled) / 32000) * 1000
                            
                            return SynthesisResult(
                                audio_data=audio_data_resampled,
                                format="pcm",
                                sample_rate=16000,
                                duration_ms=duration_ms,
                                language=language,
                                voice_used=f"gemini:{voice_name or 'Puck'}",
                            )
                else:
                    logger.error(f"Gemini TTS synthesis failed: {response.status_code} - {response.text}")
                    return None

        except Exception as e:
            logger.error(f"Gemini TTS error: {e}")
            return None

    async def _synthesize_elevenlabs(
        self,
        text: str,
        voice_id: str,
        language: str,
    ) -> Optional[SynthesisResult]:
        """Synthesize using ElevenLabs Multilingual v2 API."""
        try:
            import httpx
            api_key = self.settings.elevenlabs_api_key
            if not api_key:
                logger.warning("ElevenLabs API key not configured. Falling back.")
                return None

            clean_text = self._clean_for_tts(text)

            payload = {
                "text": clean_text,
                "model_id": "eleven_multilingual_v2",
                "voice_settings": {
                    "stability": 0.5,
                    "similarity_boost": 0.75
                }
            }

            headers = {
                "Accept": "audio/mpeg",
                "Content-Type": "application/json",
                "xi-api-key": api_key
            }

            url = f"https://api.elevenlabs.io/v1/text-to-speech/{voice_id}"

            async with httpx.AsyncClient(timeout=30.0) as client:
                response = await client.post(url, json=payload, headers=headers)
                
                if response.status_code == 200:
                    audio_data = response.content
                    # Estimate duration from content size
                    duration_ms = (len(audio_data) / 16000) * 1000
                    
                    return SynthesisResult(
                        audio_data=audio_data,
                        format="mp3",
                        sample_rate=44100,
                        duration_ms=duration_ms,
                        language=language,
                        voice_used=f"elevenlabs:{voice_id}",
                    )
                else:
                    logger.error(f"ElevenLabs synthesis failed: {response.status_code} - {response.text}")
                    if response.status_code == 402 and voice_id != "EXAVITQu4vr4xnSDxMaL":
                        logger.warning("ElevenLabs library voice requires paid plan. Retrying with pre-made voice.")
                        return await self._synthesize_elevenlabs(text, "EXAVITQu4vr4xnSDxMaL", language)
                    return None

        except Exception as e:
            logger.error(f"ElevenLabs error: {e}")
            return None

    async def synthesize_sections(
        self,
        text: str,
        language: str = "en",
        voice: str = None,
        gender: str = "female",
        rate: str = None,
        engine: str = "edge-tts",
    ) -> Optional[SectionedSynthesisResult]:
        """
        Synthesize a long text by splitting into logical sections.
        
        Used for detailed AI reports so the user can pause/skip between sections.
        Splits on markdown headers, numbered lists, or paragraph breaks.
        """
        if not text.strip():
            return None

        # Split text into sections
        sections_text = self._split_into_sections(text)
        
        if not sections_text:
            # If splitting fails, treat entire text as one section
            sections_text = [("Full Report", text)]

        # Synthesize each section
        sections = []
        section_titles = []
        total_duration = 0.0

        for title, section_text in sections_text:
            if not section_text.strip():
                continue
            
            result = await self.synthesize(
                text=section_text,
                language=language,
                voice=voice,
                gender=gender,
                rate=rate,
                engine=engine,
            )
            if result:
                sections.append(result)
                section_titles.append(title)
                total_duration += result.duration_ms

        if not sections:
            return None

        return SectionedSynthesisResult(
            sections=sections,
            total_duration_ms=total_duration,
            language=language,
            section_titles=section_titles,
        )

    def _split_into_sections(self, text: str) -> list[tuple[str, str]]:
        """Split text into named sections for TTS playback."""
        sections = []
        
        # Try splitting by markdown headers (## or **Title**)
        header_pattern = r'(?:^|\n)(?:#{1,3}\s+(.+?)$|(?:\*\*(.+?)\*\*))'
        parts = re.split(header_pattern, text, flags=re.MULTILINE)
        
        if len(parts) > 3:
            # We have headers — group content under them
            current_title = "Introduction"
            current_text = ""
            
            i = 0
            while i < len(parts):
                part = parts[i]
                if part is None:
                    i += 1
                    continue
                
                # Check if this looks like a header
                stripped = part.strip()
                if stripped and len(stripped) < 80 and not stripped.endswith('.'):
                    if current_text.strip():
                        sections.append((current_title, current_text.strip()))
                    current_title = stripped
                    current_text = ""
                else:
                    current_text += part
                i += 1
            
            if current_text.strip():
                sections.append((current_title, current_text.strip()))
        
        if not sections:
            # Fallback — split by double newlines (paragraphs)
            paragraphs = text.split("\n\n")
            for i, para in enumerate(paragraphs):
                if para.strip():
                    title = f"Section {i + 1}"
                    # Try to extract first line as title
                    lines = para.strip().split("\n")
                    if len(lines) > 1 and len(lines[0]) < 80:
                        title = lines[0].strip().strip("*#:- ")
                    sections.append((title, para.strip()))
        
        return sections

    async def _synthesize_edge_tts(
        self,
        text: str,
        voice: str,
        rate: str,
        pitch: str,
        language: str,
    ) -> Optional[SynthesisResult]:
        """Synthesize using Edge-TTS (Microsoft neural voices)."""
        try:
            import edge_tts

            # Clean text for TTS — remove markdown formatting
            clean_text = self._clean_for_tts(text)
            effective_rate = rate or getattr(self.settings, "tts_rate", "+22%") or "+22%"

            communicate = edge_tts.Communicate(
                text=clean_text,
                voice=voice,
                rate=effective_rate,
                pitch=pitch,
            )

            audio_buffer = io.BytesIO()

            async for chunk in communicate.stream():
                if chunk["type"] == "audio":
                    audio_buffer.write(chunk["data"])

            audio_data = audio_buffer.getvalue()

            if not audio_data:
                return None

            # Estimate duration (rough estimate from MP3 size)
            duration_ms = len(audio_data) / 16 * 8  # Very rough estimate

            return SynthesisResult(
                audio_data=audio_data,
                format="mp3",
                sample_rate=24000,
                duration_ms=duration_ms,
                language=language,
                voice_used=voice,
            )

        except Exception as e:
            logger.error(f"Edge-TTS synthesis failed: {e}")
            return None

    async def _synthesize_pyttsx3(
        self, text: str, language: str
    ) -> Optional[SynthesisResult]:
        """Fallback TTS using pyttsx3 (offline)."""
        try:
            import pyttsx3

            engine = pyttsx3.init()

            # Set language-appropriate voice
            voices = engine.getProperty("voices")
            for v in voices:
                if language == "hi" and "hindi" in v.name.lower():
                    engine.setProperty("voice", v.id)
                    break
                elif language == "en" and "english" in v.name.lower():
                    engine.setProperty("voice", v.id)
                    break

            engine.setProperty("rate", 190)

            # Save to buffer
            import tempfile
            import os

            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                temp_path = f.name

            engine.save_to_file(text, temp_path)
            engine.runAndWait()

            with open(temp_path, "rb") as f:
                audio_data = f.read()

            os.unlink(temp_path)

            return SynthesisResult(
                audio_data=audio_data,
                format="wav",
                sample_rate=22050,
                duration_ms=len(audio_data) / 44.1,
                language=language,
                voice_used="pyttsx3",
            )

        except ImportError:
            logger.warning("pyttsx3 not installed")
        except Exception as e:
            logger.error(f"pyttsx3 synthesis failed: {e}")

        return None

    def _clean_for_tts(self, text: str) -> str:
        """Remove markdown and formatting artifacts from text for TTS."""
        # Remove markdown bold/italic
        text = re.sub(r'\*\*(.*?)\*\*', r'\1', text)
        text = re.sub(r'\*(.*?)\*', r'\1', text)
        # Remove markdown headers
        text = re.sub(r'^#{1,6}\s+', '', text, flags=re.MULTILINE)
        # Remove bullet points
        text = re.sub(r'^[•\-\*]\s+', '', text, flags=re.MULTILINE)
        # Remove emoji
        text = re.sub(r'[\U00010000-\U0010ffff]', '', text, flags=re.UNICODE)
        # Collapse whitespace
        text = re.sub(r'\n{3,}', '\n\n', text)
        return text.strip()

    async def get_available_voices(self, language_filter: str = None) -> list[dict]:
        """List all available TTS voices, optionally filtered by language."""
        voices = []
        if self._edge_tts_available:
            try:
                import edge_tts
                available = await edge_tts.list_voices()
                
                # Build filter list from supported languages
                if language_filter:
                    lang_info = SUPPORTED_LANGUAGES.get(language_filter)
                    if lang_info:
                        locale_prefix = lang_info["code"][:2]
                    else:
                        locale_prefix = language_filter
                    filter_prefixes = [locale_prefix]
                else:
                    # All supported language prefixes
                    filter_prefixes = list(set(
                        lang["code"][:2] for lang in SUPPORTED_LANGUAGES.values()
                    ))
                
                for v in available:
                    locale = v.get("Locale", "")
                    if any(locale.startswith(prefix + "-") or locale.startswith(prefix + "_") 
                           for prefix in filter_prefixes):
                        voices.append({
                            "name": v.get("ShortName", ""),
                            "locale": locale,
                            "gender": v.get("Gender", ""),
                            "friendly_name": v.get("FriendlyName", ""),
                        })
            except Exception as e:
                logger.error(f"Failed to list voices: {e}")

        return voices

    @property
    def is_ready(self) -> bool:
        return self._edge_tts_available

    @property
    def supported_languages(self) -> list[str]:
        return list(SUPPORTED_LANGUAGES.keys())
