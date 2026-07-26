"""
AgroMate Plant Identifier

AI-powered plant species identification using ONNX models with
fallback to Plant.id and Pl@ntNet APIs for enhanced accuracy.
"""

import logging
import time
from pathlib import Path
from typing import Optional
from dataclasses import dataclass

import numpy as np

from ..utils.image_utils import load_image, preprocess_for_model, enhance_plant_image
from ..config import get_settings

logger = logging.getLogger(__name__)

# PlantVillage class labels (subset — full model has 38 classes)
PLANT_CLASSES = [
    "Apple", "Blueberry", "Cherry", "Corn", "Grape",
    "Orange", "Peach", "Pepper", "Potato", "Raspberry",
    "Rice", "Soybean", "Squash", "Strawberry", "Sugarcane",
    "Tea", "Tomato", "Wheat", "Banana", "Mango",
    "Cotton", "Groundnut", "Onion", "Chili", "Mustard",
    "Cumin", "Turmeric", "Chickpea", "Lentil", "Pigeon Pea",
]


def get_common_name_by_scientific(scientific: str) -> str:
    if not scientific:
        return ""
    
    sci_clean = scientific.lower().strip()
    
    # 1. Check local seed database
    try:
        from ..database.seed import COMMON_PLANTS
        for plant in COMMON_PLANTS:
            if plant["scientific"].lower() in sci_clean or sci_clean in plant["scientific"].lower():
                return plant["name"]
    except Exception:
        pass
            
    # 2. Fallback dictionary for common plants, houseplants, and other groups
    scientific_to_common = {
        "epipremnum aureum": "Money Plant (Golden Pothos)",
        "pothos": "Money Plant",
        "epipremnum": "Money Plant",
        "homophron": "Fungal Mushroom / Plant",
        "clitoria ternatea": "Butterfly Pea (Aparajita)",
        "monstera deliciosa": "Monstera (Split-leaf Philodendron)",
        "sansevieria trifasciata": "Snake Plant",
        "dracaena trifasciata": "Snake Plant",
        "spathiphyllum": "Peace Lily",
        "chlorophytum comosum": "Spider Plant",
        "ficus elastica": "Rubber Plant",
        "ficus lyrata": "Fiddle Leaf Fig",
        "calathea": "Calathea (Prayer Plant)",
        "agaricus": "Mushroom",
        "rosa": "Rose",
        "hibiscus rosa-sinensis": "Hibiscus",
        "aloe vera": "Aloe Vera",
        "ocimum tenuiflorum": "Tulsi (Holy Basil)",
        "azadirachta indica": "Neem",
        "coriandrum sativum": "Coriander (Dhaniya)",
        "mentha": "Mint (Pudina)",
        "musa": "Banana",
        "gossypium": "Cotton",
        "citrus": "Citrus Plant",
    }
    
    for key, val in scientific_to_common.items():
        if key in sci_clean:
            return val
            
    return scientific.title()


