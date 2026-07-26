"""AgroMate Plants API — Plant identification and information with high-accuracy Vision fallback."""

import asyncio
import logging
import time
import uuid
from pathlib import Path
from fastapi import APIRouter, BackgroundTasks, UploadFile, File, HTTPException, Query
from ..ai.vision_analyser import VisionAnalyser
from ..chatbot.engine import ChatbotEngine
from ..database.crud import get_session, create_scan, create_diagnosis

logger = logging.getLogger(__name__)
router = APIRouter()

# Maximum seconds to wait for Vision API before giving up and falling back to local models
_VISION_TIMEOUT_SECONDS = 60


async def _save_scan_to_db(image_data: bytes, scan_type: str, result_dict: dict):
    """Save the scanned image file locally and persist it to the SQLite database."""
    try:
        # Save image file to disk
        uploads_dir = Path("frontend/uploads")
        uploads_dir.mkdir(parents=True, exist_ok=True)
        filename = f"scan_{int(time.time() * 1000)}.jpg"
        file_path = uploads_dir / filename
        with open(file_path, "wb") as f:
            f.write(image_data)
        
        db_image_path = f"/uploads/{filename}"

        # Resolve scan type value
        scan_type_val = "identification"
        if scan_type == "diagnose":
            scan_type_val = "disease"
        elif scan_type == "pests":
            scan_type_val = "pest"
        elif scan_type == "nutrients":
            scan_type_val = "nutrient"

        async with await get_session() as session:
            db_scan = await create_scan(
                session=session,
                image_path=db_image_path,
                scan_type=scan_type_val,
                user_id=None,
                robot_id=None
            )

            # Determine fields
            plant_name = result_dict.get("plant_name", "Unknown")
            
            if scan_type == "identify":
                disease_name = "Healthy"
                severity = "none"
                treatment = ""
                organic_treatment = ""
            elif scan_type == "diagnose":
                disease_name = result_dict.get("disease_name", "").strip()
                raw_healthy = result_dict.get("is_healthy")
                if disease_name and disease_name.lower() not in ("healthy", "none", "n/a", "", "null"):
                    is_healthy = False
                elif raw_healthy is not None:
                    is_healthy = (str(raw_healthy).lower().strip() in ("true", "1", "yes", "healthy")) if isinstance(raw_healthy, str) else bool(raw_healthy)
                else:
                    is_healthy = True
                if not disease_name:
                    disease_name = "Healthy" if is_healthy else "Diseased Plant"
                severity = result_dict.get("severity") or ("none" if is_healthy else "moderate")
                treatment = result_dict.get("treatment", "")
                organic_treatment = result_dict.get("organic_treatment", "")
            elif scan_type == "pests":
                pest_name = result_dict.get("pest_name", "None")
                disease_name = f"Pest: {pest_name}" if pest_name not in ["None", ""] else "Healthy"
                severity = result_dict.get("severity", "none")
                treatment = result_dict.get("chemical_treatment", "")
                organic_treatment = result_dict.get("organic_treatment", "")
            elif scan_type == "nutrients":
                deficiency = result_dict.get("primary_deficiency", "None")
                disease_name = f"Deficiency: {deficiency}" if deficiency not in ["None", ""] else "Healthy"
                severity = "mild" if result_dict.get("deficiency_detected", False) else "none"
                treatment = result_dict.get("treatment", "")
                organic_treatment = result_dict.get("organic_option", "")

            await create_diagnosis(
                session=session,
                scan_id=db_scan.id,
                plant_name=plant_name,
                disease_name=disease_name,
                confidence=result_dict.get("confidence", 0.9),
                severity=severity,
                treatment=treatment,
                organic_treatment=organic_treatment,
                processing_time_ms=result_dict.get("processing_time_ms", 0.0)
            )
    except Exception as e:
        logger.error(f"Failed to save scan to database: {e}")


