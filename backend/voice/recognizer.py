"""
AgroMate Voice Recognizer (Speech-to-Text) — V2

Offline speech recognition using Vosk with Google Speech API fallback.
Supports all 23 Indian languages with auto-detection.
"""

import logging
import io
import json
from typing import Optional
from dataclasses import dataclass

from ..config import get_settings
from ..utils.audio_utils import pcm_to_wav, resample_audio, normalize_audio, trim_silence
from .languages import SUPPORTED_LANGUAGES, get_google_stt_code

logger = logging.getLogger(__name__)


@dataclass
class RecognitionResult:
    """Speech recognition result."""
    text: str
    language: str
    confidence: float
    is_final: bool = True
    alternatives: list = None
    engine_used: str = "vosk"

    def __post_init__(self):
        if self.alternatives is None:
            self.alternatives = []

    def to_dict(self) -> dict:
        return {
            "text": self.text,
            "language": self.language,
            "confidence": round(self.confidence, 4),
            "is_final": self.is_final,
            "alternatives": self.alternatives,
            "engine_used": self.engine_used,
        }


class VoiceRecognizer:
    """
    Speech-to-text engine.

    Primary: Vosk (offline, supports English, Hindi, Gujarati + any downloaded model)
    Fallback: Google Speech Recognition API (online, supports all 23 languages)
    """

    def __init__(self):
        self.settings = get_settings()
        self._vosk_models: dict = {}
        self._vosk_available = False

    async def initialize(self):
        """Load Vosk models for supported languages."""
        try:
            from vosk import Model, SetLogLevel
            SetLogLevel(-1)  # Suppress verbose Vosk logs

            from pathlib import Path
            model_path = Path(self.settings.vosk_model_path)
            models_dir = model_path.parent

            # Load English model (primary)
            if model_path.exists():
                self._vosk_models["en"] = Model(str(model_path))
                self._vosk_available = True
                logger.info(f"Vosk English model loaded from {model_path}")
            else:
                logger.warning(f"Vosk model not found at {model_path}")

            # Dynamically discover all Vosk language models in models directory
            if models_dir.exists():
                for lang_code, lang_info in SUPPORTED_LANGUAGES.items():
                    vosk_model_name = lang_info.get("vosk_model", "")
                    if not vosk_model_name or lang_code in self._vosk_models:
                        continue
                    
                    lang_model_path = models_dir / vosk_model_name
                    if lang_model_path.exists():
                        try:
                            self._vosk_models[lang_code] = Model(str(lang_model_path))
                            self._vosk_available = True
                            logger.info(f"Vosk {lang_info['name']} model loaded")
                        except Exception as e:
                            logger.warning(f"Failed to load Vosk model for {lang_info['name']}: {e}")

            logger.info(f"Vosk models loaded: {list(self._vosk_models.keys())}")

        except ImportError:
            logger.warning("Vosk not installed. Using Google Speech API only.")
        except Exception as e:
            logger.error(f"Vosk initialization failed: {e}")

    async def recognize(
        self,
        audio_data: bytes,
        language: str = "en",
        sample_rate: int = 16000,
    ) -> Optional[RecognitionResult]:
        """
        Recognize speech from audio data.

        Args:
            audio_data: Raw PCM or WAV audio bytes
            language: Language code (en, hi, gu, mr, pa, bn, ta, te, kn, ml, etc.)
                      Use "auto" for automatic language detection.
            sample_rate: Audio sample rate

        Returns:
            RecognitionResult or None
        """
        # Handle auto-detection
        if language == "auto":
            language = await self.detect_language(audio_data)
            logger.info(f"Auto-detected language: {language}")

        # Preprocess audio
        audio_data = normalize_audio(audio_data)
        audio_data = trim_silence(audio_data)

        if len(audio_data) < 200:
            logger.warning("Audio too short for recognition")
            return None

        # Resample to 16kHz if needed
        if sample_rate != 16000:
            audio_data = resample_audio(audio_data, sample_rate, 16000)

        # Try Gemini STT first (extremely fast and accurate multilingual transcription)
        if self.settings.gemini_api_key:
            result = await self._recognize_gemini(audio_data, language, sample_rate)
            if result and result.text.strip():
                return result

        # Try Vosk (offline) first
        if self._vosk_available and language in self._vosk_models:
            result = await self._recognize_vosk(audio_data, language)
            if result and result.text.strip():
                return result

        # Fallback to Google Speech API (supports all 23 languages)
        result = await self._recognize_google(audio_data, language, sample_rate)
        if result:
            return result

        logger.warning("Speech recognition failed on all engines")
        return None

    async def _recognize_vosk(
        self, audio_data: bytes, language: str
    ) -> Optional[RecognitionResult]:
        """Recognize speech using Vosk (offline)."""
        try:
            from vosk import KaldiRecognizer

            model = self._vosk_models[language]
            recognizer = KaldiRecognizer(model, 16000)
            recognizer.SetWords(True)

            # Feed audio in chunks
            chunk_size = 4000
            for i in range(0, len(audio_data), chunk_size):
                chunk = audio_data[i : i + chunk_size]
                recognizer.AcceptWaveform(chunk)

            # Get final result
            result = json.loads(recognizer.FinalResult())
            text = result.get("text", "").strip()

            if text:
                # Extract confidence from word-level results
                words = result.get("result", [])
                if words:
                    confidence = sum(w.get("conf", 0) for w in words) / len(words)
                else:
                    confidence = 0.7  # Default confidence for Vosk

                return RecognitionResult(
                    text=text,
                    language=language,
                    confidence=confidence,
                    engine_used="vosk",
                )

        except Exception as e:
            logger.error(f"Vosk recognition failed: {e}")

        return None

    async def _recognize_gemini(
        self, audio_data: bytes, language: str, sample_rate: int = 16000
    ) -> Optional[RecognitionResult]:
        """Recognize speech using Gemini 2.5 Flash API."""
        try:
            import base64
            import httpx

            # Convert PCM to WAV for Gemini
            wav_data = pcm_to_wav(audio_data, sample_rate=16000)
            b64_audio = base64.b64encode(wav_data).decode("utf-8")

            prompt = (
                "Transcribe this audio. Return ONLY the transcribed text in its native language. "
                "Do not translate it, do not explain it, do not add any notes. Just the transcription. "
                "If Gujarati is spoken, write in Gujarati script. If Hindi is spoken, write in Devanagari script."
            )

            payload = {
                "contents": [{
                    "parts": [
                        {
                            "inlineData": {
                                "mimeType": "audio/wav",
                                "data": b64_audio
                            }
                        },
                        {"text": prompt}
                    ]
                }]
            }

            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={self.settings.gemini_api_key}"

            async with httpx.AsyncClient(timeout=15.0) as client:
                response = await client.post(
                    url,
                    headers={"Content-Type": "application/json"},
                    json=payload
                )

                if response.status_code == 200:
                    data = response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        content = candidates[0].get("content", {})
                        parts = content.get("parts", [])
                        if parts:
                            text = parts[0].get("text", "").strip()
                            # Clean up placeholder brackets or silence indicators
                            clean_text = text.lower().strip(" .[](),*")
                            silence_indicators = [
                                "no speech", "silence", "noise", "no audio", "background noise", 
                                "નો સ્પીચ", "કોઈ અવાજ નથી", "कोई आवाज नहीं", "बिना आवाज", "અવાજ નથી"
                            ]
                            if not clean_text or any(indicator in clean_text for indicator in silence_indicators):
                                return None
                                
                            return RecognitionResult(
                                text=text,
                                language=language,
                                confidence=0.95,
                                engine_used="gemini",
                            )
        except Exception as e:
            logger.error(f"Gemini speech recognition failed: {e}")
        return None

    async def _recognize_google(
        self, audio_data: bytes, language: str, sample_rate: int = 16000
    ) -> Optional[RecognitionResult]:
        """Recognize speech using Google Speech Recognition API (online).
        Supports all 23 Indian languages."""
        try:
            import speech_recognition as sr

            recognizer = sr.Recognizer()

            # Convert PCM to WAV for speech_recognition
            wav_data = pcm_to_wav(audio_data, sample_rate=16000)
            audio = sr.AudioData(wav_data, 16000, 2)

            # Get Google-compatible language code
            lang_code = get_google_stt_code(language)

            text = recognizer.recognize_google(
                audio, language=lang_code, show_all=False
            )

            if text:
                return RecognitionResult(
                    text=text,
                    language=language,
                    confidence=0.8,
                    engine_used="google",
                )

        except ImportError:
            logger.warning("speech_recognition not installed")
        except Exception as e:
            logger.error(f"Google recognition failed: {e}")

        return None

    async def detect_language(self, audio_data: bytes) -> str:
        """
        Auto-detect language from audio.

        Tries recognition in each available Vosk model language and returns
        the one with highest confidence. Falls back to "en" if detection fails.
        """
        best_language = "en"
        best_confidence = 0.0

        for lang in self._vosk_models.keys():
            result = await self._recognize_vosk(audio_data, lang)
            if result and result.confidence > best_confidence:
                best_confidence = result.confidence
                best_language = lang

        # Require minimum confidence for detection
        if best_confidence < 0.3:
            return "en"

        return best_language

    @property
    def is_ready(self) -> bool:
        return self._vosk_available

    @property
    def supported_languages(self) -> list[str]:
        """Languages with available Vosk models (offline STT)."""
        return list(self._vosk_models.keys()) if self._vosk_available else ["en"]

    @property
    def all_supported_languages(self) -> list[str]:
        """All languages supported (including online-only via Google)."""
        return list(SUPPORTED_LANGUAGES.keys())