@dataclass
class PlantDetectionResult:
    """Result of plant identification."""
    plant_name: str
    confidence: float
    scientific_name: str = ""
    family: str = ""
    common_names: list = None
    top_5: list = None
    model_used: str = "local"
    processing_time_ms: float = 0.0

    def __post_init__(self):
        if self.common_names is None:
            self.common_names = []
        if self.top_5 is None:
            self.top_5 = []
            
        # Try to resolve a cleaner common name if plant_name is a scientific duplicate or generic
        if self.scientific_name and (not self.plant_name or self.plant_name == "Unknown" or self.plant_name == self.scientific_name):
            resolved = get_common_name_by_scientific(self.scientific_name)
            if resolved:
                self.plant_name = resolved

    def to_dict(self) -> dict:
        return {
            "plant_name": self.plant_name,
            "confidence": float(round(self.confidence, 4)),
            "scientific_name": self.scientific_name,
            "family": self.family,
            "common_names": self.common_names,
            "top_5": self.top_5,
            "model_used": self.model_used,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class PlantDetector:
    """
    Plant species identification engine.

    Uses a local ONNX model for fast inference, with optional
    fallback to external APIs (Plant.id, Pl@ntNet) for higher accuracy.
    """

    def __init__(self):
        self.settings = get_settings()
        self._model = None
        self._model_loaded = False
        self._class_labels = PLANT_CLASSES

    async def initialize(self):
        """Load the plant classification model."""
        model_path = Path(self.settings.plant_model_path)

        if model_path.exists():
            try:
                import onnxruntime as ort

                self._model = ort.InferenceSession(
                    str(model_path),
                    providers=["CPUExecutionProvider"],
                )
                self._model_loaded = True
                logger.info(f"Plant detection model loaded: {model_path.name}")
            except Exception as e:
                logger.warning(f"Failed to load plant model: {e}")
                logger.info("Will use API-based detection as fallback")
        else:
            logger.info(
                f"Plant model not found at {model_path}. "
                "Using API-based detection."
            )

    async def identify(
        self,
        image_data: bytes,
        use_api_fallback: bool = True,
    ) -> Optional[PlantDetectionResult]:
        """
        Identify a plant from image data.

        Args:
            image_data: Raw image bytes (JPEG/PNG)
            use_api_fallback: Whether to use API if local model confidence is low

        Returns:
            PlantDetectionResult or None on failure
        """
        start_time = time.time()

        # Try local model first
        if self._model_loaded:
            result = await self._predict_local(image_data)
            if result and result.confidence >= self.settings.ai_confidence_threshold:
                result.processing_time_ms = (time.time() - start_time) * 1000
                return result

            logger.info(
                f"Local model confidence too low ({result.confidence:.2f}), "
                "trying API fallback..."
            )

        # Fallback to API
        if use_api_fallback:
            result = await self._predict_api(image_data)
            if result:
                result.processing_time_ms = (time.time() - start_time) * 1000
                return result

        return None

    async def _predict_local(self, image_data: bytes) -> Optional[PlantDetectionResult]:
        """Run local ONNX model inference."""
        try:
            image = load_image(image_data)
            if image is None:
                return None

            # Enhance for better recognition
            image = enhance_plant_image(image)

            # Preprocess
            input_array = preprocess_for_model(image, input_size=(224, 224))
            if input_array is None:
                return None

            # Run inference
            input_name = self._model.get_inputs()[0].name
            output = self._model.run(None, {input_name: input_array})
            probabilities = output[0][0]

            # Apply softmax if needed
            if np.min(probabilities) < 0 or np.sum(probabilities) > 1.5:
                exp_vals = np.exp(probabilities - np.max(probabilities))
                probabilities = exp_vals / np.sum(exp_vals)

            # Get top 5 predictions
            top_indices = np.argsort(probabilities)[::-1][:5]
            top_5 = [
                {
                    "name": self._class_labels[i] if i < len(self._class_labels) else f"Class_{i}",
                    "confidence": float(probabilities[i]),
                }
                for i in top_indices
            ]

            best_idx = top_indices[0]
            best_name = self._class_labels[best_idx] if best_idx < len(self._class_labels) else f"Class_{best_idx}"

            # Look up scientific name from seed database
            sc_name = ""
            try:
                from ..database.seed import COMMON_PLANTS
                for plant in COMMON_PLANTS:
                    if plant["name"].lower() == best_name.lower():
                        sc_name = plant["scientific"]
                        break
            except Exception:
                pass

            return PlantDetectionResult(
                plant_name=best_name,
                confidence=float(probabilities[best_idx]),
                scientific_name=sc_name,
                top_5=top_5,
                model_used="local_onnx",
            )

        except Exception as e:
            logger.error(f"Local plant detection failed: {e}")
            return None

    async def _predict_api(self, image_data: bytes) -> Optional[PlantDetectionResult]:
        """Use external API for plant identification."""
        # Try Plant.id API
        if self.settings.plant_id_api_key:
            result = await self._call_plant_id(image_data)
            if result:
                return result

        # Try Pl@ntNet API
        if self.settings.plantnet_api_key:
            result = await self._call_plantnet(image_data)
            if result:
                return result

        logger.warning("No API keys configured for plant identification")
        return None

    async def _call_plant_id(self, image_data: bytes) -> Optional[PlantDetectionResult]:
        """Call Plant.id API for identification."""
        try:
            import httpx
            import base64

            b64_image = base64.b64encode(image_data).decode("utf-8")

            async with httpx.AsyncClient(timeout=30) as client:
                response = await client.post(
                    "https://api.plant.id/v3/identification",
                    headers={"Api-Key": self.settings.plant_id_api_key},
                    json={
                        "images": [f"data:image/jpeg;base64,{b64_image}"],
                    },
                )

                if response.status_code in (200, 201):
                    data = response.json()
                    suggestions = data.get("result", {}).get("classification", {}).get("suggestions", [])

                    if suggestions:
                        best = suggestions[0]
                        return PlantDetectionResult(
                            plant_name=best.get("name", "Unknown"),
                            confidence=best.get("probability", 0.0),
                            scientific_name=best.get("name", ""),
                            top_5=[
                                {"name": s.get("name", ""), "confidence": s.get("probability", 0)}
                                for s in suggestions[:5]
                            ],
                            model_used="plant_id_api",
                        )

        except Exception as e:
            logger.error(f"Plant.id API call failed: {e}")

        return None

    async def _call_plantnet(self, image_data: bytes) -> Optional[PlantDetectionResult]:
        """Call Pl@ntNet API for identification."""
        try:
            import httpx

            async with httpx.AsyncClient(timeout=30) as client:
                response = await client.post(
                    f"https://my-api.plantnet.org/v2/identify/all"
                    f"?api-key={self.settings.plantnet_api_key}",
                    files={"images": ("plant.jpg", image_data, "image/jpeg")},
                    data={"organs": "leaf"},
                )

                if response.status_code == 200:
                    data = response.json()
                    results = data.get("results", [])

                    if results:
                        best = results[0]
                        species = best.get("species", {})
                        return PlantDetectionResult(
                            plant_name=species.get("commonNames", ["Unknown"])[0] if species.get("commonNames") else species.get("scientificNameWithoutAuthor", "Unknown"),
                            confidence=best.get("score", 0.0),
                            scientific_name=species.get("scientificNameWithoutAuthor", ""),
                            family=species.get("family", {}).get("scientificNameWithoutAuthor", ""),
                            common_names=species.get("commonNames", []),
                            top_5=[
                                {
                                    "name": r.get("species", {}).get("scientificNameWithoutAuthor", ""),
                                    "confidence": r.get("score", 0),
                                }
                                for r in results[:5]
                            ],
                            model_used="plantnet_api",
                        )

        except Exception as e:
            logger.error(f"Pl@ntNet API call failed: {e}")

        return None

    @property
    def is_ready(self) -> bool:
        """Check if the detector is ready for inference."""
        return self._model_loaded or bool(
            self.settings.plant_id_api_key or self.settings.plantnet_api_key
        )
