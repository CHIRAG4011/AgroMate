"""
AgroMate BLE Server

Central BLE server that manages robot connections, message routing,
and data transfer between the laptop AI engine and ESP32 robots.
"""

import asyncio
import time
import logging
import struct
import sys
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
        self._error_handlers: list[Callable] = []

        # Running state
        self._running = False
        self._connection_tasks: dict[str, asyncio.Task] = {}
        self._connect_locks: dict[str, asyncio.Lock] = {}
        self._last_intro_voice_time: float = 0.0

        # Setup internal event handlers
        self._setup_events()

    def _setup_events(self):
        """Register internal event handlers on the connection manager."""
        self.discovery.on_robot_discovered(self._on_robot_discovered)
        self.connection_manager.on("robot_disconnected", self._on_robot_disconnected)
        self.connection_manager.on("robot_reconnecting", self._on_robot_reconnecting)
        self.connection_manager.on("image_chunk", self._on_image_chunk)
        self.connection_manager.on("audio_chunk", self._on_audio_chunk)
        self.connection_manager.on("sensor_data", self._on_sensor_data)
        self.connection_manager.on("button_event", self._on_button_event)
        self.connection_manager.on("chunk_start", self._on_chunk_start)
        self.connection_manager.on("chunk_end", self._on_chunk_end)
        self.connection_manager.on("error_report", self._on_error_report)

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

        if robot_id not in self._connect_locks:
            self._connect_locks[robot_id] = asyncio.Lock()

        async with self._connect_locks[robot_id]:
            if robot._client and robot._client.is_connected:
                if robot.state != ConnectionState.ACTIVE:
                    robot.transition_to(ConnectionState.ACTIVE)
                logger.info(
                    f"Robot {robot.name} already connected and active"
                )
                return True

            # Ensure any lingering disconnected client is fully closed
            if robot._client:
                try:
                    await robot._client.disconnect()
                except Exception:
                    pass
                robot._client = None

            robot.transition_to(ConnectionState.CONNECTING)

            was_scanning = False
            try:
                # Stop background scanning to prevent Windows BLE stack locks
                if self.discovery and self.discovery.is_scanning:
                    logger.info("Stopping background scanner for connection...")
                    was_scanning = True
                    await self.discovery.stop_continuous_scan()
                    # Give the Windows BLE stack a moment to settle
                    await asyncio.sleep(0.8)

                target = robot._ble_device or robot.mac_address
                if sys.platform == "win32":
                    try:
                        dev = await BleakScanner.find_device_by_address(robot.mac_address, timeout=6.0)
                        if not dev:
                            await asyncio.sleep(0.5)
                            dev = await BleakScanner.find_device_by_filter(
                                lambda d, ad: (d.name and d.name.startswith("AgroMate")) or (ad.local_name and ad.local_name.startswith("AgroMate")),
                                timeout=7.0
                            )
                        if dev:
                            target = dev
                            robot._ble_device = dev
                            robot.mac_address = dev.address
                    except Exception as scan_err:
                        logger.debug(f"Device resolve error: {scan_err}")

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
                        try:
                            from ..voice.languages import get_system_language
                            if not hasattr(robot, "config") or robot.config is None:
                                robot.config = {}
                            robot.config["language"] = get_system_language()
                        except Exception:
                            pass

                        # Flush queued messages
                        await self._flush_queue(robot)

                        logger.info(
                            f"✅ Connected to {robot.name} ({robot.mac_address})"
                        )
                        # Automatically trigger a weather sync to update the robot's dashboard
                        asyncio.create_task(self._auto_sync_weather(robot.robot_id))
                        # Automatically play Eva intro voice on Bluetooth speaker upon connection
                        asyncio.create_task(self._play_intro_voice())
                        return True
                    except Exception as e:
                        err_str = f"Subscribe error: {type(e).__name__}: {e}"
                        robot.last_error = err_str
                        logger.error(f"Failed to subscribe to notifications for {robot.name}: {err_str}")
                        try:
                            await client.disconnect()
                        except Exception:
                            pass
                        robot._client = None
                        robot.transition_to(ConnectionState.DISCONNECTED)
                        return False
                else:
                    robot.last_error = "BleakClient connect returned is_connected=False"
                    robot.transition_to(ConnectionState.DISCONNECTED)
                    return False

            except Exception as e:
                err_str = f"Connect error: {type(e).__name__}: {e}"
                robot.last_error = err_str
                logger.error(f"Connection failed for {robot.name}: {err_str}")
                if robot._client:
                    try:
                        await robot._client.disconnect()
                    except Exception:
                        pass
                    robot._client = None
                robot._ble_device = None
                robot.transition_to(ConnectionState.DISCONNECTED)

                if self.auto_reconnect:
                    asyncio.create_task(
                        self.connection_manager._reconnect(robot)
                    )
                return False

            finally:
                if was_scanning:
                    has_active = any(
                        r.state == ConnectionState.ACTIVE
                        for r in self.connection_manager._robots.values()
                    )
                    if not has_active or sys.platform != "win32":
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

    async def request_camera_capture(self, robot_id: str, resolution: int = 1) -> bool:
        """Request the robot to capture a camera image."""
        robot = self.connection_manager.get_robot(robot_id)
        if not robot:
            logger.error(f"request_camera_capture: Robot {robot_id} not found")
            return False
        robot_id = robot.robot_id

        if not robot._client or not robot._client.is_connected:
            logger.info(f"Robot {robot_id} not connected, connecting before capture request...")
            connected = await self.connect_to_robot(robot_id)
            if not connected:
                logger.error(f"Could not connect to {robot_id} to send capture request")
                return False

        msg = build_capture_request(resolution)
        sent = await self.connection_manager.send_message(robot_id, msg)
        logger.info(f"Camera capture command sent to {robot.name}: {sent}")
        return sent

    async def request_audio_recording(
        self, robot_id: str, duration_ms: int = 5000
    ):
        """Request the robot to record audio from microphone."""
        msg = build_record_request(duration_ms)
        await self.connection_manager.send_message(robot_id, msg)

    async def send_speech_audio(self, robot_id: str, audio_data: bytes, play_local_speaker: bool = True):
        """
        Send TTS audio data:
        1. FIRST play out loud on laptop / connected Bluetooth speaker, and WAIT until it finishes.
        2. THEN stream chunked audio over BLE to the robot speaker.
        """
        # 1. First: Play out loud through laptop / Bluetooth speaker and AWAIT full playback
        if play_local_speaker and audio_data:
            try:
                logger.info(f"🔊 [Step 1/2] Playing speech on laptop/Bluetooth speaker ({len(audio_data)} bytes)...")
                from ..utils.audio_utils import play_audio_and_cleanup_async
                await play_audio_and_cleanup_async(audio_data, audio_format="pcm", sample_rate=16000)
                logger.info("✅ [Step 1/2] Laptop/Bluetooth playback finished.")
            except Exception as e:
                logger.warning(f"Could not play audio to Bluetooth/system speaker: {e}")

        # Small 250ms pause between laptop playback and BLE transfer for smooth transition
        await asyncio.sleep(0.25)

        # 2. Then: Stream chunked audio over BLE to the robot speaker
        logger.info(f"📡 [Step 2/2] Now streaming audio over BLE to robot {robot_id}...")
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

            # Pace transmission to allow ESP32 ring buffer to drain smoothly
            # 8ms pacing for audio gives ~62KB/s throughput (16kHz mono needs 32KB/s)
            chunk_delay = 0.008 if data_type == MessageType.SPEECH_AUDIO else 0.008
            await asyncio.sleep(chunk_delay)

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

    def on_error_report(self, handler: Callable):
        """Register handler for robot error reports."""
        self._error_handlers.append(handler)

    def _on_error_report(self, robot: RobotConnection, err_msg: str):
        """Handle error report from robot."""
        for handler in self._error_handlers:
            try:
                res = handler(robot, err_msg)
                if asyncio.iscoroutine(res):
                    asyncio.create_task(res)
            except Exception as e:
                logger.error(f"Error report handler error: {e}")

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
        if robot:
            robot._client = None
            robot._ble_device = None
            if robot.state == ConnectionState.ACTIVE:
                robot.transition_to(ConnectionState.DISCONNECTED)
                self.connection_manager._emit("robot_disconnected", robot)

            if self.auto_reconnect and self._running:
                asyncio.create_task(
                    self.connection_manager._reconnect(robot)
                )
            if self.discovery and not self.discovery.is_scanning:
                asyncio.create_task(self.discovery.start_continuous_scan())

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

            is_image = (state.data_type in (MessageType.IMAGE_CHUNK, MessageType.IMAGE_CHUNK.value, 1, 0x01) or (data and data.startswith(b"\xff\xd8")))
            is_audio = (state.data_type in (MessageType.AUDIO_CHUNK, MessageType.AUDIO_CHUNK.value, 2, 0x02) or (data and data.startswith(b"RIFF")))

            if is_image:
                logger.info(
                    f"📸 Complete image received from {robot.name}: "
                    f"{len(data)} bytes (JPEG={data[:2] == b'\xff\xd8'})"
                )
                # Notify image handlers
                for handler in self._image_handlers:
                    try:
                        result = handler(robot, data)
                        if asyncio.iscoroutine(result):
                            asyncio.create_task(result)
                    except Exception as e:
                        logger.error(f"Image handler error: {e}")
            elif is_audio:
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
        
        # Log progress periodically (every 10 chunks or first/last)
        if state.received_chunks == 1 or state.received_chunks % 10 == 0 or (
            state.total_chunks > 0 and state.received_chunks >= state.total_chunks
        ):
            logger.info(
                f"📥 Image chunk from {robot.name}: #{chunk_index} "
                f"({state.received_chunks}/{state.total_chunks or '?'} chunks, "
                f"{len(actual_data)} bytes this chunk)"
            )

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
        cmd_tx_uuid = BLE_CHAR_UUIDS.get("cmd_tx")
        
        # Ensure services are loaded from peripheral
        if not client.services:
            try:
                await client.get_services()
            except Exception as e:
                logger.warning(f"get_services error: {e}")

        # Locate cmd_tx characteristic (with case-insensitive fallback)
        char = client.services.get_characteristic(cmd_tx_uuid) if client.services else None
        if not char and client.services:
            for s in client.services:
                for c in s.characteristics:
                    if c.uuid.lower() == cmd_tx_uuid.lower():
                        char = c
                        break
                if char:
                    break

        if not char:
            found_uuids = [s.uuid for s in client.services] if client.services else []
            raise RuntimeError(f"Critical GATT characteristic {cmd_tx_uuid} not discovered on {robot.name} (found services: {found_uuids})")

        def _notification_handler(sender, data: bytearray):
            """Process incoming BLE notification data."""
            try:
                msg = BLEMessage.decode(bytes(data))
                if msg:
                    self.connection_manager.handle_message(robot.robot_id, msg)
                else:
                    logger.debug(f"Could not decode BLE frame from {sender}: {len(data)} bytes")
            except Exception as e:
                logger.error(f"Error handling BLE notification from {robot.name}: {e}")

        # Subscribe to notifications safely
        for char_key in ["cmd_rx", "sensors", "img_tx", "audio_tx", "heartbeat"]:
            char_uuid = BLE_CHAR_UUIDS.get(char_key)
            if not char_uuid:
                continue
            char = client.services.get_characteristic(char_uuid)
            if char:
                try:
                    await client.start_notify(char_uuid, _notification_handler)
                    await asyncio.sleep(0.1)
                    logger.info(f"Subscribed to {char_key} ({char_uuid})")
                except Exception as e:
                    logger.warning(f"Could not subscribe to {char_key} ({char_uuid}): {e}")
            else:
                logger.warning(f"Characteristic {char_key} ({char_uuid}) not found in discovered services")

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
                await asyncio.sleep(0.15)
            
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


    async def _play_intro_voice(self, force: bool = False):
        """Play the Eva intro voice on Bluetooth / system speaker upon robot connection."""
        now = time.time()
        if not force and (now - self._last_intro_voice_time < 60.0):
            return
        self._last_intro_voice_time = now

        try:
            import os
            import soundfile as sf
            import io
            from ..utils.audio_utils import play_audio_and_cleanup_async

            base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            voice_file = os.path.join(
                base_dir,
                "Voices",
                "ElevenLabs_2026-09-19T05_03_50_Eva - Futuristic Robot Helper_pvc_sp100_s42_sb78_se77_b_m2.mp3",
            )
            if not os.path.exists(voice_file):
                logger.warning(f"Intro voice file not found at: {voice_file}")
                return

            logger.info(f"🔊 Playing intro voice on Bluetooth/system speaker: {os.path.basename(voice_file)}")
            data, sr = sf.read(voice_file)
            buf = io.BytesIO()
            sf.write(buf, data, sr, format="WAV")
            wav_bytes = buf.getvalue()
            await play_audio_and_cleanup_async(wav_bytes, audio_format="wav", sample_rate=sr)
        except Exception as e:
            logger.error(f"Failed to play intro voice on Bluetooth: {e}")
