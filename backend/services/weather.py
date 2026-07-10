"""
AgroMate Weather Service

OpenWeatherMap API integration for agricultural weather forecasting
with caching and farming-specific interpretation.
"""

import logging
from typing import Optional
from datetime import datetime, timezone

import httpx

from ..config import get_settings

logger = logging.getLogger(__name__)


class WeatherService:
    """Agricultural weather service with caching."""

    def __init__(self):
        self.settings = get_settings()
        self._cache: dict[str, dict] = {}

    async def initialize(self):
        logger.info("Weather service initialized")

    async def get_current_weather(
        self, latitude: float, longitude: float
    ) -> Optional[dict]:
        """Get current weather for a location."""
        if self.settings.openweather_api_key:
            try:
                async with httpx.AsyncClient(timeout=15) as client:
                    response = await client.get(
                        "https://api.openweathermap.org/data/2.5/weather",
                        params={
                            "lat": latitude,
                            "lon": longitude,
                            "appid": self.settings.openweather_api_key,
                            "units": "metric",
                        },
                    )

                    if response.status_code == 200:
                        data = response.json()
                        return self._parse_weather(data)
                    else:
                        logger.warning(f"OpenWeather API returned status {response.status_code}. Falling back to Open-Meteo.")
            except Exception as e:
                logger.error(f"OpenWeather API error: {e}. Falling back to Open-Meteo.")

        return await self._get_openmeteo_weather(latitude, longitude)

    async def get_forecast(
        self, latitude: float, longitude: float, days: int = 5
    ) -> Optional[list]:
        """Get weather forecast for upcoming days."""
        if self.settings.openweather_api_key:
            try:
                async with httpx.AsyncClient(timeout=15) as client:
                    response = await client.get(
                        "https://api.open-meteo.com/v1/forecast" if not self.settings.openweather_api_key else "https://api.openweathermap.org/data/2.5/forecast",
                        params={
                            "lat": latitude,
                            "lon": longitude,
                            "appid": self.settings.openweather_api_key,
                            "units": "metric",
                            "cnt": days * 8,
                        },
                    )

                    if response.status_code == 200:
                        data = response.json()
                        return self._parse_forecast(data)
                    else:
                        logger.warning(f"OpenWeather API returned status {response.status_code}. Falling back to Open-Meteo forecast.")
            except Exception as e:
                logger.error(f"Forecast API error: {e}. Falling back to Open-Meteo forecast.")

        return await self._get_openmeteo_forecast(latitude, longitude, days)

    def get_agricultural_advisory(self, weather: dict) -> dict:
        """Generate agricultural advisory based on weather conditions."""
        advisories = []
        risk_level = "low"

        temp = weather.get("temperature", 25)
        humidity = weather.get("humidity", 60)
        wind_speed = weather.get("wind_speed", 5)
        rain = weather.get("rain_mm", 0)
        description = weather.get("description", "")

        # Temperature advisories
        if temp > 40:
            advisories.append("🔥 Extreme heat — irrigate frequently, use shade nets, avoid midday fieldwork")
            risk_level = "high"
        elif temp > 35:
            advisories.append("☀️ Hot weather — increase irrigation frequency, mulch around plants")
            risk_level = "moderate"
        elif temp < 5:
            advisories.append("❄️ Frost risk — cover sensitive crops, avoid irrigating in evening")
            risk_level = "high"
        elif temp < 10:
            advisories.append("🌡️ Cool weather — protect tropical crops, delay transplanting")
            risk_level = "moderate"

        # Humidity advisories
        if humidity > 85:
            advisories.append("💧 High humidity — watch for fungal diseases, improve air circulation")
            risk_level = max(risk_level, "moderate")
        elif humidity < 30:
            advisories.append("🏜️ Low humidity — increase irrigation, consider misting for greenhouse crops")

        # Rain advisories
        if rain > 50:
            advisories.append("🌧️ Heavy rain expected — ensure drainage, delay spraying, protect seedlings")
            risk_level = "high"
        elif rain > 20:
            advisories.append("🌦️ Rain expected — skip irrigation, good time for transplanting")
        elif rain == 0:
            advisories.append("☀️ No rain — maintain regular irrigation schedule")

        # Wind advisories
        if wind_speed > 30:
            advisories.append("💨 Strong winds — stake tall plants, avoid spraying pesticides")
            risk_level = "high"
        elif wind_speed > 15:
            advisories.append("🍃 Moderate wind — avoid pesticide application, secure greenhouse covers")

        # Spraying conditions
        good_spraying = (
            wind_speed < 10
            and humidity > 40
            and humidity < 80
            and rain == 0
            and temp > 10
            and temp < 35
        )

        return {
            "advisories": advisories,
            "risk_level": risk_level,
            "good_for_spraying": good_spraying,
            "spraying_note": "Good conditions for spraying" if good_spraying else "Avoid spraying — conditions unfavorable",
            "irrigation_note": "Skip irrigation" if rain > 10 else "Irrigate as scheduled",
        }

    def _parse_weather(self, data: dict) -> dict:
        return {
            "temperature": data.get("main", {}).get("temp", 0),
            "feels_like": data.get("main", {}).get("feels_like", 0),
            "humidity": data.get("main", {}).get("humidity", 0),
            "pressure": data.get("main", {}).get("pressure", 0),
            "wind_speed": data.get("wind", {}).get("speed", 0),
            "wind_direction": data.get("wind", {}).get("deg", 0),
            "description": data.get("weather", [{}])[0].get("description", ""),
            "icon": data.get("weather", [{}])[0].get("icon", ""),
            "clouds": data.get("clouds", {}).get("all", 0),
            "rain_mm": data.get("rain", {}).get("1h", 0),
            "visibility": data.get("visibility", 10000),
            "location": data.get("name", ""),
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    def _parse_forecast(self, data: dict) -> list:
        forecasts = []
        for entry in data.get("list", []):
            forecasts.append({
                "datetime": entry.get("dt_txt", ""),
                "temperature": entry.get("main", {}).get("temp", 0),
                "humidity": entry.get("main", {}).get("humidity", 0),
                "description": entry.get("weather", [{}])[0].get("description", ""),
                "rain_mm": entry.get("rain", {}).get("3h", 0),
                "wind_speed": entry.get("wind", {}).get("speed", 0),
            })
        return forecasts

    def _get_demo_weather(self) -> dict:
        """Return demo weather data when API key is not configured."""
        return {
            "temperature": 28.5,
            "feels_like": 30.2,
            "humidity": 65,
            "pressure": 1013,
            "wind_speed": 8,
            "wind_direction": 180,
            "description": "partly cloudy",
            "icon": "02d",
            "clouds": 40,
            "rain_mm": 0,
            "visibility": 10000,
            "location": "Demo Location",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "is_demo": True,
        }

    def _get_demo_forecast(self) -> list:
        return [
            {"datetime": "Demo", "temperature": 28, "humidity": 65, "description": "partly cloudy", "rain_mm": 0, "wind_speed": 8},
            {"datetime": "Demo", "temperature": 26, "humidity": 70, "description": "light rain", "rain_mm": 5, "wind_speed": 10},
            {"datetime": "Demo", "temperature": 30, "humidity": 55, "description": "clear sky", "rain_mm": 0, "wind_speed": 6},
        ]

    async def _get_openmeteo_weather(self, latitude: float, longitude: float) -> Optional[dict]:
        """Get current weather from Open-Meteo (no API key required fallback)."""
        try:
            async with httpx.AsyncClient(timeout=15) as client:
                response = await client.get(
                    "https://api.open-meteo.com/v1/forecast",
                    params={
                        "latitude": latitude,
                        "longitude": longitude,
                        "current": "temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,rain,weather_code,pressure_msl,wind_speed_10m,wind_direction_10m,visibility",
                        "timezone": "auto"
                    }
                )
                if response.status_code == 200:
                    data = response.json()
                    current = data.get("current", {})
                    code = current.get("weather_code", 0)
                    
                    WMO_CODES = {
                        0: "clear sky",
                        1: "mainly clear",
                        2: "partly cloudy",
                        3: "overcast",
                        45: "foggy",
                        48: "depositing rime fog",
                        51: "light drizzle",
                        53: "moderate drizzle",
                        55: "dense drizzle",
                        56: "light freezing drizzle",
                        57: "dense freezing drizzle",
                        61: "light rain",
                        63: "moderate rain",
                        65: "heavy rain",
                        66: "light freezing rain",
                        67: "heavy freezing rain",
                        71: "slight snow fall",
                        73: "moderate snow fall",
                        75: "heavy snow fall",
                        77: "snow grains",
                        80: "slight rain showers",
                        81: "moderate rain showers",
                        82: "violent rain showers",
                        85: "slight snow showers",
                        86: "heavy snow showers",
                        95: "thunderstorm",
                        96: "thunderstorm with slight hail",
                        99: "thunderstorm with heavy hail"
                    }
                    description = WMO_CODES.get(code, "clear sky")
                    
                    return {
                        "temperature": current.get("temperature_2m", 25.0),
                        "feels_like": current.get("apparent_temperature", 25.0),
                        "humidity": current.get("relative_humidity_2m", 60),
                        "pressure": current.get("pressure_msl", 1013),
                        "wind_speed": current.get("wind_speed_10m", 5),
                        "wind_direction": current.get("wind_direction_10m", 0),
                        "description": description,
                        "icon": self._get_wmo_icon_code(code),
                        "clouds": 40 if code > 0 else 0,
                        "rain_mm": current.get("rain", 0),
                        "visibility": current.get("visibility", 10000),
                        "location": f"Local Farm ({latitude:.2f}, {longitude:.2f})",
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                        "is_openmeteo": True
                    }
        except Exception as e:
            logger.error(f"Open-Meteo current weather error: {e}")
        return self._get_demo_weather()

    async def _get_openmeteo_forecast(self, latitude: float, longitude: float, days: int = 5) -> Optional[list]:
        """Get forecast from Open-Meteo (no API key required fallback)."""
        try:
            async with httpx.AsyncClient(timeout=15) as client:
                response = await client.get(
                    "https://api.open-meteo.com/v1/forecast",
                    params={
                        "latitude": latitude,
                        "longitude": longitude,
                        "daily": "temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max,weather_code",
                        "timezone": "auto"
                    }
                )
                if response.status_code == 200:
                    data = response.json()
                    daily = data.get("daily", {})
                    
                    WMO_CODES = {
                        0: "clear sky", 1: "mainly clear", 2: "partly cloudy", 3: "overcast",
                        45: "foggy", 48: "rime fog", 51: "light drizzle", 53: "drizzle",
                        55: "dense drizzle", 61: "light rain", 63: "moderate rain", 65: "heavy rain",
                        80: "light showers", 81: "showers", 82: "heavy showers", 95: "thunderstorm"
                    }
                    
                    forecasts = []
                    times = daily.get("time", [])
                    temp_maxs = daily.get("temperature_2m_max", [])
                    temp_mins = daily.get("temperature_2m_min", [])
                    precips = daily.get("precipitation_sum", [])
                    winds = daily.get("wind_speed_10m_max", [])
                    codes = daily.get("weather_code", [])
                    
                    for i in range(min(len(times), days)):
                        code = codes[i] if i < len(codes) else 0
                        temp = (temp_maxs[i] + temp_mins[i]) / 2 if (i < len(temp_maxs) and i < len(temp_mins)) else 25.0
                        desc = WMO_CODES.get(code, "clear sky")
                        
                        forecasts.append({
                            "datetime": times[i] if i < len(times) else "Date",
                            "temperature": temp,
                            "humidity": 65,
                            "description": desc,
                            "rain_mm": precips[i] if i < len(precips) else 0,
                            "wind_speed": winds[i] if i < len(winds) else 5,
                        })
                    return forecasts
        except Exception as e:
            logger.error(f"Open-Meteo forecast error: {e}")
        return self._get_demo_forecast()

    def _get_wmo_icon_code(self, code: int) -> str:
        if code == 0:
            return "01d"
        elif code in [1, 2]:
            return "02d"
        elif code == 3:
            return "03d"
        elif code in [45, 48]:
            return "50d"
        elif code in [51, 53, 55, 56, 57]:
            return "09d"
        elif code in [61, 63, 65, 66, 67, 80, 81, 82]:
            return "10d"
        elif code in [71, 73, 75, 77, 85, 86]:
            return "13d"
        elif code in [95, 96, 99]:
            return "11d"
        return "02d"

    @property
    def is_ready(self) -> bool:
        return True
