"""
AgroMate Crop Advisor

Provides intelligent crop recommendations based on location,
season, soil type, and weather conditions.
"""

import logging
from datetime import datetime, timezone
from typing import Optional
from dataclasses import dataclass, field

logger = logging.getLogger(__name__)


CROP_DATABASE = {
    "Kharif": {
        "season": "June-October (Monsoon)",
        "crops": [
            {"name": "Rice", "water": "High", "soil": "Clay/Loam", "temp_range": "20-35°C"},
            {"name": "Cotton", "water": "Moderate", "soil": "Black/Clay", "temp_range": "21-35°C"},
            {"name": "Soybean", "water": "Moderate", "soil": "Loam", "temp_range": "20-30°C"},
            {"name": "Groundnut", "water": "Moderate", "soil": "Sandy Loam", "temp_range": "25-35°C"},
            {"name": "Maize", "water": "Moderate", "soil": "Loam", "temp_range": "21-30°C"},
            {"name": "Sugarcane", "water": "High", "soil": "Loam/Clay", "temp_range": "20-35°C"},
            {"name": "Bajra (Pearl Millet)", "water": "Low", "soil": "Sandy", "temp_range": "25-35°C"},
            {"name": "Jowar (Sorghum)", "water": "Low", "soil": "Black", "temp_range": "25-35°C"},
        ],
    },
    "Rabi": {
        "season": "October-March (Winter)",
        "crops": [
            {"name": "Wheat", "water": "Moderate", "soil": "Loam/Clay", "temp_range": "10-25°C"},
            {"name": "Chickpea", "water": "Low", "soil": "Loam", "temp_range": "15-25°C"},
            {"name": "Mustard", "water": "Low", "soil": "Loam/Sandy", "temp_range": "10-25°C"},
            {"name": "Potato", "water": "Moderate", "soil": "Sandy Loam", "temp_range": "15-20°C"},
            {"name": "Onion", "water": "Moderate", "soil": "Loam", "temp_range": "13-24°C"},
            {"name": "Barley", "water": "Low", "soil": "Loam", "temp_range": "12-25°C"},
            {"name": "Pea", "water": "Moderate", "soil": "Loam", "temp_range": "10-20°C"},
            {"name": "Linseed", "water": "Low", "soil": "Clay Loam", "temp_range": "15-25°C"},
        ],
    },
    "Zaid": {
        "season": "March-June (Summer)",
        "crops": [
            {"name": "Watermelon", "water": "High", "soil": "Sandy Loam", "temp_range": "25-35°C"},
            {"name": "Muskmelon", "water": "High", "soil": "Sandy Loam", "temp_range": "25-35°C"},
            {"name": "Cucumber", "water": "Moderate", "soil": "Loam", "temp_range": "20-30°C"},
            {"name": "Moong (Green Gram)", "water": "Low", "soil": "Loam", "temp_range": "25-35°C"},
            {"name": "Sunflower", "water": "Moderate", "soil": "Loam", "temp_range": "20-30°C"},
            {"name": "Bitter Gourd", "water": "Moderate", "soil": "Loam", "temp_range": "25-35°C"},
        ],
    },
}


@dataclass
class CropRecommendation:
    """A single crop recommendation."""
    crop_name: str
    suitability_score: float
    water_requirement: str
    ideal_soil: str
    temp_range: str
    season: str
    reasons: list = field(default_factory=list)
    tips: list = field(default_factory=list)


@dataclass
class CropAdvisoryResult:
    """Result of crop advisory analysis."""
    current_season: str
    recommendations: list[CropRecommendation] = field(default_factory=list)
    weather_advisory: str = ""
    soil_advisory: str = ""
    general_tips: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "current_season": self.current_season,
            "recommendations": [
                {
                    "crop_name": r.crop_name,
                    "suitability_score": round(r.suitability_score, 2),
                    "water_requirement": r.water_requirement,
                    "ideal_soil": r.ideal_soil,
                    "temp_range": r.temp_range,
                    "season": r.season,
                    "reasons": r.reasons,
                    "tips": r.tips,
                }
                for r in self.recommendations
            ],
            "weather_advisory": self.weather_advisory,
            "soil_advisory": self.soil_advisory,
            "general_tips": self.general_tips,
        }


