"""
AgroMate Nutrient Analyzer

Analyzes plant leaf images for nutrient deficiency symptoms using
color distribution analysis and pattern matching.
"""

import logging
import time
from typing import Optional
from dataclasses import dataclass, field

import numpy as np
from PIL import Image

from ..utils.image_utils import load_image
from ..database.seed import NUTRIENT_DEFICIENCIES

logger = logging.getLogger(__name__)


@dataclass
class NutrientAnalysisResult:
    """Result of nutrient deficiency analysis."""
    deficiency_detected: bool
    primary_deficiency: str = ""
    confidence: float = 0.0
    symptoms_matched: str = ""
    treatment: str = ""
    organic_option: str = ""
    all_deficiencies: list = field(default_factory=list)
    color_analysis: dict = field(default_factory=dict)
    processing_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "deficiency_detected": bool(self.deficiency_detected),
            "primary_deficiency": self.primary_deficiency,
            "confidence": float(round(self.confidence, 4)),
            "symptoms_matched": self.symptoms_matched,
            "treatment": self.treatment,
            "organic_option": self.organic_option,
            "all_deficiencies": self.all_deficiencies,
            "color_analysis": self.color_analysis,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class NutrientAnalyzer:
    """
    Plant nutrient deficiency analyzer.

    Uses color analysis of leaf images to detect common nutrient
    deficiencies based on visual symptoms.
    """

    def __init__(self):
        self._deficiency_db = NUTRIENT_DEFICIENCIES

    async def initialize(self):
        """Initialize the analyzer."""
        logger.info("Nutrient analyzer initialized")

    async def analyze(self, image_data: bytes) -> Optional[NutrientAnalysisResult]:
        """
        Analyze a leaf image for nutrient deficiencies.

        Uses HSV color space analysis to detect:
        - Yellowing (chlorosis) → Nitrogen, Iron, Magnesium
        - Purpling → Phosphorus
        - Brown edges → Potassium
        - Tip burn → Calcium
        """
        start_time = time.time()

        try:
            image = load_image(image_data, target_size=(224, 224))
            if image is None:
                return None

            arr = np.array(image)

            # Convert to HSV for better color analysis
            from PIL import ImageDraw
            hsv_image = image.convert("HSV")
            hsv_arr = np.array(hsv_image)

            # Color channel analysis
            h = hsv_arr[:, :, 0].astype(float)  # Hue (0-255 in PIL)
            s = hsv_arr[:, :, 1].astype(float)  # Saturation
            v = hsv_arr[:, :, 2].astype(float)  # Value/Brightness

            r = arr[:, :, 0].astype(float)
            g = arr[:, :, 1].astype(float)
            b = arr[:, :, 2].astype(float)

            color_analysis = {
                "mean_rgb": [float(r.mean()), float(g.mean()), float(b.mean())],
                "mean_hsv": [float(h.mean()), float(s.mean()), float(v.mean())],
                "green_dominance": float(g.mean() / (r.mean() + 1)),
                "yellow_index": float((r.mean() + g.mean()) / (2 * (b.mean() + 1))),
                "brown_index": float(r.mean() / (g.mean() + 1)),
            }

            deficiencies = []

            # --- Nitrogen deficiency: overall yellowing, pale leaves ---
            green_ratio = g.mean() / (r.mean() + 1)
            yellow_pixels = np.sum((r > 150) & (g > 150) & (b < 100))
            yellow_ratio = yellow_pixels / (arr.shape[0] * arr.shape[1])

            if green_ratio < 0.9 or yellow_ratio > 0.3:
                conf = min(0.8, yellow_ratio * 2 + (1 - green_ratio))
                deficiencies.append({
                    "nutrient": "Nitrogen (N)",
                    "confidence": conf,
                    "indicator": "Overall yellowing/pale green color",
                })

            # --- Phosphorus deficiency: purple/reddish tint ---
            purple_pixels = np.sum((r > 100) & (b > 80) & (g < 100))
            purple_ratio = purple_pixels / (arr.shape[0] * arr.shape[1])

            if purple_ratio > 0.15:
                deficiencies.append({
                    "nutrient": "Phosphorus (P)",
                    "confidence": min(0.75, purple_ratio * 3),
                    "indicator": "Purple/reddish coloration detected",
                })

            # --- Potassium deficiency: brown leaf edges ---
            # Check border pixels
            edge_width = 15
            edges = np.concatenate([
                arr[:edge_width, :, :].reshape(-1, 3),
                arr[-edge_width:, :, :].reshape(-1, 3),
                arr[:, :edge_width, :].reshape(-1, 3),
                arr[:, -edge_width:, :].reshape(-1, 3),
            ])
            edge_brown = np.sum(
                (edges[:, 0] > 120) & (edges[:, 1] < 100) & (edges[:, 2] < 80)
            )
            edge_brown_ratio = edge_brown / len(edges)

            if edge_brown_ratio > 0.2:
                deficiencies.append({
                    "nutrient": "Potassium (K)",
                    "confidence": min(0.7, edge_brown_ratio * 2),
                    "indicator": "Brown leaf edges (marginal scorch)",
                })

            # --- Iron deficiency: interveinal chlorosis on young leaves ---
            # High contrast between veins (green) and interveinal areas (yellow)
            g_std = np.std(g)
            if g_std > 40 and green_ratio < 1.0:
                deficiencies.append({
                    "nutrient": "Iron (Fe)",
                    "confidence": min(0.6, g_std / 80),
                    "indicator": "Interveinal chlorosis pattern detected",
                })

            # --- Magnesium deficiency: interveinal chlorosis on older leaves ---
            if g_std > 35 and yellow_ratio > 0.2:
                deficiencies.append({
                    "nutrient": "Magnesium (Mg)",
                    "confidence": min(0.55, yellow_ratio + g_std / 100),
                    "indicator": "Interveinal yellowing detected",
                })

            # Sort by confidence
            deficiencies.sort(key=lambda x: x["confidence"], reverse=True)

            # Enrich with treatment info
            for d in deficiencies:
                for ref in self._deficiency_db:
                    if ref["nutrient"] == d["nutrient"]:
                        d["symptoms"] = ref["symptoms"]
                        d["treatment"] = ref["treatment"]
                        d["organic_option"] = ref["organic_option"]
                        break

            if deficiencies:
                primary = deficiencies[0]
                result = NutrientAnalysisResult(
                    deficiency_detected=True,
                    primary_deficiency=primary["nutrient"],
                    confidence=primary["confidence"],
                    symptoms_matched=primary.get("indicator", ""),
                    treatment=primary.get("treatment", ""),
                    organic_option=primary.get("organic_option", ""),
                    all_deficiencies=deficiencies,
                    color_analysis=color_analysis,
                )
            else:
                result = NutrientAnalysisResult(
                    deficiency_detected=False,
                    primary_deficiency="None - plant appears healthy",
                    confidence=0.0,
                    color_analysis=color_analysis,
                )

            result.processing_time_ms = (time.time() - start_time) * 1000
            return result

        except Exception as e:
            logger.error(f"Nutrient analysis failed: {e}")
            return None

    @property
    def is_ready(self) -> bool:
        return True
