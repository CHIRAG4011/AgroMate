"""AgroMate Analytics API — Dashboard and usage statistics."""

import logging
from fastapi import APIRouter

from ..database.crud import get_session, get_analytics_summary, get_disease_statistics
from ..utils.analytics import analytics

logger = logging.getLogger(__name__)
router = APIRouter()


@router.get("/")
async def get_analytics(days: int = 7):
    """Get analytics summary for the dashboard."""
    async with await get_session() as session:
        db_summary = await get_analytics_summary(session, days=days)
        memory_summary = analytics.get_summary()
        return {
            "status": "success",
            "database": db_summary,
            "runtime": memory_summary,
        }


@router.get("/diseases")
async def get_disease_stats(days: int = 30):
    """Get disease frequency statistics."""
    async with await get_session() as session:
        stats = await get_disease_statistics(session, days=days)
        return {"status": "success", "diseases": stats}


@router.get("/weather")
async def get_weather(lat: float = 23.0, lon: float = 72.6, city: str = None):
    """Get weather data with agricultural advisory."""
    from ..services.weather import WeatherService

    service = WeatherService()
    weather = await service.get_current_weather(lat, lon)
    forecast = await service.get_forecast(lat, lon, days=5)
    advisory = service.get_agricultural_advisory(weather) if weather else {}

    if weather and city:
        weather["location"] = city

    return {
        "status": "success",
        "weather": weather,
        "forecast": forecast,
        "advisory": advisory,
    }


@router.get("/dashboard")
async def get_dashboard_summary(days: int = 30):
    """Combined dashboard summary — stats, weather, robots in one call."""
    async with await get_session() as session:
        db_summary = await get_analytics_summary(session, days=days)
        disease_stats = await get_disease_statistics(session, days=days)
        memory_summary = analytics.get_summary()

    # Robot status
    try:
        from ..main import ble_server
        robots_data = ble_server.get_status() if ble_server else {"robots": [], "running": False}
    except Exception:
        robots_data = {"robots": [], "running": False}

    return {
        "status": "success",
        "database": db_summary,
        "runtime": memory_summary,
        "diseases": disease_stats,
        "robots": robots_data,
    }


@router.get("/keys")
async def check_api_keys():
    """Verify and check the connection status of all external API keys."""
    import httpx
    from ..config import get_settings
    
    settings = get_settings()
    results = {}

    async def check_key(name, key, test_url, method="GET", headers=None, params=None, json=None, expected_unauthorized_status=401):
        if not key or len(key.strip()) == 0:
            return {"status": "not_connected", "error": "API Key is missing or empty"}
        
        try:
            async with httpx.AsyncClient(timeout=5) as client:
                if method == "GET":
                    response = await client.get(test_url, headers=headers, params=params)
                elif method == "POST":
                    response = await client.post(test_url, headers=headers, params=params, json=json)
                
                # Check for unauthorized status codes
                if response.status_code == expected_unauthorized_status or response.status_code == 403 or (response.status_code == 400 and "invalid" in response.text.lower()):
                    return {"status": "error", "error": f"Invalid API Key (HTTP {response.status_code})"}
                
                return {"status": "connected", "error": None}
        except Exception as e:
            return {"status": "error", "error": f"Connection failed: {str(e)}"}

    # Gemini
    gemini_key = settings.gemini_api_key
    results["gemini"] = await check_key(
        "Gemini",
        gemini_key,
        f"https://generativelanguage.googleapis.com/v1beta/models?key={gemini_key}",
        method="GET",
        expected_unauthorized_status=400
    )

    # OpenAI
    openai_key = settings.openai_api_key
    results["openai"] = await check_key(
        "OpenAI",
        openai_key,
        "https://api.openai.com/v1/models",
        method="GET",
        headers={"Authorization": f"Bearer {openai_key}"}
    )

    # ElevenLabs
    elevenlabs_key = settings.elevenlabs_api_key
    results["elevenlabs"] = await check_key(
        "ElevenLabs",
        elevenlabs_key,
        "https://api.elevenlabs.io/v1/voices",
        method="GET",
        headers={"xi-api-key": elevenlabs_key}
    )

    # OpenWeather
    openweather_key = settings.openweather_api_key
    results["openweather"] = await check_key(
        "OpenWeather",
        openweather_key,
        f"https://api.openweathermap.org/data/2.5/weather?lat=0&lon=0&appid={openweather_key}",
        method="GET"
    )

    # Plant.id
    plant_id_key = settings.plant_id_api_key
    results["plant_id"] = await check_key(
        "Plant.id",
        plant_id_key,
        "https://api.plant.id/v3/usage_info",
        method="GET",
        headers={"Api-Key": plant_id_key}
    )

    # PlantNet
    plantnet_key = settings.plantnet_api_key
    results["plantnet"] = await check_key(
        "Pl@ntNet",
        plantnet_key,
        "https://my-api.plantnet.org/v2/_status",
        method="GET",
        params={"api-key": plantnet_key}
    )

    # Trefle
    trefle_key = settings.trefle_api_key
    results["trefle"] = await check_key(
        "Trefle",
        trefle_key,
        "https://trefle.io/api/v1/plants/search",
        method="GET",
        params={"token": trefle_key, "q": "test"}
    )

    return {"status": "success", "keys": results}


