"""
AgroMate Disease Detector

AI-powered plant disease detection using image classification.
Trained on PlantVillage + PlantDoc datasets for multi-class disease identification.
"""

import logging
import time
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, field

import numpy as np

from ..utils.image_utils import load_image, preprocess_for_model, enhance_plant_image
from ..config import get_settings
from ..database.seed import COMMON_DISEASES

logger = logging.getLogger(__name__)

# PlantVillage disease class labels (38 classes)
DISEASE_CLASSES = [
    "Apple___Apple_scab", "Apple___Black_rot", "Apple___Cedar_apple_rust", "Apple___healthy",
    "Blueberry___healthy",
    "Cherry___Powdery_mildew", "Cherry___healthy",
    "Corn___Cercospora_leaf_spot", "Corn___Common_rust", "Corn___Northern_Leaf_Blight", "Corn___healthy",
    "Grape___Black_rot", "Grape___Esca", "Grape___Leaf_blight", "Grape___healthy",
    "Orange___Huanglongbing",
    "Peach___Bacterial_spot", "Peach___healthy",
    "Pepper___Bacterial_spot", "Pepper___healthy",
    "Potato___Early_blight", "Potato___Late_blight", "Potato___healthy",
    "Raspberry___healthy",
    "Rice___Blast", "Rice___Brown_spot", "Rice___Leaf_smut", "Rice___healthy",
    "Soybean___healthy",
    "Squash___Powdery_mildew",
    "Strawberry___Leaf_scorch", "Strawberry___healthy",
    "Tomato___Bacterial_spot", "Tomato___Early_blight", "Tomato___Late_blight",
    "Tomato___Leaf_Mold", "Tomato___Septoria_leaf_spot",
    "Tomato___Spider_mites", "Tomato___Target_Spot",
    "Tomato___Mosaic_virus", "Tomato___Yellow_Leaf_Curl_Virus", "Tomato___healthy",
    "Wheat___Rust", "Wheat___Septoria", "Wheat___healthy",
]


@dataclass
class DiseaseDetectionResult:
    """Result of disease detection analysis."""
    plant_name: str
    disease_name: str
    is_healthy: bool
    confidence: float
    severity: str = "unknown"  # mild, moderate, severe
    affected_area_percent: float = 0.0
    treatment: str = ""
    organic_treatment: str = ""
    fertilizer_recommendation: str = ""
    water_recommendation: str = ""
    top_5: list = field(default_factory=list)
    model_used: str = "local"
    processing_time_ms: float = 0.0

    def to_dict(self) -> dict:
        return {
            "plant_name": self.plant_name,
            "disease_name": self.disease_name,
            "is_healthy": bool(self.is_healthy),
            "confidence": float(round(self.confidence, 4)),
            "severity": self.severity,
            "affected_area_percent": float(round(self.affected_area_percent, 1)),
            "treatment": self.treatment,
            "organic_treatment": self.organic_treatment,
            "fertilizer_recommendation": self.fertilizer_recommendation,
            "water_recommendation": self.water_recommendation,
            "top_5": self.top_5,
            "model_used": self.model_used,
            "processing_time_ms": float(round(self.processing_time_ms, 2)),
        }