async def _generate_ai_summary(result_dict: dict, scan_type: str, language: str) -> str:
    """Generate a short localized summary of the scan results using ChatbotEngine."""
    try:
        chatbot = ChatbotEngine()
        await chatbot.initialize()

        # Build appropriate scan context based on scan type
        if scan_type == "identify":
            scan_context = {
                "scan_type": "identification",
                "plant_name": result_dict.get("plant_name", "Unknown"),
                "scientific_name": result_dict.get("scientific_name", "Unknown"),
                "family": result_dict.get("family", "Unknown"),
                "is_healthy": True
            }
        elif scan_type == "diagnose":
            scan_context = {
                "scan_type": "disease",
                "plant_name": result_dict.get("plant_name", "Unknown"),
                "disease_name": result_dict.get("disease_name", "Healthy"),
                "severity": result_dict.get("severity", "none"),
                "is_healthy": result_dict.get("is_healthy", True),
                "organic_treatment": result_dict.get("organic_treatment", ""),
                "treatment": result_dict.get("treatment", "")
            }
        elif scan_type == "pests":
            scan_context = {
                "scan_type": "pest",
                "pest_name": result_dict.get("pest_name", "None"),
                "count": result_dict.get("count", 0),
                "severity": result_dict.get("severity", "none"),
                "organic_treatment": result_dict.get("organic_treatment", ""),
                "chemical_treatment": result_dict.get("chemical_treatment", ""),
                "is_healthy": result_dict.get("pest_name") in [None, "None", ""]
            }
        elif scan_type == "nutrients":
            scan_context = {
                "scan_type": "nutrient",
                "deficiency_detected": result_dict.get("deficiency_detected", False),
                "primary_deficiency": result_dict.get("primary_deficiency", "None"),
                "organic_treatment": result_dict.get("organic_option", ""),
                "treatment": result_dict.get("treatment", ""),
                "is_healthy": not result_dict.get("deficiency_detected", False)
            }
        else:
            return ""

        chat_response = await chatbot.chat(
            message="Scan results received. Generate a summary.",
            session_id="default",
            language=language,
            scan_context=scan_context,
            response_type="summary"
        )
        return chat_response.short_summary or chat_response.text
    except Exception as e:
        logger.error(f"Failed to generate AI scan summary: {e}")
        return ""


@router.post("/identify")
async def identify_plant(background_tasks: BackgroundTasks, image: UploadFile = File(...), language: str = Query("en")):
    """Identify a plant from an uploaded image."""
    if not image.content_type.startswith("image/"):
        raise HTTPException(status_code=400, detail="File must be an image")

    image_data = await image.read()
    start_time = time.time()
    
    # 1. Try Premium Vision Analyser first (with hard timeout)
    vision = VisionAnalyser()
    try:
        result_dict = await asyncio.wait_for(
            vision.analyze_image(image_data, "identify"),
            timeout=_VISION_TIMEOUT_SECONDS
        )
    except asyncio.TimeoutError:
        logger.warning("Vision API timed out for identify — falling back to local model")
        result_dict = None
    
    if result_dict:
        result_dict["model_used"] = "gemini_vision"
        result_dict["processing_time_ms"] = (time.time() - start_time) * 1000
    else:
        # Fallback to local classification
        from ..ai.plant_detector import PlantDetector
        detector = PlantDetector()
        await detector.initialize()
        result = await detector.identify(image_data)
        if result:
            result_dict = result.to_dict()
        else:
            raise HTTPException(status_code=422, detail="Could not identify plant")

    # Fetch Trefle API details in background (non-blocking)
    sc_name = result_dict.get("scientific_name")
    if sc_name:
        try:
            from ..services.plant_api import PlantAPIService
            plant_api = PlantAPIService()
            await plant_api.initialize()
            trefle_data = await plant_api.get_plant_details(sc_name)
            if trefle_data:
                result_dict["trefle_details"] = {
                    "genus": trefle_data.get("genus"),
                    "family": trefle_data.get("family"),
                    "bibliography": trefle_data.get("bibliography"),
                    "image_url": trefle_data.get("image_url"),
                    "year": trefle_data.get("year"),
                    "author": trefle_data.get("author")
                }
        except Exception as ex:
            logger.error(f"Failed to query Trefle API for details: {ex}")

    # Generate AI summary inline but with a short timeout so it doesn't block
    try:
        result_dict["ai_summary"] = await asyncio.wait_for(
            _generate_ai_summary(result_dict, "identify", language),
            timeout=20.0
        )
    except asyncio.TimeoutError:
        logger.warning("AI summary timed out for identify scan — returning without summary")
        result_dict["ai_summary"] = ""
    
    # Persist scan to database in background (non-blocking)
    background_tasks.add_task(_save_scan_to_db, image_data, "identify", result_dict)
    
    return {"status": "success", "result": result_dict}


