"""
AgroMate Configuration Management

Centralized configuration using Pydantic Settings with .env file support.
All configuration values are validated at startup.
"""

from pathlib import Path
from typing import Optional
from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field


# Project root directory
BASE_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = BASE_DIR.parent


class Settings(BaseSettings):
    """Application settings loaded from environment variables and .env file."""

    model_config = SettingsConfigDict(
        env_file=str(BASE_DIR / ".env"),
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore",
    )

    # --- Server ---
    host: str = "0.0.0.0"
    port: int = 8000
    debug: bool = False
    secret_key: str = "change-me-in-production"
    allowed_origins: str = "http://localhost:3000,http://localhost:8080,http://localhost:8000"

    # --- Database ---
    database_url: str = f"sqlite+aiosqlite:///{BASE_DIR / 'agromate.db'}"

    # --- BLE ---
    ble_scan_timeout: int = 10
    ble_reconnect_interval: int = 5
    ble_max_robots: int = 5
    ble_service_uuid: str = "0000ag00-0000-1000-8000-00805f9b34fb"

    # --- AI Models ---
    plant_model_path: str = str(PROJECT_ROOT / "models" / "plant_classifier.onnx")
    disease_model_path: str = str(PROJECT_ROOT / "models" / "disease_classifier.onnx")
    pest_model_path: str = str(PROJECT_ROOT / "models" / "pest_detector.onnx")
    ai_confidence_threshold: float = 0.65

    # --- Voice ---
    vosk_model_path: str = str(PROJECT_ROOT / "models" / "vosk-model-small-en-us")
    tts_voice_en: str = "en-US-AriaNeural"
    tts_voice_hi: str = "hi-IN-SwaraNeural"
    tts_voice_gu: str = "gu-IN-DhwaniNeural"
    tts_sample_rate: int = 16000
    tts_rate: str = "+22%"
    default_language: str = "hi"
    elevenlabs_api_key: str = ""
    elevenlabs_voice_id: str = "EXAVITQu4vr4xnSDxMaL"

    # --- External APIs ---
    plant_id_api_key: str = ""
    plantnet_api_key: str = ""
    openweather_api_key: str = ""
    trefle_api_key: str = ""

    # --- Chatbot ---
    openai_api_key: str = ""
    gemini_api_key: str = ""
    chatbot_model: str = "gpt-4o-mini"
    chatbot_max_tokens: int = 1024

    # --- Cloud Sync ---
    cloud_provider: str = "none"
    firebase_credentials_path: str = ""
    aws_access_key_id: str = ""
    aws_secret_access_key: str = ""
    aws_region: str = "ap-south-1"

    # --- Logging ---
    log_level: str = "INFO"
    log_file: str = str(BASE_DIR / "logs" / "agromate.log")

    # --- Dataset Manager ---
    dataset_dir: str = str(PROJECT_ROOT / "datasets")
    dataset_auto_update: bool = True
    dataset_update_interval_hours: int = 168
    kaggle_username: str = ""
    kaggle_key: str = ""

    @property
    def cors_origins(self) -> list[str]:
        """Parse comma-separated CORS origins."""
        return [origin.strip() for origin in self.allowed_origins.split(",")]


@lru_cache()
def get_settings() -> Settings:
    """Get cached application settings (singleton)."""
    return Settings()
