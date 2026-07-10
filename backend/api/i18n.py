"""AgroMate Internationalization API — Language and translation endpoints."""

import logging
from fastapi import APIRouter

from ..voice.languages import (
    get_all_languages,
    get_all_translations,
    get_language_info,
    SUPPORTED_LANGUAGES,
)

logger = logging.getLogger(__name__)
router = APIRouter()


@router.get("/languages")
async def list_languages():
    """Get all supported languages with metadata."""
    return {
        "status": "success",
        "languages": get_all_languages(),
        "total": len(SUPPORTED_LANGUAGES),
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