class CropAdvisor:
    """
    Intelligent crop recommendation engine.

    Considers season, temperature, soil type, water availability,
    and location to suggest suitable crops.
    """

    def __init__(self):
        self._crop_db = CROP_DATABASE

    async def initialize(self):
        logger.info("Crop advisor initialized")

    def get_current_season(self, month: int = None) -> str:
        """Determine the current Indian agricultural season."""
        if month is None:
            month = datetime.now(timezone.utc).month

        if 6 <= month <= 10:
            return "Kharif"
        elif month >= 10 or month <= 3:
            return "Rabi"
        else:
            return "Zaid"

    async def get_recommendations(
        self,
        temperature: float = None,
        soil_type: str = None,
        water_availability: str = None,
        latitude: float = None,
        longitude: float = None,
        month: int = None,
    ) -> CropAdvisoryResult:
        """
        Get crop recommendations based on conditions.

        Args:
            temperature: Current temperature in Celsius
            soil_type: Soil type (Sandy, Loam, Clay, Black, etc.)
            water_availability: Water availability (Low, Moderate, High)
            latitude: Location latitude
            longitude: Location longitude
            month: Month number (1-12)
        """
        season = self.get_current_season(month)
        season_data = self._crop_db.get(season, {})
        crops = season_data.get("crops", [])

        recommendations = []

        for crop in crops:
            score = 0.5  # Base score
            reasons = []
            tips = []

            # Temperature match
            if temperature is not None:
                temp_range = crop.get("temp_range", "")
                try:
                    low, high = [
                        float(t.replace("°C", "").strip())
                        for t in temp_range.split("-")
                    ]
                    if low <= temperature <= high:
                        score += 0.2
                        reasons.append(f"Temperature {temperature}°C is ideal ({temp_range})")
                    elif abs(temperature - low) <= 5 or abs(temperature - high) <= 5:
                        score += 0.1
                        reasons.append(f"Temperature {temperature}°C is near ideal range ({temp_range})")
                    else:
                        score -= 0.2
                        reasons.append(f"Temperature {temperature}°C is outside ideal range ({temp_range})")
                except (ValueError, TypeError):
                    pass

            # Soil match
            if soil_type:
                ideal_soil = crop.get("soil", "").lower()
                if soil_type.lower() in ideal_soil:
                    score += 0.15
                    reasons.append(f"{soil_type} soil is suitable")
                else:
                    tips.append(f"Consider soil amendment — ideal soil: {crop['soil']}")

            # Water match
            if water_availability:
                crop_water = crop.get("water", "Moderate")
                water_levels = {"Low": 1, "Moderate": 2, "High": 3}
                available = water_levels.get(water_availability, 2)
                required = water_levels.get(crop_water, 2)

                if available >= required:
                    score += 0.15
                    reasons.append(f"Water availability ({water_availability}) meets requirement ({crop_water})")
                else:
                    score -= 0.1
                    tips.append(f"Needs {crop_water} water — consider drip irrigation")

            recommendations.append(CropRecommendation(
                crop_name=crop["name"],
                suitability_score=max(0, min(1, score)),
                water_requirement=crop.get("water", "Moderate"),
                ideal_soil=crop.get("soil", "Loam"),
                temp_range=crop.get("temp_range", ""),
                season=season,
                reasons=reasons,
                tips=tips,
            ))

        # Sort by suitability score
        recommendations.sort(key=lambda x: x.suitability_score, reverse=True)

        # General tips
        general_tips = [
            "Test your soil pH and nutrient levels before planting",
            "Ensure proper drainage to prevent waterlogging",
            "Consider crop rotation to maintain soil health",
            "Use mulching to conserve moisture and suppress weeds",
            "Plan irrigation schedule based on crop growth stage",
        ]

        return CropAdvisoryResult(
            current_season=season,
            recommendations=recommendations[:8],
            weather_advisory=self._get_weather_advisory(season, temperature),
            soil_advisory=self._get_soil_advisory(soil_type),
            general_tips=general_tips,
        )

    def _get_weather_advisory(self, season: str, temperature: float = None) -> str:
        advisories = {
            "Kharif": "Monsoon season — ensure proper drainage. Watch for fungal diseases due to high humidity.",
            "Rabi": "Winter season — protect crops from frost. Ensure adequate irrigation as rainfall is low.",
            "Zaid": "Summer season — ensure adequate irrigation. Use shade nets for sensitive crops.",
        }
        advisory = advisories.get(season, "")
        if temperature and temperature > 40:
            advisory += " HEAT ALERT: Extreme temperatures — use shade nets and increase irrigation frequency."
        elif temperature and temperature < 5:
            advisory += " FROST ALERT: Protect crops with mulch or frost covers."
        return advisory

    def _get_soil_advisory(self, soil_type: str = None) -> str:
        if not soil_type:
            return "Get your soil tested for accurate recommendations."
        advisories = {
            "sandy": "Sandy soil drains fast — add organic matter and use drip irrigation.",
            "clay": "Clay soil retains water — ensure good drainage and add gypsum to improve structure.",
            "loam": "Loam soil is ideal for most crops — maintain with regular organic matter addition.",
            "black": "Black soil is rich in nutrients — ideal for cotton. Avoid waterlogging.",
        }
        return advisories.get(soil_type.lower(), f"Manage {soil_type} soil with appropriate amendments.")

    @property
    def is_ready(self) -> bool:
        return True
