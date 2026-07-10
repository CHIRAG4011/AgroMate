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


@router.post("/{robot_id}/scan")
async def trigger_scan(robot_id: str):
    """Trigger a camera scan on a robot."""
    from ..main import ble_server
    if ble_server:
        await ble_server.request_camera_capture(robot_id)
        return {"status": "success", "message": "Capture requested"}
    return {"status": "error", "detail": "BLE server not running"}


@router.post("/{robot_id}/speak")
async def make_robot_speak(robot_id: str, text: str = "Hello from AgroMate"):
    """Send text for the robot to speak."""
    from ..main import ble_server
    from ..ble.protocol import FaceExpression
    if ble_server:
        await ble_server.send_ai_response(robot_id, text, FaceExpression.TALKING)
        return {"status": "success", "message": "Speech sent"}
    return {"status": "error", "detail": "BLE server not running"}


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
