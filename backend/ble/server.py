"""
AgroMate BLE Server

Central BLE server that manages robot connections, message routing,
and data transfer between the laptop AI engine and ESP32 robots.
"""

import asyncio
import logging
import struct
import zlib
from typing import Optional, Callable

try:
    from bleak import BleakClient, BleakScanner
    from bleak.backends.device import BLEDevice

    BLEAK_AVAILABLE = True
except ImportError:
    BLEAK_AVAILABLE = False

from .protocol import (
    BLE_CHAR_UUIDS,
    BLE_SERVICE_UUIDS,
    BLEMessage,
    MessageType,
    ChunkTransferState,
    build_ack,
    build_face_command,
    build_ai_response,
    build_capture_request,
    build_record_request,
    build_chunk_start,
    build_chunk_end,
    FaceExpression,
    MAX_PAYLOAD_SIZE,
)
from .connection import ConnectionManager, ConnectionState, RobotConnection
from .discovery import RobotDiscovery

logger = logging.getLogger(__name__)


class BLEServer:
    """
    AgroMate BLE Server

    Acts as the central hub connecting to ESP32 robots via BLE.
    The laptop acts as a BLE central device that connects to
    ESP32 peripherals advertising the AgroMate service.

    Architecture note: Despite the name "server", the laptop operates as a
    BLE Central (client in BLE terms) connecting to ESP32 Peripherals.
    It's called "server" because it serves as the AI processing hub.
    """

    def __init__(
        self,
        max_robots: int = 5,
        scan_timeout: float = 10.0,
        heartbeat_interval: float = 5.0,
        auto_reconnect: bool = True,
    ):
        self.auto_reconnect = auto_reconnect
        self.connection_manager = ConnectionManager(
            max_robots=max_robots,
            heartbeat_interval=heartbeat_interval,
        )
        self.discovery = RobotDiscovery(
            connection_manager=self.connection_manager,
            scan_timeout=scan_timeout,
        )

        # Active transfers tracking
        self._active_transfers: dict[str, dict[int, ChunkTransferState]] = {}

        # Event handlers for AI pipeline integration
        self._image_handlers: list[Callable] = []
        self._audio_handlers: list[Callable] = []
        self._sensor_handlers: list[Callable] = []
        self._button_handlers: list[Callable] = []

        # Running state
        self._running = False
        self._connection_tasks: dict[str, asyncio.Task] = {}

        # Setup internal event handlers
        self._setup_events()

    def _setup_events(self):
        """Register internal event handlers on the connection manager."""
        self.discovery.on_robot_discovered(self._on_robot_discovered)
        self.connection_manager.on("robot_discovered", self._on_robot_discovered)
        self.connection_manager.on("robot_disconnected", self._on_robot_disconnected)
        self.connection_manager.on("robot_reconnecting", self._on_robot_reconnecting)
        self.connection_manager.on("image_chunk", self._on_image_chunk)
        self.connection_manager.on("audio_chunk", self._on_audio_chunk)
        self.connection_manager.on("sensor_data", self._on_sensor_data)
        self.connection_manager.on("button_event", self._on_button_event)
        self.connection_manager.on("chunk_start", self._on_chunk_start)
        self.connection_manager.on("chunk_end", self._on_chunk_end)

    # --- Public API ---

    async def start(self):
        """Start the BLE server: begin scanning and heartbeat monitoring."""
        if not BLEAK_AVAILABLE:
            logger.error(
                "Cannot start BLE server: bleak not installed. "
                "Install with: pip install bleak"
            )
            return

        self._running = True
        logger.info("🔵 AgroMate BLE Server starting...")

        # Start heartbeat monitor
        await self.connection_manager.start_heartbeat_monitor()

        # Start continuous scanning
        await self.discovery.start_continuous_scan()

        logger.info("🔵 AgroMate BLE Server started successfully")

    async def stop(self):
        """Stop the BLE server gracefully."""
        self._running = False
        logger.info("🔵 AgroMate BLE Server stopping...")

        # Stop scanning
        await self.discovery.stop_continuous_scan()

        # Stop heartbeat monitor
        await self.connection_manager.stop_heartbeat_monitor()

        # Disconnect all robots
        for robot_id in list(self._connection_tasks.keys()):
            task = self._connection_tasks.pop(robot_id, None)
            if task:
                task.cancel()

        for robot in self.connection_manager.robots.values():
            if robot._client and robot._client.is_connected:
                try:
                    await robot._client.disconnect()
                except Exception as e:
                    logger.error(f"Error disconnecting {robot.name}: {e}")

        logger.info("🔵 AgroMate BLE Server stopped")

    async def connect_to_robot(self, robot_id: str) -> bool:
        """Initiate a BLE connection to a discovered robot."""
        robot = self.connection_manager.get_robot(robot_id)
        if not robot:
            logger.error(f"Robot {robot_id} not found")
            return False

        # Ignore direct BLE connections to pseudo-MAC addresses registered by Android app sync
        if ":" not in robot.mac_address and "-" not in robot.mac_address:
            logger.info(
                f"Ignoring direct BLE connection to pseudonymous robot address: {robot.mac_address}. "
                "This device is managed by the Android app bridge."
            )
            return False

        if robot.state in (ConnectionState.ACTIVE, ConnectionState.CONNECTING):
            logger.info(
                f"Robot {robot.name} already in active or connecting state "
                f"({robot.state.value})"
            )
            return True

        robot.transition_to(ConnectionState.CONNECTING)

        was_scanning = False
        try:
            # Stop background scanning to prevent Windows BLE stack locks
            if self.discovery and self.discovery.is_scanning:
                logger.info("Stopping background scanner for connection...")
                was_scanning = True
                await self.discovery.stop_continuous_scan()
                # Give the Windows BLE stack a moment to settle
                await asyncio.sleep(0.5)

            target = robot._ble_device if robot._ble_device else robot.mac_address
            client = BleakClient(
                target,
                disconnected_callback=lambda c: self._handle_disconnect(robot_id),
            )
            await client.connect(timeout=15.0)

            if client.is_connected:
                # Wait for the connection to stabilize (critical for Windows BLE stack)
                await asyncio.sleep(1.0)
                
                robot._client = client
                robot.transition_to(ConnectionState.PAIRED)

                # Subscribe to notifications
                try:
                    await self._subscribe_notifications(robot)
                    robot.transition_to(ConnectionState.ACTIVE)
                    robot.update_heartbeat()

                    # Flush queued messages
                    await self._flush_queue(robot)

                    logger.info(
                        f"✅ Connected to {robot.name} ({robot.mac_address})"
                    )
                    # Automatically trigger a weather sync to update the robot's dashboard
                    asyncio.create_task(self._auto_sync_weather(robot.robot_id))
                    return True
                except Exception as e:
                    logger.error(f"Failed to subscribe to notifications for {robot.name}: {e}")
                    try:
                        await client.disconnect()
                    except Exception:
                        pass
                    robot._client = None
                    robot.transition_to(ConnectionState.DISCONNECTED)
                    return False
            else:
                robot.transition_to(ConnectionState.DISCONNECTED)
                return False

        except Exception as e:
            logger.error(f"Connection failed for {robot.name}: {e}")
            robot.transition_to(ConnectionState.DISCONNECTED)

            if self.auto_reconnect:
                asyncio.create_task(
                    self.connection_manager._reconnect(robot)
                )
            return False

        finally:
            if was_scanning:
                logger.info("Restarting background scanner...")
                await self.discovery.start_continuous_scan()

    async def disconnect_robot(self, robot_id: str):
        """Disconnect a specific robot."""
        robot = self.connection_manager.get_robot(robot_id)
        if not robot:
            return

        if robot._client and robot._client.is_connected:
            await robot._client.disconnect()

        robot.transition_to(ConnectionState.DISCONNECTED)
        logger.info(f"Disconnected from {robot.name}")

    # --- Message Sending ---

    async def send_ai_response(
        self,
        robot_id: str,
        text: str,
        expression: FaceExpression = FaceExpression.TALKING,
    ):
        """Send an AI response text to the robot for display/speech."""
        msg = build_ai_response(text, expression)
        await self.connection_manager.send_message(robot_id, msg)

    async def set_face_expression(
        self, robot_id: str, expression: FaceExpression, intensity: int = 100
    ):
        """Set the robot's face expression."""
        msg = build_face_command(expression, intensity)
        await self.connection_manager.send_message(robot_id, msg)

    async def request_camera_capture(self, robot_id: str, resolution: int = 1):
        """Request the robot to capture a camera image."""
        msg = build_capture_request(resolution)
        await self.connection_manager.send_message(robot_id, msg)

    async def request_audio_recording(
        self, robot_id: str, duration_ms: int = 5000
    ):
        """Request the robot to record audio from microphone."""
        msg = build_record_request(duration_ms)
        await self.connection_manager.send_message(robot_id, msg)

    async def send_speech_audio(self, robot_id: str, audio_data: bytes):
        """Send TTS audio data to robot for playback via chunked transfer."""
        await self._send_chunked(robot_id, audio_data, MessageType.SPEECH_AUDIO)

    async def set_screen_mode(self, robot_id: str, screen_mode: int):
        """Set the robot's active screen mode (0=FACE, 1=DASHBOARD)."""
        from .protocol import build_status_update
        msg = build_status_update(screen_mode)
        await self.connection_manager.send_message(robot_id, msg)

    async def send_weather_update(self, robot_id: str, temperature: float, humidity: int, forecast: str):
        """Send weather data to the robot's dashboard."""
        from .protocol import build_weather_update
        msg = build_weather_update(temperature, humidity, forecast)
        await self.connection_manager.send_message(robot_id, msg)

    async def send_disease_notify(self, robot_id: str):
        """Notify the robot that a disease was detected (increments counter)."""
        from .protocol import build_disease_notify
        msg = build_disease_notify()
        await self.connection_manager.send_message(robot_id, msg)

    async def send_stats_sync(self, robot_id: str, total_scans: int, diseases_found: int):
        """Send database scan statistics to the robot."""
        from .protocol import build_stats_sync
        msg = build_stats_sync(total_scans, diseases_found)
        await self.connection_manager.send_message(robot_id, msg)

    async def _send_chunked(
        self, robot_id: str, data: bytes, data_type: MessageType
    ):
        """Send large data via chunked transfer protocol."""
        if not data:
            total_chunks = 0
            chunk_size = 0
        else:
            # Limit chunk size to 480 bytes so the 522-byte frame does not get truncated on Windows.
            # Align with the ESP32's dynamic chunk size division logic.
            max_chunk_size = 480
            total_chunks = (len(data) + max_chunk_size - 1) // max_chunk_size
            chunk_size = (len(data) + total_chunks - 1) // total_chunks

        transfer_id = hash(data) & 0xFFFFFFFF
        checksum = zlib.crc32(data) & 0xFFFFFFFF

        # Send CHUNK_START
        start_msg = build_chunk_start(transfer_id, len(data), total_chunks, data_type)
        await self.connection_manager.send_message(robot_id, start_msg)
        await asyncio.sleep(0.01)

        # Send data chunks
        for i in range(total_chunks):
            offset = i * chunk_size
            chunk = data[offset : offset + chunk_size]

            # Prepend chunk index to payload
            payload = struct.pack("<IH", transfer_id, i) + chunk
            msg = BLEMessage(msg_type=data_type, payload=payload)
            
            # Use Write-Without-Response (response=False) for high-speed audio data chunks
            # Keep control/handshake frames (start/end) with response=True for guaranteed delivery
            use_response = (data_type != MessageType.SPEECH_AUDIO)
            await self.connection_manager.send_message(robot_id, msg, response=use_response)

            # Small delay to pace the transmission and avoid overflow in ESP32 ring buffer
            await asyncio.sleep(0.008)

        # Send CHUNK_END
        end_msg = build_chunk_end(transfer_id, checksum)
        await self.connection_manager.send_message(robot_id, end_msg)

    # --- Event Handler Registration ---

    def on_image_received(self, handler: Callable):
        """Register handler for when a complete image is received from a robot."""
        self._image_handlers.append(handler)

    def on_audio_received(self, handler: Callable):
        """Register handler for when complete audio is received from a robot."""
        self._audio_handlers.append(handler)

    def on_sensor_update(self, handler: Callable):
        """Register handler for sensor data updates."""
        self._sensor_handlers.append(handler)

    def on_button_press(self, handler: Callable):
        """Register handler for button press events."""
        self._button_handlers.append(handler)

    # --- Internal Event Handlers ---

    def _on_robot_discovered(self, robot: RobotConnection):
        """Auto-connect when a new robot is discovered."""
        if self._running and self.auto_reconnect:
            task = asyncio.create_task(self.connect_to_robot(robot.robot_id))
            self._connection_tasks[robot.robot_id] = task

    def _on_robot_reconnecting(self, robot: RobotConnection):
        """Handle robot reconnection attempt."""
        if self._running:
            task = asyncio.create_task(self.connect_to_robot(robot.robot_id))
            self._connection_tasks[robot.robot_id] = task

    def _on_robot_disconnected(self, robot: RobotConnection):
        """Handle robot disconnection."""
        logger.warning(f"⚠️ Robot {robot.name} disconnected")
        # Clean up any active transfers
        self._active_transfers.pop(robot.robot_id, None)

    def _handle_disconnect(self, robot_id: str):
        """BleakClient disconnect callback."""
        robot = self.connection_manager.get_robot(robot_id)
        if robot and robot.state == ConnectionState.ACTIVE:
            robot.transition_to(ConnectionState.DISCONNECTED)
            self.connection_manager._emit("robot_disconnected", robot)

            if self.auto_reconnect and self._running:
                asyncio.create_task(
                    self.connection_manager._reconnect(robot)
                )

    def _on_chunk_start(
        self, robot: RobotConnection, transfer_id: int, total_size: int, total_chunks: int, data_type: int
    ):
        """Handle incoming chunk transfer start."""
        robot_id = robot.robot_id
        if robot_id not in self._active_transfers:
            self._active_transfers[robot_id] = {}
        
        self._active_transfers[robot_id][transfer_id] = ChunkTransferState(
            transfer_id=transfer_id,
            total_size=total_size,
            total_chunks=total_chunks,
            data_type=data_type,
        )
        logger.info(
            f"📥 Chunk transfer started from {robot.name}: id={transfer_id}, "
            f"size={total_size} bytes, chunks={total_chunks}, type=0x{data_type:02X}"
        )

    def _on_chunk_end(
        self, robot: RobotConnection, transfer_id: int, checksum: int
    ):
        """Handle incoming chunk transfer end."""
        robot_id = robot.robot_id
        transfers = self._active_transfers.get(robot_id, {})
        if transfer_id not in transfers:
            logger.warning(f"Received CHUNK_END for unknown transfer {transfer_id} from {robot.name}")
            return

        state = transfers[transfer_id]
        if state.validate_and_complete(checksum):
            data = bytes(state.data)
            del transfers[transfer_id]

            if state.data_type == MessageType.IMAGE_CHUNK or state.data_type == MessageType.IMAGE_CHUNK.value:
                # Notify image handlers
                for handler in self._image_handlers:
                    try:
                        result = handler(robot, data)
                        if asyncio.iscoroutine(result):
                            asyncio.create_task(result)
                    except Exception as e:
                        logger.error(f"Image handler error: {e}")
            elif state.data_type == MessageType.AUDIO_CHUNK or state.data_type == MessageType.AUDIO_CHUNK.value:
                # Notify audio handlers
                for handler in self._audio_handlers:
                    try:
                        result = handler(robot, data)
                        if asyncio.iscoroutine(result):
                            asyncio.create_task(result)
                    except Exception as e:
                        logger.error(f"Audio handler error: {e}")

    def _on_image_chunk(
        self, robot: RobotConnection, chunk_data: bytes, sequence: int
    ):
        """Handle incoming image chunk."""
        robot_id = robot.robot_id

        if robot_id not in self._active_transfers:
            self._active_transfers[robot_id] = {}

        # Parse transfer_id and chunk index from payload
        if len(chunk_data) < 6:
            return

        transfer_id, chunk_index = struct.unpack("<IH", chunk_data[:6])
        actual_data = chunk_data[6:]

        transfers = self._active_transfers[robot_id]
        if transfer_id not in transfers:
            transfers[transfer_id] = ChunkTransferState(
                transfer_id=transfer_id,
                data_type=MessageType.IMAGE_CHUNK
            )

        state = transfers[transfer_id]
        state.add_chunk(actual_data, chunk_index)

    def _on_audio_chunk(
        self, robot: RobotConnection, chunk_data: bytes, sequence: int
    ):
        """Handle incoming audio chunk."""
        robot_id = robot.robot_id

        if robot_id not in self._active_transfers:
            self._active_transfers[robot_id] = {}

        if len(chunk_data) < 6:
            return

        transfer_id, chunk_index = struct.unpack("<IH", chunk_data[:6])
        actual_data = chunk_data[6:]

        transfers = self._active_transfers[robot_id]
        if transfer_id not in transfers:
            transfers[transfer_id] = ChunkTransferState(
                transfer_id=transfer_id,
                data_type=MessageType.AUDIO_CHUNK
            )

        state = transfers[transfer_id]
        state.add_chunk(actual_data, chunk_index)

    def _on_sensor_data(self, robot: RobotConnection, data: dict):
        """Forward sensor data to registered handlers."""
        for handler in self._sensor_handlers:
            try:
                result = handler(robot, data)
                if asyncio.iscoroutine(result):
                    asyncio.create_task(result)
            except Exception as e:
                logger.error(f"Sensor handler error: {e}")

    def _on_button_event(self, robot: RobotConnection, payload: bytes):
        """Forward button events to registered handlers."""
        for handler in self._button_handlers:
            try:
                result = handler(robot, payload)
                if asyncio.iscoroutine(result):
                    asyncio.create_task(result)
            except Exception as e:
                logger.error(f"Button handler error: {e}")

    async def _subscribe_notifications(self, robot: RobotConnection):
        """Subscribe to BLE notifications from a connected robot."""
        client = robot._client
        if not client:
            return

        logger.info(f"Discovering services for {robot.name} ({robot.mac_address})...")
        await client.get_services()

        async def _notification_handler(sender, data: bytearray):
            """Process incoming BLE notification data."""
            msg = BLEMessage.decode(bytes(data))
            if msg:
                self.connection_manager.handle_message(robot.robot_id, msg)

        # Subscribe to command responses
        await client.start_notify(
            BLE_CHAR_UUIDS["cmd_rx"], _notification_handler
        )
        await asyncio.sleep(0.15)

        # Subscribe to sensor notifications
        await client.start_notify(
            BLE_CHAR_UUIDS["sensors"], _notification_handler
        )
        await asyncio.sleep(0.15)

        # Subscribe to image notifications
        await client.start_notify(
            BLE_CHAR_UUIDS["img_tx"], _notification_handler
        )
        await asyncio.sleep(0.15)

        # Subscribe to audio notifications
        await client.start_notify(
            BLE_CHAR_UUIDS["audio_tx"], _notification_handler
        )
        await asyncio.sleep(0.15)

        # Subscribe to heartbeat
        await client.start_notify(
            BLE_CHAR_UUIDS["heartbeat"], _notification_handler
        )

    async def _flush_queue(self, robot: RobotConnection):
        """Send all queued messages for a robot."""
        while robot.message_queue:
            if robot.state != ConnectionState.ACTIVE:
                break
            msg = robot.message_queue.popleft()
            success = await self.connection_manager.send_message(robot.robot_id, msg)
            if not success:
                robot.message_queue.appendleft(msg)
                break
            await asyncio.sleep(0.01)

    # --- Status ---

    def get_status(self) -> dict:
        """Get complete BLE server status for API responses."""
        return {
            "running": self._running,
            "bluetooth": self.discovery.get_bluetooth_info(),
            "robots": self.connection_manager.get_all_status(),
            "active_transfers": {
                rid: len(transfers)
                for rid, transfers in self._active_transfers.items()
            },
        }

    async def _auto_sync_weather(self, robot_id: str):
        """Automatically fetch and push current weather and stats to robot upon connection."""
        try:
            # 1. Sync Weather
            from ..services.weather import WeatherService
            weather_svc = WeatherService()
            # Default coordinates for sync (can be updated by user)
            weather_data = await weather_svc.get_current_weather(28.6139, 77.2090)
            if weather_data:
                temp = weather_data.get("temperature", 25.0)
                humidity = weather_data.get("humidity", 60)
                forecast = weather_data.get("description", "Clear").title()
                await self.send_weather_update(robot_id, temp, humidity, forecast)
                logger.info(f"⛅ Auto-synced weather on connection for robot: {robot_id}")
            
            # 2. Sync Stats from DB (30 days to match dashboard)
            from ..database.crud import get_session, get_analytics_summary
            async with await get_session() as session:
                summary = await get_analytics_summary(session, days=30)
                if summary:
                    total_scans = summary.get("total_scans", 0)
                    total_diagnoses = summary.get("total_diagnoses", 0)
                    await self.send_stats_sync(robot_id, total_scans, total_diagnoses)
                    logger.info(f"📊 Auto-synced stats on connection for robot: {robot_id} (Scans={total_scans}, Diseases={total_diagnoses})")
        except Exception as e:
            logger.error(f"Failed to auto-sync weather and stats: {e}")
