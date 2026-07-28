"""
AgroMate Backend — Main FastAPI Application

Entry point for the laptop AI server. Initializes all services,
starts the BLE server, and serves the REST/WebSocket API.
"""

import asyncio
import logging
from contextlib import asynccontextmanager

import uvicorn
from pathlib import Path
from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles

from .config import get_settings
from .utils.logger import setup_logging
from .database.crud import init_database, close_database
from .ble.server import BLEServer
from .api.router import api_router
from .api.websocket import router as ws_router

# Global BLE server instance (accessed by API routes)
ble_server: BLEServer = None

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan — startup and shutdown logic."""
    global ble_server

    # ── Startup ──
    setup_logging(settings.log_level, settings.log_file)
    logger = logging.getLogger(__name__)
    logger.info("🌱 AgroMate AI Server starting...")

    # Initialize database
    await init_database(settings.database_url)
    logger.info("✅ Database initialized")

    # Seed default data
    try:
        from .database.crud import get_session
        from .database.migrations import seed_default_data
        async with await get_session() as session:
            await seed_default_data(session)
    except Exception as e:
        logger.warning(f"Seed data warning: {e}")

    # Initialize BLE server
    ble_server = BLEServer(
        max_robots=settings.ble_max_robots,
        scan_timeout=settings.ble_scan_timeout,
    )

    # Start BLE in background
    try:
        await ble_server.start()
        logger.info("✅ BLE server started")
    except Exception as e:
        logger.warning(f"BLE server could not start: {e}")
        logger.info("Server will run without BLE (API-only mode)")

    # Setup BLE event handlers for AI pipeline
    _setup_ble_pipeline(ble_server, logger)

    logger.info(f"🌱 AgroMate AI Server ready at http://{settings.host}:{settings.port}")
    logger.info(f"📖 API docs at http://{settings.host}:{settings.port}/docs")

    yield

    # ── Shutdown ──
    logger.info("🌱 AgroMate AI Server shutting down...")
    if ble_server:
        await ble_server.stop()
    await close_database()
    logger.info("🌱 Goodbye!")
    # Trigger uvicorn reload


def _setup_ble_pipeline(server: BLEServer, logger):
    """Wire up BLE events to AI processing pipeline."""

    def _clean_for_tft(text: str, max_len: int = 60) -> str:
        """
        Sanitize text for TFT display:
        - Strip markdown bold/italic/headers
        - Replace newlines with semicolons (field separator inside a field)
        - Remove pipe chars that would break the pipe-delimited protocol
        - Remove bullet points and leading symbols
        - Collapse whitespace
        - Truncate to max_len
        """
        import re
        if not text:
            return ""
        # Strip markdown bold/italic (**text** → text, *text* → text)
        text = re.sub(r'\*{1,3}(.*?)\*{1,3}', r'\1', text)
        # Strip markdown headers (## Heading → Heading)
        text = re.sub(r'^#{1,6}\s*', '', text, flags=re.MULTILINE)
        # Replace newlines / carriage returns with semicolon separator
        text = re.sub(r'[\r\n]+', '; ', text)
        # Remove pipe characters (protocol delimiter — must not appear in fields)
        text = text.replace('|', '-')
        # Remove leading bullet/dash/number list markers
        text = re.sub(r'^[\-•*\d\.]+\s*', '', text, flags=re.MULTILINE)
        # Remove backticks
        text = text.replace('`', '')
        # Collapse multiple spaces/semicolons
        text = re.sub(r';\s*;+', ';', text)
        text = re.sub(r'\s{2,}', ' ', text)
        text = text.strip('; ').strip()
        # Truncate
        if len(text) > max_len:
            text = text[:max_len - 3].rsplit(' ', 1)[0] + '...'
        return text

    def _resolve_is_healthy(scan_result: dict) -> bool:
        """Determine if the plant is healthy based on scan result fields.
        Handles the Python bool('false') == True gotcha from LLM string outputs."""
        if not scan_result:
            return True
        
        raw = scan_result.get("is_healthy", True)
        # Handle string booleans from LLM JSON output
        if isinstance(raw, str):
            if raw.lower().strip() in ("false", "0", "no", "unhealthy"):
                return False
            return True
        
        # If disease/pest is named, it's NOT healthy regardless of is_healthy flag
        disease = scan_result.get("disease_name", "").strip().lower()
        pest = scan_result.get("pest_name", "").strip().lower()
        deficiency = scan_result.get("primary_deficiency", "").strip().lower()
        
        none_indicators = {"", "none", "n/a", "healthy", "no disease", "no pest", "no deficiency"}
        if disease not in none_indicators or pest not in none_indicators or deficiency not in none_indicators:
            return False
        
        return bool(raw)

    async def on_image_received(robot, image_data):
        """Process image received from robot through AI pipeline."""
        logger.info(f"📸 Image received from {robot.name} ({len(image_data)} bytes)")

        import uuid
        import time
        from pathlib import Path
        from .utils.image_utils import convert_raw_to_jpeg
        from .ai.vision_analyser import VisionAnalyser
        from .chatbot.engine import ChatbotEngine
        from .ble.protocol import FaceExpression

        # Set thinking expression
        await server.set_face_expression(robot.robot_id, FaceExpression.THINKING)

        # 1. Convert raw formats (Grayscale/YUV422) if needed
        jpeg_data = convert_raw_to_jpeg(image_data)

        # 2. Save image to disk under frontend/uploads/
        db_image_path = f"/uploads/scan_{robot.robot_id}_{int(time.time())}.jpg"
        try:
            uploads_dir = Path("frontend/uploads")
            uploads_dir.mkdir(parents=True, exist_ok=True)
            filename = f"scan_{int(time.time() * 1000)}.jpg"
            file_path = uploads_dir / filename
            with open(file_path, "wb") as f:
                f.write(jpeg_data)
            db_image_path = f"/uploads/{filename}"
            logger.info(f"💾 Saved robot image scan to {file_path}")
        except Exception as e:
            logger.error(f"Failed to save scan image to disk: {e}")

        # Single comprehensive Gemini scan — identify + diagnose + symptoms + treatment in one call
        analyser = VisionAnalyser()
        scan_result = await analyser.analyze_robot_scan(jpeg_data)

        scan_context = {}
        common_name = "Unknown Plant"
        scientific_name = "N/A"
        confidence = 0.0
        issue = "Unknown"
        is_healthy = True

        if scan_result:
            common_name     = scan_result.get("plant_name", "Unknown Plant")
            scientific_name = scan_result.get("scientific_name", "N/A")
            confidence      = float(scan_result.get("confidence", 0.0)) * 100
            issue           = scan_result.get("disease_name", "Healthy")
            is_healthy      = _resolve_is_healthy(scan_result)
            if is_healthy:
                issue = "Healthy"

            scan_context = {
                "scan_type":        "disease" if not is_healthy else "identification",
                "plant_name":       common_name,
                "scientific_name":  scientific_name,
                "is_healthy":       is_healthy,
                "disease_name":     issue,
                "severity":         scan_result.get("severity", "none"),
                "treatment":        scan_result.get("chemical_treatment", ""),
                "organic_treatment": scan_result.get("organic_treatment", ""),
            }
        else:
            logger.warning("Robot scan: all AI APIs failed. Using local fallback.")
            try:
                from .ai.plant_detector import PlantDetector
                detector = PlantDetector()
                await detector.initialize()
                plant_res = await detector.identify(jpeg_data)
                if plant_res:
                    common_name     = plant_res.plant_name
                    scientific_name = plant_res.scientific_name
                    confidence      = plant_res.confidence * 100
                    scan_context    = {
                        "scan_type": "identification",
                        "plant_name": common_name,
                        "scientific_name": scientific_name,
                        "is_healthy": True,
                        "disease_name": "Healthy",
                    }
            except Exception as e:
                logger.error(f"Local fallback failed: {e}")

        # Build TFT pipe-delimited payload directly from Gemini response (no chatbot overhead)
        tft_plant   = _clean_for_tft(common_name, 24)
        tft_sci     = _clean_for_tft(scientific_name, 40)
        tft_conf    = f"{min(confidence, 100.0):.1f}"
        tft_summary = _clean_for_tft(
            scan_result.get("ai_summary", "") if scan_result else "", 100
        )

        if not is_healthy and issue.lower() not in ("healthy", "none", ""):
            severity     = scan_context.get("severity", "moderate")
            organic      = scan_context.get("organic_treatment", "")
            chemical     = scan_context.get("treatment", "")
            disease_raw  = scan_context.get("disease_name", issue)

            # Get symptoms directly from Gemini (semicolon-separated)
            symptoms_raw = scan_result.get("symptoms_list", "") if scan_result else ""
            if not symptoms_raw:
                symptoms_raw = "Visible damage;Discoloration;Abnormal growth"
            symptoms = _clean_for_tft(symptoms_raw, 80)

            # Health score from severity
            sev_scores = {"none": 90, "mild": 65, "moderate": 45, "high": 25, "severe": 10}
            health_score = sev_scores.get(severity.lower(), 50)
            if scan_result and scan_result.get("health_score"):
                health_score = int(scan_result["health_score"])

            # Treatment: organic first (semicolons), then chemical fallback
            treatment_combined = organic if organic else chemical
            tft_treatment = _clean_for_tft(treatment_combined, 80)
            tft_disease   = _clean_for_tft(disease_raw, 28)
            tft_severity  = _clean_for_tft(severity, 12)

            display_text = f"D|{tft_plant}|{tft_sci}|{tft_conf}|{tft_disease}|{tft_severity}|{health_score}|{symptoms}|{tft_treatment}|{tft_summary}"
        else:
            health_score = 95
            if scan_result:
                organic_tips = scan_result.get("organic_treatment", "")
                chem_tips    = scan_result.get("chemical_treatment", "")
                tips_raw     = organic_tips or chem_tips
            else:
                tips_raw = scan_context.get("organic_treatment", "") or scan_context.get("treatment", "")
            tft_tips = _clean_for_tft(tips_raw, 80) if tips_raw else "Water regularly;Ensure sunlight;Check soil pH;Apply compost"
            display_text = f"H|{tft_plant}|{tft_sci}|{tft_conf}|{health_score}|{tft_tips}"

        expression = FaceExpression.HAPPY if is_healthy else FaceExpression.WARNING

        # Build concise, clear spoken text for speech synthesis
        conf_pct = int(confidence) if confidence else 0
        response_text = f"Scan complete. The scanned plant is {common_name}"
        if scientific_name and scientific_name != "N/A":
            response_text += f", scientific name {scientific_name}"
        response_text += f", with {conf_pct} percent confidence. "
        if is_healthy:
            response_text += "No diseases detected. Your plant looks healthy!"
        else:
            response_text += f"Detected {issue}. "
            treatment_val = scan_context.get("organic_treatment", "") or scan_context.get("treatment", "")
            if treatment_val:
                first_tip = treatment_val.split(';')[0]
                response_text += f"Recommended treatment: {first_tip}. "
        response_text += "Visit the app for details."
        logger.info(f"🗣️ TTS response: {response_text}")

        # Get robot language for TTS
        lang = robot.config.get("language", "en") if hasattr(robot, 'config') and robot.config else "en"

        # Send structured data to the robot display
        await server.send_ai_response(robot.robot_id, display_text, expression)
        
        # Start speech synthesis & BLE streaming IMMEDIATELY in background (zero delay!)
        async def speak_in_background():
            try:
                from .voice.synthesizer import VoiceSynthesizer
                synthesizer = VoiceSynthesizer()
                await synthesizer.initialize()
                speech = await synthesizer.synthesize(response_text, language=lang, compress_for_robot=True)
                if speech:
                    logger.info(f"🔊 Sending {len(speech.audio_data)} bytes of speech to robot...")
                    await server.send_speech_audio(robot.robot_id, speech.audio_data)
            except Exception as se:
                logger.error(f"Background speech synthesis failed: {se}")

        asyncio.create_task(speak_in_background())

        # Notify robot to increment disease counter if not healthy
        if not is_healthy:
            await server.send_disease_notify(robot.robot_id)

        # Store scan in database & broadcast to frontend in parallel
        try:
            from .database.crud import get_session, create_scan, create_diagnosis, get_robot_by_id
            async with await get_session() as session:
                db_robot = await get_robot_by_id(session, robot.robot_id)
                db_robot_id = db_robot.id if db_robot else None

                scan = await create_scan(
                    session=session, 
                    image_path=db_image_path, 
                    scan_type="disease" if not is_healthy else "identification", 
                    user_id=None,
                    robot_id=db_robot_id
                )
                if auto_result or scan_context:
                    disease_name_val = scan_context.get("disease_name") or scan_context.get("issue_detected") or issue
                    severity_val = scan_context.get("severity") or "none"
                    treatment_val = scan_context.get("treatment") or ""
                    organic_val = scan_context.get("organic_treatment") or ""

                    await create_diagnosis(
                        session,
                        scan_id=scan.id,
                        plant_name=common_name,
                        disease_name=disease_name_val,
                        confidence=confidence / 100.0,
                        severity=severity_val,
                        treatment=treatment_val,
                        organic_treatment=organic_val,
                    )
                
                from .api.websocket import ws_manager
                message = {
                    "type": "scan_complete",
                    "image_path": db_image_path,
                    "plant_name": common_name,
                    "scientific_name": scientific_name,
                    "confidence": confidence,
                    "is_healthy": is_healthy,
                    "issue": issue,
                    "treatment": scan_context.get("treatment", ""),
                    "organic_treatment": scan_context.get("organic_treatment", ""),
                    "response_text": response_text
                }
                await ws_manager.broadcast("robot:all", message)
                await ws_manager.broadcast(f"robot:{robot.robot_id}", message)

                from .database.crud import get_analytics_summary
                summary = await get_analytics_summary(session, days=30)
                if summary:
                    total_scans = summary.get("total_scans", 0)
                    total_diagnoses = summary.get("total_diagnoses", 0)
                    await server.send_stats_sync(robot.robot_id, total_scans, total_diagnoses)
                    logger.info(f"📊 Synced post-scan stats to robot: {robot.name} (Scans={total_scans}, Diseases={total_diagnoses})")
        except Exception as e:
            logger.error(f"Database save or WebSocket broadcast failed: {e}")

    async def on_audio_received(robot, audio_data):
        """Process audio received from robot — speech recognition pipeline."""
        logger.info(f"🎤 Audio received from {robot.name} ({len(audio_data)} bytes)")

        from .utils.audio_utils import decompress_from_ble
        try:
            audio_data = decompress_from_ble(audio_data)
            logger.info(f"🔊 Decompressed robot audio to {len(audio_data)} bytes of 16-bit PCM")
        except Exception as e:
            logger.error(f"Failed to decompress robot audio: {e}")

        from .voice.recognizer import VoiceRecognizer
        from .voice.synthesizer import VoiceSynthesizer
        from .chatbot.engine import ChatbotEngine
        from .ble.protocol import FaceExpression

        # Set listening expression
        await server.set_face_expression(robot.robot_id, FaceExpression.LISTENING)

        # Speech-to-text
        recognizer = VoiceRecognizer()
        await recognizer.initialize()
        
        # Match robot configuration language
        lang = robot.config.get("language", "hi")
        recognition = await recognizer.recognize(audio_data, language=lang)

        if not recognition or not recognition.text.strip():
            await server.send_ai_response(
                robot.robot_id,
                "I couldn't understand that. Please try again.",
                FaceExpression.ERROR,
            )
            return

        logger.info(f"🗣 nighttime Recognized: '{recognition.text}' (lang={recognition.language})")

        text = recognition.text.lower().strip()
        
        # Check for screen switching, scan, or test voice commands
        is_dashboard_cmd = any(cmd in text for cmd in [
            "open dashboard", "show dashboard", "go to dashboard", "dashboard",
            "डैशबोर्ड खोलें", "डैशबोर्ड दिखाएं", "ડેશબોર્ડ ખોલો", "ડેશબોર્ડ"
        ])
        
        is_face_cmd = any(cmd in text for cmd in [
            "close dashboard", "show eye", "show face", "back to eye", "eye animation", "face", "exit",
            "डैशबोर्ड बंद करें", "चेहरा दिखाएं", "आंखें दिखाएं", "चेहरा", "आंख",
            "ડેશબોર્ડ બંધ કરો", "આંખ", "ચહેરો"
        ])

        is_scan_cmd = any(cmd in text for cmd in [
            "scan now", "scan", "take photo", "capture", "start scan", "scan plant",
            "स्कैन करें", "फोटो खींचें", "कैमरा चालू करें", "स्कैन",
            "સ્કેન કરો", "ફોટો લો", "સ્કેન"
        ])

        is_test_voice_cmd = any(cmd in text for cmd in [
            "test voice", "send test voice", "test voice message", "voice test", "send voice test",
            "आवाज का परीक्षण", "टेस्ट वॉइस", "आवाज टेस्ट",
            "ટેસ્ટ વોઈસ", "અવાજ ટેસ્ટ"
        ])

        if is_dashboard_cmd:
            logger.info("🎙️ Voice command recognized: OPEN DASHBOARD")
            await server.set_screen_mode(robot.robot_id, 1) # 1 = DASHBOARD
            
            # Choose confirmation text based on language
            if recognition.language == "hi":
                confirm_text = "डैशबोर्ड खोल रही हूँ।"
            elif recognition.language == "gu":
                confirm_text = "ડેશબોર્ડ ખોલી રહી છું."
            else:
                confirm_text = "Opening dashboard."
                
            await server.send_ai_response(robot.robot_id, confirm_text, FaceExpression.HAPPY)
            try:
                synthesizer = VoiceSynthesizer()
                await synthesizer.initialize()
                speech = await synthesizer.synthesize(confirm_text, language=recognition.language, compress_for_robot=True)
                if speech:
                    await server.send_speech_audio(robot.robot_id, speech.audio_data)
            except Exception as e:
                logger.error(f"Speech synthesis failed: {e}")
            return

        elif is_face_cmd:
            logger.info("🎙️ Voice command recognized: CLOSE DASHBOARD (SHOW FACE)")
            await server.set_screen_mode(robot.robot_id, 0) # 0 = FACE
            
            # Choose confirmation text based on language
            if recognition.language == "hi":
                confirm_text = "चेहरा दिखा रही हूँ।"
            elif recognition.language == "gu":
                confirm_text = "ચહેરો બતાવી રહી છું."
            else:
                confirm_text = "Showing eye animation."
                
            await server.send_ai_response(robot.robot_id, confirm_text, FaceExpression.HAPPY)
            try:
                synthesizer = VoiceSynthesizer()
                await synthesizer.initialize()
                speech = await synthesizer.synthesize(confirm_text, language=recognition.language, compress_for_robot=True)
                if speech:
                    await server.send_speech_audio(robot.robot_id, speech.audio_data)
            except Exception as e:
                logger.error(f"Speech synthesis failed: {e}")
            return

        elif is_scan_cmd:
            logger.info("🎙️ Voice command recognized: SCAN NOW")
            
            if recognition.language == "hi":
                confirm_text = "स्कैनिंग शुरू कर रही हूँ।"
            elif recognition.language == "gu":
                confirm_text = "સ્કેનીંગ શરૂ કરી રહ્યા છીએ."
            else:
                confirm_text = "Starting scan now."
                
            await server.send_ai_response(robot.robot_id, confirm_text, FaceExpression.HAPPY)
            try:
                synthesizer = VoiceSynthesizer()
                await synthesizer.initialize()
                speech = await synthesizer.synthesize(confirm_text, language=recognition.language, compress_for_robot=True)
                if speech:
                    await server.send_speech_audio(robot.robot_id, speech.audio_data)
            except Exception as e:
                logger.error(f"Speech synthesis failed: {e}")
            
            # Trigger scan on the robot
            await server.request_camera_capture(robot.robot_id)
            return

        elif is_test_voice_cmd:
            logger.info("🎙️ Voice command recognized: TEST VOICE")
            
            if recognition.language == "hi":
                test_text = "यह एग्रोमेट आवाज प्रणाली का एक परीक्षण संदेश है। सब कुछ ठीक से काम कर रहा है।"
            elif recognition.language == "gu":
                test_text = "આ એગ્રોમેટ અવાજ સિસ્ટમનો ટેસ્ટ સંદેશ છે. બધું બરાબર કામ કરી રહ્યું છે."
            else:
                test_text = "This is a test of the AgroMate voice system. Everything is working correctly."
                
            await server.send_ai_response(robot.robot_id, test_text, FaceExpression.HAPPY)
            try:
                synthesizer = VoiceSynthesizer()
                await synthesizer.initialize()
                speech = await synthesizer.synthesize(test_text, language=recognition.language, compress_for_robot=True)
                if speech:
                    await server.send_speech_audio(robot.robot_id, speech.audio_data)
            except Exception as e:
                logger.error(f"Speech synthesis failed: {e}")
            return

        # AI chatbot
        await server.set_face_expression(robot.robot_id, FaceExpression.THINKING)
        chatbot = ChatbotEngine()
        await chatbot.initialize()
        
        # Query chatbot
        response = await chatbot.chat(
            message=recognition.text,
            session_id=robot.robot_id,
            language=recognition.language,
            response_type="summary"
        )

        response_text = response.short_summary or response.text

        # Send text response
        await server.send_ai_response(
            robot.robot_id, response_text, FaceExpression.TALKING
        )

        # Generate and send speech (Short Summary only!)
        try:
            synthesizer = VoiceSynthesizer()
            await synthesizer.initialize()
            speech = await synthesizer.synthesize(response_text, language=recognition.language, compress_for_robot=True)
            if speech:
                await server.send_speech_audio(robot.robot_id, speech.audio_data)
        except Exception as e:
            logger.error(f"Speech synthesis failed: {e}")

    # Register handlers
    server.on_image_received(on_image_received)
    server.on_audio_received(on_audio_received)


# ── Create FastAPI App ──

app = FastAPI(
    title="AgroMate AI Server",
    description="AI-powered farming robot backend with plant identification, "
    "disease detection, voice processing, and BLE robot communication.",
    version="1.0.0",
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include routers
app.include_router(api_router)
app.include_router(ws_router)

# ── Mount Frontend Dashboard ──
_frontend_dir = Path(__file__).resolve().parent.parent / "frontend"
if _frontend_dir.is_dir():
    app.mount("/dashboard", StaticFiles(directory=str(_frontend_dir), html=True), name="dashboard")
    uploads_path = _frontend_dir / "uploads"
    uploads_path.mkdir(parents=True, exist_ok=True)
    app.mount("/uploads", StaticFiles(directory=str(uploads_path)), name="uploads")


# ── Root Endpoint ──

@app.get("/", tags=["Root"])
async def root(request: Request):
    """Root endpoint — redirects browsers to dashboard, returns JSON for API clients."""
    accept = request.headers.get("accept", "")
    if "text/html" in accept:
        return RedirectResponse(url="/dashboard", status_code=302)
    return {
        "name": "AgroMate AI Server",
        "version": "1.0.0",
        "status": "running",
        "docs": "/docs",
        "dashboard": "/dashboard",
        "ble_status": ble_server.get_status() if ble_server else {"running": False},
    }


@app.get("/health", tags=["Root"])
async def health_check():
    """Health check endpoint."""
    return {"status": "healthy"}


# ── Entry Point ──

def main():
    """Run the AgroMate server."""
    uvicorn.run(
        "backend.main:app",
        host=settings.host,
        port=settings.port,
        reload=settings.debug,
        log_level=settings.log_level.lower(),
    )


if __name__ == "__main__":
    main()

# Triggered reload to refresh BLE connection state

