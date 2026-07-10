"""
AgroMate Growth Stage Estimator

Estimates plant growth stages from visual characteristics
to provide stage-appropriate care recommendations.
"""

import logging
import time
from typing import Optional
from dataclasses import dataclass

import numpy as np

from ..utils.image_utils import load_image

logger = logging.getLogger(__name__)

GROWTH_STAGES = [
    {"stage": "Germination/Seedling", "description": "Seeds sprouting, first leaves emerging", "duration": "1-2 weeks"},
    {"stage": "Vegetative", "description": "Rapid leaf and stem growth", "duration": "3-6 weeks"},
    {"stage": "Flowering", "description": "Flowers developing, pollination period", "duration": "2-4 weeks"},
    {"stage": "Fruiting", "description": "Fruit/grain development", "duration": "3-6 weeks"},
    {"stage": "Maturity/Harvest", "description": "Crop ready for harvest", "duration": "1-2 weeks"},
]


@dataclass
class GrowthEstimationResult:
    """Result of growth stage estimation."""
    estimated_stage: str
    stage_index: int
    confidence: float
    description: str
    care_tips: list
    days_to_harvest: str
    processing_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "estimated_stage": self.estimated_stage,
            "stage_index": int(self.stage_index),
            "confidence": float(round(self.confidence, 4)),
            "description": self.description,
            "care_tips": self.care_tips,
            "days_to_harvest": self.days_to_harvest,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class GrowthEstimator:
    """Estimates plant growth stage from images."""

    def __init__(self):
        pass

    async def initialize(self):
        logger.info("Growth estimator initialized")

    async def estimate(self, image_data: bytes, plant_name: str = None) -> Optional[GrowthEstimationResult]:
        """Estimate growth stage from a plant image."""
        start_time = time.time()

        try:
            image = load_image(image_data, target_size=(224, 224))
            if image is None:
                return None

            arr = np.array(image)
            g = arr[:, :, 1].astype(float)

            # Vegetation coverage
            vegetation_mask = g > 100
            coverage = vegetation_mask.sum() / vegetation_mask.size

            # Color analysis for stage estimation
            green_intensity = g.mean()
            color_variance = np.std(arr, axis=(0, 1)).mean()

            # Simple heuristic based on green coverage and variance
            if coverage < 0.2:
                stage_idx = 0
                confidence = 0.5
            elif coverage < 0.5 and green_intensity > 100:
                stage_idx = 1
                confidence = 0.55
            elif color_variance > 50:
                stage_idx = 2
                confidence = 0.45
            elif coverage > 0.6:
                stage_idx = 3
                confidence = 0.5
            else:
                stage_idx = 4
                confidence = 0.4

            stage = GROWTH_STAGES[stage_idx]

            care_tips = self._get_care_tips(stage_idx, plant_name)
            harvest_estimate = self._estimate_harvest_time(stage_idx)

            result = GrowthEstimationResult(
                estimated_stage=stage["stage"],
                stage_index=stage_idx,
                confidence=confidence,
                description=stage["description"],
                care_tips=care_tips,
                days_to_harvest=harvest_estimate,
            )
            result.processing_time_ms = (time.time() - start_time) * 1000
            return result

        except Exception as e:
            logger.error(f"Growth estimation failed: {e}")
            return None

    def _get_care_tips(self, stage_idx: int, plant_name: str = None) -> list:
        tips_by_stage = {
            0: [
                "Keep soil consistently moist but not waterlogged",
                "Protect seedlings from direct harsh sunlight",
                "Watch for damping-off disease",
                "Thin seedlings if overcrowded",
            ],
            1: [
                "Apply nitrogen-rich fertilizer for leaf growth",
                "Ensure adequate spacing between plants",
                "Start pest monitoring",
                "Maintain consistent watering schedule",
            ],
            2: [
                "Reduce nitrogen, increase phosphorus and potassium",
                "Ensure pollinator access (avoid pesticides during flowering)",
                "Monitor for flower drop due to heat stress",
                "Provide support for tall/vining plants",
            ],
            3: [
                "Maintain consistent watering for fruit development",
                "Apply potassium-rich fertilizer for fruit quality",
                "Watch for fruit pests and diseases",
                "Provide support for heavy fruit clusters",
            ],
            4: [
                "Reduce watering gradually",
                "Check maturity indicators before harvest",
                "Harvest in cool morning hours for best quality",
                "Plan storage and post-harvest handling",
            ],
        }
        return tips_by_stage.get(stage_idx, [])

    def _estimate_harvest_time(self, stage_idx: int) -> str:
        estimates = {
            0: "8-12 weeks",
            1: "6-10 weeks",
            2: "4-8 weeks",
            3: "2-4 weeks",
            4: "Ready for harvest",
        }
        return estimates.get(stage_idx, "Unknown")

    @property
    def is_ready(self) -> bool:
        return True
