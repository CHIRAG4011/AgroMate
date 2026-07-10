"""AgroMate WebSocket — Real-time communication for robot updates and chat."""

import logging
import json
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

logger = logging.getLogger(__name__)
router = APIRouter()

# Active WebSocket connections
_active_connections: dict[str, list[WebSocket]] = {}


class ConnectionManager:
    """Manages WebSocket connections."""

    def __init__(self):
        self.connections: dict[str, list[WebSocket]] = {}

    async def connect(self, websocket: WebSocket, channel: str):
        await websocket.accept()
        if channel not in self.connections:
            self.connections[channel] = []
        self.connections[channel].append(websocket)
        logger.info(f"WebSocket connected to channel: {channel}")

    def disconnect(self, websocket: WebSocket, channel: str):
        if channel in self.connections:
            self.connections[channel] = [
                c for c in self.connections[channel] if c != websocket
            ]

    async def broadcast(self, channel: str, message: dict):
        for connection in self.connections.get(channel, []):
            try:
                await connection.send_json(message)
            except Exception:
                pass


ws_manager = ConnectionManager()


@router.websocket("/ws/robot/{robot_id}")
async def robot_websocket(websocket: WebSocket, robot_id: str):
    """WebSocket for real-time robot status updates."""
    await ws_manager.connect(websocket, f"robot:{robot_id}")
    try:
        while True:
            data = await websocket.receive_text()
            message = json.loads(data)
            # Handle incoming commands
            if message.get("type") == "scan":
                from ..main import ble_server
                if ble_server:
                    await ble_server.request_camera_capture(robot_id)
            elif message.get("type") == "speak":
                from ..main import ble_server
                if ble_server:
                    await ble_server.send_ai_response(robot_id, message.get("text", ""))
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket, f"robot:{robot_id}")


@router.websocket("/ws/chat")
async def chat_websocket(websocket: WebSocket):
    """WebSocket for real-time chat."""
    await ws_manager.connect(websocket, "chat")
    try:
        while True:
            data = await websocket.receive_text()
            message = json.loads(data)
            # Process chat message
            from ..chatbot.engine import ChatbotEngine
            chatbot = ChatbotEngine()
            await chatbot.initialize()
            response = await chatbot.chat(
                message=message.get("text", ""),
                language=message.get("language", "en"),
            )
            await websocket.send_json({"type": "response", "data": response.to_dict()})
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket, "chat")
