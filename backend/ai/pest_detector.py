"""
AgroMate Pest Detector

AI-powered pest detection using object detection models to identify
common agricultural pests with bounding boxes and IPM recommendations.
"""

import logging
import time
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, field

import numpy as np

from ..utils.image_utils import load_image, preprocess_for_model
from ..config import get_settings

logger = logging.getLogger(__name__)

PEST_CLASSES = [
    "Aphid", "Whitefly", "Caterpillar", "Beetle", "Mite",
    "Thrips", "Leafhopper", "Mealybug", "Fruit Fly", "Armyworm",
    "Bollworm", "Stem Borer", "Grasshopper", "Snail/Slug", "Nematode",
]

PEST_TREATMENTS = {
    "Aphid": {
        "chemical": "Imidacloprid or acetamiprid spray",
        "organic": "Neem oil, insecticidal soap, ladybug release, water jet spray",
        "prevention": "Encourage natural predators, avoid excess nitrogen fertilizer",
    },
    "Whitefly": {
        "chemical": "Spiromesifen or pyriproxyfen",
        "organic": "Yellow sticky traps, neem oil spray, insecticidal soap",
        "prevention": "Reflective mulches, remove infested leaves early",
    },
    "Caterpillar": {
        "chemical": "Chlorantraniliprole or spinosad",
        "organic": "Bacillus thuringiensis (Bt) spray, handpicking, bird encouragement",
        "prevention": "Pheromone traps, crop rotation, maintain garden hygiene",
    },
    "Beetle": {
        "chemical": "Carbaryl or bifenthrin",
        "organic": "Handpicking, neem oil, diatomaceous earth",
        "prevention": "Row covers, crop rotation, trap crops",
    },
    "Mite": {
        "chemical": "Abamectin or spiromesifen",
        "organic": "Predatory mites release, water spraying, sulfur dust",
        "prevention": "Maintain humidity, avoid dusty conditions, avoid broad-spectrum pesticides",
    },
    "Thrips": {
        "chemical": "Spinosad or fipronil",
        "organic": "Blue sticky traps, neem oil, predatory mites",
        "prevention": "Remove weeds, avoid nearby thrips-host plants",
    },
    "Leafhopper": {
        "chemical": "Thiamethoxam or imidacloprid",
        "organic": "Kaolin clay spray, neem oil, row covers",
        "prevention": "Remove weeds, use reflective mulch",
    },
    "Mealybug": {
        "chemical": "Profenofos or chlorpyrifos",
        "organic": "Isopropyl alcohol swab, neem oil, lacewing release",
        "prevention": "Quarantine new plants, avoid overwatering",
    },
    "Fruit Fly": {
        "chemical": "Malathion bait spray",
        "organic": "Fruit fly traps (vinegar + soap), bag fruits, sanitation",
        "prevention": "Remove fallen fruit, early harvest, pheromone traps",
    },
    "Armyworm": {
        "chemical": "Emamectin benzoate or chlorantraniliprole",
        "organic": "Bt spray, bird perches, Trichogramma release",
        "prevention": "Pheromone traps, early detection scouting",
    },
}


