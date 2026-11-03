"""AgroMate Robots API — Robot connection management."""

import logging
from fastapi import APIRouter

logger = logging.getLogger(__name__)
router = APIRouter()


@router.get("/")
async def list_robots():
    """List all connected robots."""
    # Injected by app lifespan
    from ..main import ble_server
    if ble_server:
        status = ble_server.get_status()
        return {"status": "success", "robots": status.get("robots", []), "ble": status.get("bluetooth", {})}
    return {"status": "success", "robots": [], "ble": {"available": False}}


@router.get("/{robot_id}/status")
async def get_robot_status(robot_id: str):
    """Get specific robot status."""
    from ..main import ble_server
    if ble_server:
        robot = ble_server.connection_manager.get_robot(robot_id)
        if robot:
            return {"status": "success", "robot": robot.to_dict()}
    return {"status": "error", "detail": "Robot not found"}


@router.post("/discover")
async def discover_robots():
    """Trigger an immediate BLE discovery scan."""
    from ..main import ble_server
    if ble_server and ble_server.discovery:
        discovered = await ble_server.discovery.scan_once()
        return {
            "status": "success",
            "found": len(discovered),
            "robots": [{"address": d.address, "name": d.name} for d in discovered],
        }
    return {"status": "error", "detail": "BLE discovery unavailable"}


@router.post("/{robot_id}/connect")
async def connect_robot(robot_id: str):
    """Connect to a robot over BLE."""
    from ..main import ble_server
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
    
    connected = await ble_server.connect_to_robot(robot_id)
    if connected:
        robot = ble_server.connection_manager.get_robot(robot_id)
        return {"status": "success", "message": "Connected", "robot": robot.to_dict() if robot else None}
    
    robot = ble_server.connection_manager.get_robot(robot_id)
    err = robot.last_error if robot and robot.last_error else "Could not establish BLE connection"
    return {"status": "error", "detail": err}


@router.post("/{robot_id}/disconnect")
async def disconnect_robot(robot_id: str):
    """Disconnect a robot over BLE."""
    from ..main import ble_server
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
    
    await ble_server.disconnect_robot(robot_id)
    return {"status": "success", "message": "Disconnected"}


@router.post("/{robot_id}/scan")
async def trigger_scan(robot_id: str):
    """Trigger a camera scan on a robot."""
    from ..main import ble_server
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
    
    robot = ble_server.connection_manager.get_robot(robot_id)
    success = await ble_server.request_camera_capture(robot_id)
    if success:
        return {"status": "success", "message": "Capture requested"}
    
    err_detail = robot.last_error if robot and robot.last_error else "Device offline or unreachable"
    return {"status": "error", "detail": f"Failed to send capture command: {err_detail}"}


@router.post("/{robot_id}/speak")
async def make_robot_speak(robot_id: str, text: str = "Hello from AgroMate", language: str = "en"):
    """Send text and synthesized speech audio for the robot to speak."""
    from ..main import ble_server
    from ..ble.protocol import FaceExpression
    from ..voice.synthesizer import VoiceSynthesizer
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
        
    # 1. Send text response
    await ble_server.send_ai_response(robot_id, text, FaceExpression.TALKING)
    
    # 2. Synthesize and send audio data
    try:
        synthesizer = VoiceSynthesizer()
        await synthesizer.initialize()
        speech = await synthesizer.synthesize(text, language=language, compress_for_robot=True)
        if speech:
            await ble_server.send_speech_audio(robot_id, speech.audio_data)
            return {"status": "success", "message": "Speech text and audio sent"}
    except Exception as e:
        logger.error(f"Speech synthesis for robot speak endpoint failed: {e}")
        return {"status": "success", "message": "Speech text sent, but audio synthesis failed"}
        
    return {"status": "success", "message": "Speech text sent"}


@router.post("/{robot_id}/sync-weather")
async def sync_weather(robot_id: str, lat: float = 28.6139, lon: float = 77.2090):
    """Fetch current weather and push to robot."""
    from ..main import ble_server
    from ..services.weather import WeatherService
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
    
    weather_svc = WeatherService()
    weather_data = await weather_svc.get_current_weather(lat, lon)
    
    if weather_data:
        temp = weather_data.get("temp", 0.0)
        humidity = weather_data.get("humidity", 0)
        forecast = weather_data.get("description", "Unknown").title()
        
        await ble_server.send_weather_update(robot_id, temp, humidity, forecast)
        return {"status": "success", "message": "Weather synced to robot"}
        
    return {"status": "error", "detail": "Failed to fetch weather data"}


from pydantic import BaseModel

class RobotSyncRequest(BaseModel):
    robot_id: str
    name: str
    mac_address: str
    connected: bool
    battery: int = 50
    signal_strength: str = "Excellent"
    scans_today: int = 0

@router.post("/sync")
async def sync_robot(payload: RobotSyncRequest):
    """Sync robot discovery and status from Android application to backend."""
    from ..main import ble_server
    from ..ble.connection import ConnectionState
    if ble_server:
        # Register or get the robot
        robot = ble_server.connection_manager.register_robot(
            robot_id=payload.robot_id,
            name=payload.name,
            mac_address=payload.mac_address,
            rssi=-50 if payload.signal_strength.lower() == "excellent" else -80
        )
        if robot:
            robot.state = ConnectionState.ACTIVE if payload.connected else ConnectionState.DISCONNECTED
            robot.battery_percent = payload.battery
            robot.update_heartbeat()
            robot.scans_today = payload.scans_today
            return {"status": "success", "robot": robot.to_dict()}
    return {"status": "error", "detail": "BLE server not running"}

@router.post("/{robot_id}/screen-mode")
async def set_robot_screen_mode(robot_id: str, mode: int):
    """Set screen mode of the robot (0=FACE, 1=DASHBOARD)."""
    from ..main import ble_server
    if ble_server:
        await ble_server.set_screen_mode(robot_id, mode)
        return {"status": "success", "message": f"Screen mode set to {mode}"}
    return {"status": "error", "detail": "BLE server not running"}


@router.post("/{robot_id}/config")
async def update_robot_config(robot_id: str, config: dict):
    """Update robot settings configuration."""
    from ..main import ble_server
    if ble_server:
        robot = ble_server.connection_manager.get_robot(robot_id)
        if robot:
            robot.config.update(config)
            return {"status": "success", "message": "Robot config updated", "config": robot.config}
    return {"status": "error", "detail": "Robot not found"}

@router.post("/{robot_id}/intro-voice")
async def play_intro_voice_endpoint(robot_id: str):
    """Play the Eva intro voice on Bluetooth speaker / system audio."""
    from ..main import ble_server
    if not ble_server:
        return {"status": "error", "detail": "BLE server not running"}
    await ble_server._play_intro_voice(force=True)
    return {"status": "success", "message": "Intro voice started on Bluetooth"}
