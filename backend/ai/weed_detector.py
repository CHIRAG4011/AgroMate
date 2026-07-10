"""
AgroMate Weed Detector

Identifies weeds in crop fields using image analysis to distinguish
between crop plants and unwanted weed species.
"""

import logging
import time
from typing import Optional
from dataclasses import dataclass, field

import numpy as np

from ..utils.image_utils import load_image, preprocess_for_model
from ..config import get_settings

logger = logging.getLogger(__name__)

WEED_CLASSES = [
    "Black Nightshade", "Chinee Apple", "Lantana", "Parthenium",
    "Prickly Acacia", "Rubber Vine", "Siam Weed", "Snake Weed",
    "Nutgrass", "Bermuda Grass", "Crabgrass", "Pigweed",
]

WEED_MANAGEMENT = {
    "Parthenium": {
        "method": "Manual removal before flowering. Biological control with Zygogramma beetle.",
        "herbicide": "Glyphosate (non-selective) or Metribuzin (selective)",
        "organic": "Mexican beetle release, competitive cropping with Cassia sericea",
    },
    "Nutgrass": {
        "method": "Deep plowing to expose tubers. Solarization.",
        "herbicide": "Halosulfuron or Imazethapyr",
        "organic": "Repeated cultivation, solarization with plastic mulch",
    },
}


@dataclass
class WeedDetectionResult:
    """Result of weed detection analysis."""
    weed_detected: bool
    weed_name: str = ""
    confidence: float = 0.0
    coverage_percent: float = 0.0
    management: str = ""
    herbicide: str = ""
    organic_control: str = ""
    processing_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "weed_detected": bool(self.weed_detected),
            "weed_name": self.weed_name,
            "confidence": float(round(self.confidence, 4)),
            "coverage_percent": float(round(self.coverage_percent, 1)),
            "management": self.management,
            "herbicide": self.herbicide,
            "organic_control": self.organic_control,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class WeedDetector:
    """Weed detection using image classification and segmentation."""

    def __init__(self):
        self.settings = get_settings()
        self._model = None
        self._model_loaded = False

    async def initialize(self):
        logger.info("Weed detector initialized (heuristic mode)")

    async def detect(self, image_data: bytes, crop_name: str = None) -> Optional[WeedDetectionResult]:
        """Detect weeds in a field/crop image."""
        start_time = time.time()

        try:
            image = load_image(image_data, target_size=(224, 224))
            if image is None:
                return None

            arr = np.array(image)

            # Green vegetation analysis
            r, g, b = arr[:, :, 0].astype(float), arr[:, :, 1].astype(float), arr[:, :, 2].astype(float)

            # Excess Green Index (ExG) = 2G - R - B
            exg = 2 * g - r - b
            vegetation_mask = exg > 20

            vegetation_percent = vegetation_mask.sum() / vegetation_mask.size * 100

            # Look for irregular vegetation patterns (potential weeds)
            # Weeds often have different shade/texture than crops
            if vegetation_percent > 60:
                # High vegetation — analyze color variance
                green_variance = np.std(g[vegetation_mask])

                if green_variance > 40:
                    result = WeedDetectionResult(
                        weed_detected=True,
                        weed_name="Mixed vegetation (potential weed presence)",
                        confidence=min(0.6, green_variance / 80),
                        coverage_percent=vegetation_percent * 0.3,
                        management="Manual weeding recommended. Inspect field for weed species.",
                        organic_control="Mulching, hand weeding, or cover cropping",
                    )
                else:
                    result = WeedDetectionResult(
                        weed_detected=False,
                        weed_name="No significant weed presence detected",
                        confidence=0.0,
                        coverage_percent=0.0,
                    )
            else:
                result = WeedDetectionResult(
                    weed_detected=False,
                    weed_name="Insufficient vegetation for analysis",
                    confidence=0.0,
                    coverage_percent=vegetation_percent,
                )

            result.processing_time_ms = (time.time() - start_time) * 1000
            return result

        except Exception as e:
            logger.error(f"Weed detection failed: {e}")
            return None

    @property
    def is_ready(self) -> bool:
        return True
