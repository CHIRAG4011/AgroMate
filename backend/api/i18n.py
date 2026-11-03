"""AgroMate Internationalization API — Language and translation endpoints."""

import logging
from fastapi import APIRouter
from pydantic import BaseModel

from ..voice.languages import (
    get_all_languages,
    get_all_translations,
    get_language_info,
    SUPPORTED_LANGUAGES,
    set_system_language,
    get_system_language,
)

logger = logging.getLogger(__name__)
router = APIRouter()


class SetLanguageRequest(BaseModel):
    language: str


@router.get("/languages")
async def list_languages():
    """Get all supported languages with metadata."""
    return {
        "status": "success",
        "languages": get_all_languages(),
        "total": len(SUPPORTED_LANGUAGES),
        "current_language": get_system_language(),
    }


@router.get("/current-language")
async def get_current_language():
    """Get the currently active system language."""
    curr = get_system_language()
    info = get_language_info(curr) if curr in SUPPORTED_LANGUAGES else {}
    return {
        "status": "success",
        "language": curr,
        "name": info.get("name", "English"),
        "native": info.get("native", "English"),
    }


@router.post("/set-language")
async def set_active_language(req: SetLanguageRequest):
    """Set the system-wide active language for TTS, TFT display, and all connected robots."""
    lang = req.language.lower().strip()
    if lang not in SUPPORTED_LANGUAGES and lang != "en":
        return {
            "status": "error",
            "message": f"Language '{lang}' not supported.",
        }

    set_system_language(lang)
    logger.info(f"🌐 Active system language updated to: {lang}")

    # Synchronize all active BLE connected robots
    try:
        from ..main import ble_server
        if ble_server and hasattr(ble_server, "connection_manager"):
            for robot in ble_server.connection_manager.get_all_robots():
                if not hasattr(robot, "config") or robot.config is None:
                    robot.config = {}
                robot.config["language"] = lang
                logger.info(f"🤖 Synchronized robot {robot.name} language -> {lang}")
    except Exception as e:
        logger.debug(f"Could not sync language to BLE robots: {e}")

    # Broadcast language update via WebSocket
    try:
        from .websocket import ws_manager
        await ws_manager.broadcast({
            "type": "language_changed",
            "language": lang,
        })
    except Exception:
        pass

    return {
        "status": "success",
        "language": lang,
        "message": f"System language set to {lang}",
    }


@router.get("/translations/{language}")
async def get_translations(language: str):
    """Get all UI translations for a given language code."""
    if language not in SUPPORTED_LANGUAGES:
        return {
            "status": "error",
            "message": f"Language '{language}' not supported. Use /api/i18n/languages to see available options.",
        }

    lang_info = get_language_info(language)
    translations = get_all_translations(language)

    return {
        "status": "success",
        "language": {
            "code": language,
            "name": lang_info["name"],
            "native": lang_info["native"],
            "rtl": lang_info.get("rtl", False),
        },
        "translations": translations,
    }
