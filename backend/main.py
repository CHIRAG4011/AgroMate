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


def _setup_ble_pipeline(server: BLEServer, logger):
    """Wire up BLE events to AI processing pipeline."""

    async def on_image_received(robot, image_data):
        """Process image received from robot through AI pipeline."""
        logger.info(f"📸 Image received from {robot.name} ({len(image_data)} bytes)")

        from .ai.plant_detector import PlantDetector
        from .ai.disease_detector import DiseaseDetector
        from .chatbot.engine import ChatbotEngine
        from .ble.protocol import FaceExpression

        # Set thinking expression
        await server.set_face_expression(robot.robot_id, FaceExpression.THINKING)

        # Identify plant
        plant_detector = PlantDetector()
        await plant_detector.initialize()
        plant_result = await plant_detector.identify(image_data)

        # Detect disease
        disease_detector = DiseaseDetector()
        await disease_detector.initialize()
        plant_name = plant_result.plant_name if plant_result else None
        disease_result = await disease_detector.detect(image_data, plant_name)

        # Build scan context for chatbot
        scan_context = {}
        if plant_result:
            scan_context["plant_name"] = plant_result.plant_name
            scan_context["scientific_name"] = plant_result.scientific_name
            scan_context["confidence"] = plant_result.confidence
        if disease_result:
            scan_context["disease_name"] = disease_result.disease_name
            scan_context["is_healthy"] = disease_result.is_healthy
            scan_context["severity"] = disease_result.severity
            scan_context["treatment"] = disease_result.treatment
            scan_context["organic_treatment"] = disease_result.organic_treatment

        # Use ChatbotEngine to generate the localized two-tier summary response
        chatbot = ChatbotEngine()
        await chatbot.initialize()
        
        # Get language from robot settings or default to Hindi/English
        lang = robot.config.get("language", "hi")
        
        # Query chatbot with prompt trigger
        query_msg = "Scan results received. Generate a summary."
        chat_response = await chatbot.chat(
            message=query_msg,
            session_id=robot.robot_id,
            language=lang,
            scan_context=scan_context,
            response_type="summary"
        )

        response_text = chat_response.short_summary or chat_response.text
        expression = FaceExpression.HAPPY if (disease_result and disease_result.is_healthy) else FaceExpression.WARNING

        # Send response back to robot
        await server.send_ai_response(robot.robot_id, response_text, expression)

        # Generate and send speech (Short Summary only!)
        try:
            from .voice.synthesizer import VoiceSynthesizer
            synthesizer = VoiceSynthesizer()
            await synthesizer.initialize()
            speech = await synthesizer.synthesize(response_text, language=lang)
            if speech:
                await server.send_speech_audio(robot.robot_id, speech.audio_data)
        except Exception as e:
            logger.error(f"Speech synthesis failed: {e}")

        # Store scan in database
        try:
            from .database.crud import get_session, create_scan, create_diagnosis
            async with await get_session() as session:
                scan = await create_scan(session, image_path=f"ble://{robot.robot_id}/capture")
                if disease_result:
                    await create_diagnosis(
                        session,
                        scan_id=scan.id,
                        plant_name=plant_result.plant_name if plant_result else None,
                        disease_name=disease_result.disease_name,
                        confidence=disease_result.confidence,
                        severity=disease_result.severity,
                        treatment=disease_result.treatment,
                        organic_treatment=disease_result.organic_treatment,
                    )
        except Exception as e:
            logger.error(f"Failed to store scan: {e}")

    async def on_audio_received(robot, audio_data):
        """Process audio received from robot — speech recognition pipeline."""
        logger.info(f"🎤 Audio received from {robot.name} ({len(audio_data)} bytes)")

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
        
        # Check for screen switching voice commands
        is_dashboard_cmd = any(cmd in text for cmd in [
            "open dashboard", "show dashboard", "go to dashboard", "dashboard",
            "डैशबोर्ड खोलें", "डैशबोर्ड दिखाएं", "ડેશબોર્ડ ખોલો", "ડેશબોર્ડ"
        ])
        
        is_face_cmd = any(cmd in text for cmd in [
            "close dashboard", "show eye", "show face", "back to eye", "eye animation", "face", "exit",
            "डैशबोर्ड बंद करें", "चेहरा दिखाएं", "आंखें दिखाएं", "चेहरा", "आंख",
            "ડેશબોર્ડ બંધ કરો", "આંખ", "ચહેરો"
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
                speech = await synthesizer.synthesize(confirm_text, language=recognition.language)
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
                speech = await synthesizer.synthesize(confirm_text, language=recognition.language)
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
            speech = await synthesizer.synthesize(response_text, language=recognition.language)
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
