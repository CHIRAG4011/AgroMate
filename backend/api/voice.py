"""AgroMate Voice API — V2 Speech-to-Text and Text-to-Speech REST endpoints.
Supports all 23 Indian languages with auto-detection and sectioned TTS."""

import logging
from fastapi import APIRouter, UploadFile, File, HTTPException, Query
from fastapi.responses import StreamingResponse
from pydantic import BaseModel
from typing import Optional
import io

from ..voice.recognizer import VoiceRecognizer
from ..voice.synthesizer import VoiceSynthesizer
from ..voice.languages import SUPPORTED_LANGUAGES
from ..utils.audio_utils import validate_audio

logger = logging.getLogger(__name__)
router = APIRouter()

# Shared lazy-initialized voice engines
_recognizer = VoiceRecognizer()
_synthesizer = VoiceSynthesizer()


class TTSRequest(BaseModel):
    text: str
    language: str = "en"
    gender: str = "female"
    voice: Optional[str] = None
    engine: Optional[str] = "edge-tts"


class SectionedTTSRequest(BaseModel):
    text: str
    language: str = "en"
    gender: str = "female"
    engine: Optional[str] = "edge-tts"


@router.post("/stt")
async def speech_to_text(
    file: UploadFile = File(...),
    language: str = Query("en", description="Language code (en, hi, gu, mr, ta, te, etc.) or 'auto' for detection"),
):
    """
    Transcribe uploaded audio file to text.
    Supports all 23 Indian languages.
    Use language='auto' for automatic language detection.
    """
    # Read uploaded file bytes
    try:
        audio_bytes = await file.read()
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Failed to read upload: {e}")

    # Validate audio size / content
    is_valid, msg = validate_audio(audio_bytes)
    if not is_valid:
        if len(audio_bytes) < 44:
            raise HTTPException(status_code=400, detail=msg)

    # Validate language code
    if language != "auto" and language not in SUPPORTED_LANGUAGES:
        raise HTTPException(
            status_code=400,
            detail=f"Unsupported language '{language}'. Use /api/i18n/languages for options.",
        )

    # Initialize STT engine
    try:
        await _recognizer.initialize()
    except Exception as e:
        logger.error(f"Failed to initialize VoiceRecognizer: {e}")
        raise HTTPException(status_code=500, detail="STT engine initialization failed")

    # Perform speech recognition
    try:
        pcm_bytes = audio_bytes
        sample_rate = 16000
        
        # Check if WAV header is present
        if audio_bytes.startswith(b"RIFF") and b"WAVE" in audio_bytes[:15]:
            from ..utils.audio_utils import wav_to_pcm
            try:
                pcm_bytes, sample_rate, _, _ = wav_to_pcm(audio_bytes)
            except Exception as e:
                logger.warning(f"Could not parse WAV headers, passing raw: {e}")

        result = await _recognizer.recognize(pcm_bytes, language=language, sample_rate=sample_rate)
        
        if not result:
            return {
                "status": "success",
                "text": "",
                "language": language if language != "auto" else "en",
                "confidence": 0.0,
                "message": "No speech detected or recognition failed"
            }

        return {
            "status": "success",
            "text": result.text,
            "language": result.language,
            "confidence": result.confidence,
            "engine": result.engine_used,
        }

    except Exception as e:
        logger.error(f"Speech recognition API error: {e}")
        raise HTTPException(status_code=500, detail=f"Speech recognition failed: {e}")