@router.post("/diagnose")
async def diagnose_plant(background_tasks: BackgroundTasks, image: UploadFile = File(...), plant_name: str = None, language: str = Query("en")):
    """Diagnose disease in a plant image."""
    image_data = await image.read()
    start_time = time.time()

    # 1. Try Premium Vision Analyser first (with hard timeout)
    vision = VisionAnalyser()
    try:
        result_dict = await asyncio.wait_for(
            vision.analyze_image(image_data, "diagnose", plant_name),
            timeout=_VISION_TIMEOUT_SECONDS
        )
    except asyncio.TimeoutError:
        logger.warning("Vision API timed out for diagnose — falling back to local model")
        result_dict = None

    if result_dict:
        result_dict["model_used"] = "gemini_vision"
        result_dict["processing_time_ms"] = (time.time() - start_time) * 1000
    else:
        # Fallback to local diagnosis
        from ..ai.disease_detector import DiseaseDetector
        detector = DiseaseDetector()
        await detector.initialize()
        result = await detector.detect(image_data, plant_name)
        if result:
            result_dict = result.to_dict()
        else:
            raise HTTPException(status_code=422, detail="Could not diagnose plant")

    # Generate AI summary with timeout
    try:
        result_dict["ai_summary"] = await asyncio.wait_for(
            _generate_ai_summary(result_dict, "diagnose", language),
            timeout=20.0
        )
    except asyncio.TimeoutError:
        logger.warning("AI summary timed out for diagnose scan")
        result_dict["ai_summary"] = ""
    
    # Persist scan to database in background (non-blocking)
    background_tasks.add_task(_save_scan_to_db, image_data, "diagnose", result_dict)
    
    return {"status": "success", "result": result_dict}


@router.post("/pests")
async def detect_pests(background_tasks: BackgroundTasks, image: UploadFile = File(...), language: str = Query("en")):
    """Detect pests in an image."""
    image_data = await image.read()
    start_time = time.time()

    # 1. Try Premium Vision Analyser first (with hard timeout)
    vision = VisionAnalyser()
    try:
        result_dict = await asyncio.wait_for(
            vision.analyze_image(image_data, "pests"),
            timeout=_VISION_TIMEOUT_SECONDS
        )
    except asyncio.TimeoutError:
        logger.warning("Vision API timed out for pests — falling back to local model")
        result_dict = None

    if result_dict:
        result_dict["model_used"] = "gemini_vision"
        result_dict["processing_time_ms"] = (time.time() - start_time) * 1000
    else:
        # Fallback to local detector
        from ..ai.pest_detector import PestDetector
        detector = PestDetector()
        await detector.initialize()
        result = await detector.detect(image_data)
        if result:
            result_dict = result.to_dict()
        else:
            raise HTTPException(status_code=422, detail="Could not analyze image")

    # Generate AI summary with timeout
    try:
        result_dict["ai_summary"] = await asyncio.wait_for(
            _generate_ai_summary(result_dict, "pests", language),
            timeout=20.0
        )
    except asyncio.TimeoutError:
        logger.warning("AI summary timed out for pests scan")
        result_dict["ai_summary"] = ""
    
    # Persist scan to database in background (non-blocking)
    background_tasks.add_task(_save_scan_to_db, image_data, "pests", result_dict)
    
    return {"status": "success", "result": result_dict}


@router.post("/nutrients")
async def analyze_nutrients(background_tasks: BackgroundTasks, image: UploadFile = File(...), language: str = Query("en")):
    """Analyze nutrient deficiency from a leaf image."""
    image_data = await image.read()
    start_time = time.time()

    # 1. Try Premium Vision Analyser first (with hard timeout)
    vision = VisionAnalyser()
    try:
        result_dict = await asyncio.wait_for(
            vision.analyze_image(image_data, "nutrients"),
            timeout=_VISION_TIMEOUT_SECONDS
        )
    except asyncio.TimeoutError:
        logger.warning("Vision API timed out for nutrients — falling back to local model")
        result_dict = None

    if result_dict:
        result_dict["model_used"] = "gemini_vision"
        result_dict["processing_time_ms"] = (time.time() - start_time) * 1000
    else:
        # Fallback to local analyzer
        from ..ai.nutrient_analyzer import NutrientAnalyzer
        analyzer = NutrientAnalyzer()
        result = await analyzer.analyze(image_data)
        if result:
            result_dict = result.to_dict()
        else:
            raise HTTPException(status_code=422, detail="Could not analyze nutrients")

    # Generate AI summary with timeout
    try:
        result_dict["ai_summary"] = await asyncio.wait_for(
            _generate_ai_summary(result_dict, "nutrients", language),
            timeout=20.0
        )
    except asyncio.TimeoutError:
        logger.warning("AI summary timed out for nutrients scan")
        result_dict["ai_summary"] = ""
    
    # Persist scan to database in background (non-blocking)
    background_tasks.add_task(_save_scan_to_db, image_data, "nutrients", result_dict)
    
    return {"status": "success", "result": result_dict}


@router.get("/crops")
async def get_crop_recommendations(
    temperature: float = None,
    soil_type: str = None,
    water: str = None,
    month: int = None,
):
    """Get crop recommendations based on conditions."""
    from ..ai.crop_advisor import CropAdvisor

    advisor = CropAdvisor()
    result = await advisor.get_recommendations(
        temperature=temperature, soil_type=soil_type, water_availability=water, month=month
    )
    return {"status": "success", "result": result.to_dict()}