@dataclass
class PestDetectionResult:
    """Result of pest detection analysis."""
    pest_name: str
    confidence: float
    count: int = 1
    bounding_boxes: list = field(default_factory=list)
    chemical_treatment: str = ""
    organic_treatment: str = ""
    prevention: str = ""
    severity: str = "mild"
    top_5: list = field(default_factory=list)
    model_used: str = "local"
    processing_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "pest_name": self.pest_name,
            "confidence": float(round(self.confidence, 4)),
            "count": int(self.count),
            "bounding_boxes": self.bounding_boxes,
            "chemical_treatment": self.chemical_treatment,
            "organic_treatment": self.organic_treatment,
            "prevention": self.prevention,
            "severity": self.severity,
            "top_5": self.top_5,
            "model_used": self.model_used,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class PestDetector:
    """Pest detection engine using object detection models."""

    def __init__(self):
        self.settings = get_settings()
        self._model = None
        self._model_loaded = False
        self._class_labels = PEST_CLASSES

    async def initialize(self):
        """Load the pest detection model."""
        model_path = Path(self.settings.pest_model_path)
        if model_path.exists():
            try:
                import onnxruntime as ort
                self._model = ort.InferenceSession(
                    str(model_path), providers=["CPUExecutionProvider"]
                )
                self._model_loaded = True
                logger.info(f"Pest detection model loaded: {model_path.name}")
            except Exception as e:
                logger.warning(f"Failed to load pest model: {e}")
        else:
            logger.info("Pest model not found. Using heuristic detection.")

    async def detect(self, image_data: bytes) -> Optional[PestDetectionResult]:
        """Detect pests in an image."""
        start_time = time.time()

        if self._model_loaded:
            result = await self._predict_local(image_data)
        else:
            result = await self._predict_heuristic(image_data)

        if result:
            result.processing_time_ms = (time.time() - start_time) * 1000
            self._enrich_result(result)

        return result

    async def _predict_local(self, image_data: bytes) -> Optional[PestDetectionResult]:
        """Run local model inference."""
        try:
            image = load_image(image_data)
            if image is None:
                return None

            input_meta = self._model.get_inputs()[0]
            input_name = input_meta.name
            expected_shape = input_meta.shape
            target_h = expected_shape[2] if (len(expected_shape) == 4 and expected_shape[1] == 3) else 224
            target_w = expected_shape[3] if (len(expected_shape) == 4 and expected_shape[1] == 3) else 224

            input_array = preprocess_for_model(image, input_size=(target_w, target_h))
            if input_array is None:
                return None

            if len(expected_shape) == 4 and expected_shape[1] == 3 and input_array.shape[-1] == 3:
                input_array = np.transpose(input_array, (0, 3, 1, 2)).astype(np.float32)

            outputs = self._model.run(None, {input_name: input_array})

            # Handle classification output [1, num_classes]
            if len(outputs) == 1:
                probs = outputs[0][0]
                if np.min(probs) < 0 or np.sum(probs) > 1.5:
                    exp_vals = np.exp(probs - np.max(probs))
                    probs = exp_vals / np.sum(exp_vals)
                best_idx = int(np.argmax(probs))
                best_conf = float(probs[best_idx])
                pest_name = self._class_labels[best_idx] if best_idx < len(self._class_labels) else "Unknown"
                return PestDetectionResult(
                    pest_name=pest_name if best_conf >= 0.4 else "No pests detected",
                    confidence=best_conf,
                    count=1 if best_conf >= 0.4 else 0,
                    severity="moderate" if best_conf >= 0.6 else ("mild" if best_conf >= 0.4 else "none"),
                    model_used="local_onnx",
                )

            # Parse detection output (format depends on model architecture)
            # Standard: [boxes, scores, classes, num_detections]
            if len(outputs) >= 3:
                boxes = outputs[0][0]
                scores = outputs[1][0]
                classes = outputs[2][0].astype(int)

                # Filter by confidence threshold
                valid = scores > self.settings.ai_confidence_threshold
                if not valid.any():
                    return PestDetectionResult(
                        pest_name="No pests detected",
                        confidence=0.0,
                        count=0,
                        model_used="local_onnx",
                    )

                valid_scores = scores[valid]
                valid_classes = classes[valid]
                valid_boxes = boxes[valid]

                # Get most common pest
                from collections import Counter
                pest_counts = Counter(valid_classes.tolist())
                most_common_class = pest_counts.most_common(1)[0][0]

                pest_name = PEST_CLASSES[most_common_class] if most_common_class < len(PEST_CLASSES) else f"Pest_{most_common_class}"

                return PestDetectionResult(
                    pest_name=pest_name,
                    confidence=float(valid_scores.max()),
                    count=len(valid_scores),
                    bounding_boxes=valid_boxes.tolist(),
                    model_used="local_onnx",
                )

        except Exception as e:
            logger.error(f"Local pest detection failed: {e}")
        return None

    async def _predict_heuristic(self, image_data: bytes) -> Optional[PestDetectionResult]:
        """Basic heuristic pest detection using color/texture analysis."""
        try:
            image = load_image(image_data, target_size=(224, 224))
            if image is None:
                return None

            arr = np.array(image)

            # Analyze for small dark spots (potential pest bodies)
            gray = np.mean(arr, axis=2)
            dark_pixels = (gray < 60).sum()
            total_pixels = gray.size
            dark_ratio = dark_pixels / total_pixels

            if dark_ratio > 0.05:
                return PestDetectionResult(
                    pest_name="Possible pest presence (dark spots detected)",
                    confidence=min(dark_ratio * 5, 0.6),
                    severity="mild",
                    model_used="heuristic",
                )

            return PestDetectionResult(
                pest_name="No pests detected",
                confidence=0.0,
                count=0,
                model_used="heuristic",
            )

        except Exception as e:
            logger.error(f"Heuristic pest detection failed: {e}")
            return None

    def _enrich_result(self, result: PestDetectionResult):
        """Add treatment recommendations to detection result."""
        treatments = PEST_TREATMENTS.get(result.pest_name, {})
        result.chemical_treatment = treatments.get("chemical", "Consult local extension office")
        result.organic_treatment = treatments.get("organic", "Neem oil spray or insecticidal soap")
        result.prevention = treatments.get("prevention", "Regular scouting and garden hygiene")

        if result.count > 10:
            result.severity = "severe"
        elif result.count > 5:
            result.severity = "moderate"

    @property
    def is_ready(self) -> bool:
        return True