@router.post("/tts")
async def text_to_speech(request: TTSRequest):
    """
    Synthesize text to speech audio.
    Returns streaming MP3 audio.
    Supports all 23 Indian languages.
    """
    if not request.text.strip():
        raise HTTPException(status_code=400, detail="Text parameter cannot be empty")

    if request.language not in SUPPORTED_LANGUAGES:
        raise HTTPException(
            status_code=400,
            detail=f"Unsupported language '{request.language}'. Use /api/i18n/languages for options.",
        )

    # Initialize TTS engine
    try:
        await _synthesizer.initialize()
    except Exception as e:
        logger.error(f"Failed to initialize VoiceSynthesizer: {e}")
        raise HTTPException(status_code=500, detail="TTS engine initialization failed")

    # Generate speech audio
    try:
        result = await _synthesizer.synthesize(
            text=request.text,
            language=request.language,
            voice=request.voice,
            gender=request.gender,
            compress_for_robot=False,
            engine=request.engine or "edge-tts",
        )

        if not result or not result.audio_data:
            raise HTTPException(status_code=500, detail="Voice synthesis failed to generate audio")

        audio_data = result.audio_data
        media_type = "audio/mpeg" if result.format == "mp3" else "audio/wav"
        filename = f"speech.{result.format}"
        
        if result.format == "pcm":
            from ..utils.audio_utils import pcm_to_wav
            audio_data = pcm_to_wav(audio_data, sample_rate=result.sample_rate)
            media_type = "audio/wav"
            filename = "speech.wav"

        audio_stream = io.BytesIO(audio_data)
        
        headers = {
            "Content-Disposition": f'inline; filename="{filename}"',
            "Accept-Ranges": "bytes",
            "X-Voice-Used": result.voice_used,
            "X-Language": result.language,
        }
        
        return StreamingResponse(
            audio_stream,
            media_type=media_type,
            headers=headers
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Text-to-speech API error: {e}")
        raise HTTPException(status_code=500, detail=f"Voice synthesis failed: {e}")


@router.post("/tts/sections")
async def text_to_speech_sections(request: SectionedTTSRequest):
    """
    Synthesize text into multiple audio sections for detailed reports.
    
    Returns JSON metadata about available sections. Each section's audio
    can be fetched individually via /tts/sections/{index}.
    """
    if not request.text.strip():
        raise HTTPException(status_code=400, detail="Text parameter cannot be empty")

    try:
        await _synthesizer.initialize()
    except Exception as e:
        raise HTTPException(status_code=500, detail="TTS engine initialization failed")

    try:
        result = await _synthesizer.synthesize_sections(
            text=request.text,
            language=request.language,
            gender=request.gender,
            engine=request.engine or "edge-tts",
        )

        if not result:
            raise HTTPException(status_code=500, detail="Sectioned synthesis failed")

        # Store sections in memory for retrieval (simple approach)
        # In production, use cache/redis
        import hashlib
        text_hash = hashlib.md5(request.text.encode()).hexdigest()[:8]
        _section_cache[text_hash] = result

        response = result.to_dict()
        response["cache_key"] = text_hash
        return {"status": "success", "result": response}

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Sectioned TTS error: {e}")
        raise HTTPException(status_code=500, detail=f"Sectioned synthesis failed: {e}")


# Simple in-memory cache for sectioned audio
_section_cache: dict = {}


@router.get("/tts/sections/{cache_key}/{index}")
async def get_section_audio(cache_key: str, index: int):
    """Get audio for a specific section of a sectioned TTS result."""
    result = _section_cache.get(cache_key)
    if not result:
        raise HTTPException(status_code=404, detail="Section cache not found. Re-synthesize.")
    
    if index < 0 or index >= len(result.sections):
        raise HTTPException(status_code=404, detail=f"Section {index} not found. Available: 0-{len(result.sections)-1}")
    
    section = result.sections[index]
    audio_data = section.audio_data
    media_type = "audio/mpeg" if section.format == "mp3" else "audio/wav"
    filename = f"section_{index}.{section.format}"

    if section.format == "pcm":
        from ..utils.audio_utils import pcm_to_wav
        audio_data = pcm_to_wav(audio_data, sample_rate=section.sample_rate)
        media_type = "audio/wav"
        filename = f"section_{index}.wav"

    audio_stream = io.BytesIO(audio_data)
    
    title = result.section_titles[index] if index < len(result.section_titles) else f"Section {index+1}"
    
    headers = {
        "Content-Disposition": f'inline; filename="{filename}"',
        "X-Section-Title": title,
        "X-Section-Index": str(index),
        "X-Total-Sections": str(len(result.sections)),
    }
    
    return StreamingResponse(
        audio_stream,
        media_type=media_type,
        headers=headers,
    )


@router.get("/voices")
async def list_voices(language: str = None):
    """
    List available TTS voices.
    Optionally filter by language code.
    """
    try:
        await _synthesizer.initialize()
    except Exception as e:
        raise HTTPException(status_code=500, detail="TTS engine initialization failed")

    voices = await _synthesizer.get_available_voices(language_filter=language)
    return {
        "status": "success",
        "voices": voices,
        "total": len(voices),
    }