class DiseaseDetector:
    """
    Plant disease detection engine.

    Uses a multi-class image classification model trained on PlantVillage
    dataset (38 classes across 14 plant species).
    """

    def __init__(self):
        self.settings = get_settings()
        self._model = None
        self._model_loaded = False
        self._class_labels = DISEASE_CLASSES
        self._treatment_db = self._build_treatment_db()

    def _build_treatment_db(self) -> dict:
        """Build a treatment lookup database from seed data."""
        db = {}
        for entry in COMMON_DISEASES:
            key = f"{entry['plant']}_{entry['disease']}".lower().replace(" ", "_")
            db[key] = {
                "treatment": entry.get("treatment", ""),
                "organic_treatment": entry.get("organic_treatment", ""),
                "symptoms": entry.get("symptoms", ""),
                "pathogen": entry.get("pathogen", ""),
            }
        return db

    async def initialize(self):
        """Load the disease classification model."""
        model_path = Path(self.settings.disease_model_path)

        if model_path.exists():
            try:
                import onnxruntime as ort

                self._model = ort.InferenceSession(
                    str(model_path),
                    providers=["CPUExecutionProvider"],
                )
                self._model_loaded = True
                logger.info(f"Disease detection model loaded: {model_path.name}")
            except Exception as e:
                logger.warning(f"Failed to load disease model: {e}")
        else:
            logger.info(
                f"Disease model not found at {model_path}. "
                "Disease detection will use heuristic analysis."
            )

    async def detect(
        self,
        image_data: bytes,
        plant_name: str = None,
    ) -> Optional[DiseaseDetectionResult]:
        """
        Detect diseases in a plant image.

        Args:
            image_data: Raw image bytes (JPEG/PNG)
            plant_name: Optional known plant name to narrow results

        Returns:
            DiseaseDetectionResult or None on failure
        """
        start_time = time.time()

        # Try API-based fallback first (if API key is configured)
        if self.settings.plant_id_api_key:
            api_result = await self._detect_api(image_data, plant_name)
            if api_result:
                api_result.processing_time_ms = (time.time() - start_time) * 1000
                return api_result

        if self._model_loaded:
            result = await self._predict_local(image_data, plant_name)
        else:
            result = await self._predict_heuristic(image_data, plant_name)

        if result:
            result.processing_time_ms = (time.time() - start_time) * 1000
            # Enrich with treatment info
            self._enrich_result(result)

        return result

    async def _detect_api(
        self, image_data: bytes, plant_name: str = None
    ) -> Optional[DiseaseDetectionResult]:
        """Call Plant.id Health Assessment API for high-accuracy disease diagnosis."""
        try:
            import base64
            import httpx

            b64_image = base64.b64encode(image_data).decode("utf-8")

            async with httpx.AsyncClient(timeout=30) as client:
                response = await client.post(
                    "https://api.plant.id/v3/health_assessment",
                    headers={"Api-Key": self.settings.plant_id_api_key},
                    json={
                        "images": [f"data:image/jpeg;base64,{b64_image}"],
                    },
                )

                if response.status_code in (200, 201):
                    data = response.json()
                    res = data.get("result", {})
                    
                    is_healthy_obj = res.get("is_healthy", {})
                    is_healthy = is_healthy_obj.get("binary", True)
                    
                    disease_obj = res.get("disease", {})
                    suggestions = disease_obj.get("suggestions", [])
                    
                    best_disease = "Healthy"
                    confidence = is_healthy_obj.get("probability", 1.0)
                    treatment = ""
                    organic_treatment = ""
                    top_5 = []

                    if suggestions:
                        best = suggestions[0]
                        best_disease = best.get("name", "Unknown Disease")
                        confidence = best.get("probability", 0.0)
                        
                        treatment_info = best.get("treatment", {})
                        chem_treatments = treatment_info.get("chemical", [])
                        bio_treatments = treatment_info.get("biological", [])
                        prev_measures = treatment_info.get("prevention", [])
                        
                        treatment = ". ".join(chem_treatments) if chem_treatments else "Consider chemical fungicides if infestation is severe."
                        organic_treatment = ". ".join(bio_treatments + prev_measures) if (bio_treatments or prev_measures) else "Remove infected leaves and avoid overhead watering."
                        
                        top_5 = [
                            {
                                "plant": plant_name or "Plant",
                                "condition": s.get("name", "Unknown"),
                                "confidence": s.get("probability", 0.0)
                            }
                            for s in suggestions[:5]
                        ]
                    
                    severity = self._estimate_severity(confidence, is_healthy)

                    return DiseaseDetectionResult(
                        plant_name=plant_name or "Crop Plant",
                        disease_name="Healthy" if is_healthy else best_disease,
                        is_healthy=is_healthy,
                        confidence=confidence,
                        severity=severity,
                        treatment=treatment,
                        organic_treatment=organic_treatment,
                        top_5=top_5,
                        model_used="plant_id_health_api"
                    )
        except Exception as e:
            logger.error(f"Plant.id health API call failed: {e}")
        return None

    async def _predict_local(
        self, image_data: bytes, plant_name: str = None
    ) -> Optional[DiseaseDetectionResult]:
        """Run local ONNX model inference for disease detection."""
        try:
            image = load_image(image_data)
            if image is None:
                return None

            image = enhance_plant_image(image)
            input_array = preprocess_for_model(image, input_size=(224, 224))
            if input_array is None:
                return None

            input_name = self._model.get_inputs()[0].name
            output = self._model.run(None, {input_name: input_array})
            probabilities = output[0][0]

            # Softmax if needed
            if np.min(probabilities) < 0 or np.sum(probabilities) > 1.5:
                exp_vals = np.exp(probabilities - np.max(probabilities))
                probabilities = exp_vals / np.sum(exp_vals)

            # Filter by plant name if provided
            if plant_name:
                mask = np.zeros_like(probabilities)
                for i, label in enumerate(self._class_labels):
                    if plant_name.lower() in label.lower():
                        mask[i] = 1.0
                if mask.sum() > 0:
                    probabilities = probabilities * mask
                    total = probabilities.sum()
                    if total > 0:
                        probabilities = probabilities / total

            # Get top 5
            top_indices = np.argsort(probabilities)[::-1][:5]
            top_5 = []
            for i in top_indices:
                if i < len(self._class_labels):
                    label = self._class_labels[i]
                    parts = label.split("___")
                    top_5.append({
                        "plant": parts[0] if len(parts) > 0 else "Unknown",
                        "condition": parts[1].replace("_", " ") if len(parts) > 1 else "Unknown",
                        "confidence": float(probabilities[i]),
                    })

            best_idx = top_indices[0]
            best_label = self._class_labels[best_idx] if best_idx < len(self._class_labels) else "Unknown___Unknown"
            parts = best_label.split("___")

            detected_plant = parts[0] if len(parts) > 0 else "Unknown"
            condition = parts[1].replace("_", " ") if len(parts) > 1 else "Unknown"
            is_healthy = "healthy" in condition.lower()

            # Estimate severity based on confidence
            confidence = float(probabilities[best_idx])
            severity = self._estimate_severity(confidence, is_healthy)

            return DiseaseDetectionResult(
                plant_name=detected_plant,
                disease_name="" if is_healthy else condition,
                is_healthy=is_healthy,
                confidence=confidence,
                severity=severity,
                top_5=top_5,
                model_used="local_onnx",
            )

        except Exception as e:
            logger.error(f"Local disease detection failed: {e}")
            return None

    async def _predict_heuristic(
        self, image_data: bytes, plant_name: str = None
    ) -> Optional[DiseaseDetectionResult]:
        """
        Heuristic disease analysis using color analysis.
        
        Used when no ML model is available. Analyzes leaf color distribution
        to detect potential issues.
        """
        try:
            image = load_image(image_data, target_size=(224, 224))
            if image is None:
                return None

            arr = np.array(image)

            # Analyze color channels
            r_mean = arr[:, :, 0].mean()
            g_mean = arr[:, :, 1].mean()
            b_mean = arr[:, :, 2].mean()

            # Brown spots indicator (high R, low G)
            brown_ratio = r_mean / (g_mean + 1)
            # Yellow indicator
            yellow_ratio = (r_mean + g_mean) / (2 * (b_mean + 1))
            # Green health indicator
            green_ratio = g_mean / (r_mean + 1)

            is_healthy = green_ratio > 1.1 and brown_ratio < 0.9

            if is_healthy:
                disease_name = ""
                confidence = min(green_ratio / 2.0, 0.85)
                severity = "none"
            elif brown_ratio > 1.3:
                disease_name = "Possible fungal infection (brown spots detected)"
                confidence = min(brown_ratio / 2.0, 0.70)
                severity = "moderate" if brown_ratio > 1.5 else "mild"
            elif yellow_ratio > 2.0:
                disease_name = "Possible nutrient deficiency (yellowing detected)"
                confidence = min(yellow_ratio / 4.0, 0.65)
                severity = "mild"
            else:
                disease_name = "Possible stress or early disease"
                confidence = 0.40
                severity = "mild"

            return DiseaseDetectionResult(
                plant_name=plant_name or "Unknown",
                disease_name=disease_name,
                is_healthy=is_healthy,
                confidence=confidence,
                severity=severity,
                model_used="heuristic_color_analysis",
            )

        except Exception as e:
            logger.error(f"Heuristic analysis failed: {e}")
            return None

    def _estimate_severity(self, confidence: float, is_healthy: bool) -> str:
        """Estimate disease severity from confidence score."""
        if is_healthy:
            return "none"
        if confidence > 0.85:
            return "severe"
        elif confidence > 0.65:
            return "moderate"
        else:
            return "mild"

    def _enrich_result(self, result: DiseaseDetectionResult):
        """Enrich detection result with treatment information from knowledge base."""
        if result.is_healthy:
            result.treatment = "Plant appears healthy. Continue regular care."
            result.organic_treatment = "Maintain organic care practices."
            return

        # Look up treatment in knowledge base
        for entry in COMMON_DISEASES:
            plant_match = entry["plant"].lower() in result.plant_name.lower()
            disease_match = (
                entry["disease"].lower() in result.disease_name.lower()
                or result.disease_name.lower() in entry["disease"].lower()
            )

            if plant_match and disease_match:
                result.treatment = entry.get("treatment", result.treatment)
                result.organic_treatment = entry.get("organic_treatment", result.organic_treatment)
                return

        # Generic advice if no specific match found
        if not result.treatment:
            result.treatment = (
                "Consult a local agricultural extension office for specific treatment. "
                "General recommendations: Remove affected leaves, improve air circulation, "
                "and consider appropriate fungicide application."
            )
        if not result.organic_treatment:
            result.organic_treatment = (
                "Neem oil spray, copper-based organic fungicide, or baking soda solution "
                "(1 tablespoon per gallon of water). Remove and destroy affected plant parts."
            )

    @property
    def is_ready(self) -> bool:
        """Check if the detector is ready."""
        return True  # Heuristic analysis is always available
