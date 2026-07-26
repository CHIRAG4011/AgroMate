"""
AgroMate BLE Connection Manager

Manages individual robot connections with state machine tracking,
message queuing, and automatic reconnection.
"""

import asyncio
import enum
import time
import logging
from dataclasses import dataclass, field
from typing import Optional, Callable, Any
from collections import deque

from .protocol import BLEMessage, MessageType, build_ack

logger = logging.getLogger(__name__)


class ConnectionState(enum.Enum):
    """BLE connection state machine states."""

    DISCOVERED = "discovered"
    CONNECTING = "connecting"
    PAIRED = "paired"
    ACTIVE = "active"
    DISCONNECTED = "disconnected"
    RECONNECTING = "reconnecting"
    ERROR = "error"


# Valid state transitions
_VALID_TRANSITIONS = {
    ConnectionState.DISCOVERED: {ConnectionState.CONNECTING, ConnectionState.ERROR},
    ConnectionState.CONNECTING: {
        ConnectionState.PAIRED,
        ConnectionState.DISCONNECTED,
        ConnectionState.ERROR,
    },
    ConnectionState.PAIRED: {ConnectionState.ACTIVE, ConnectionState.DISCONNECTED},
    ConnectionState.ACTIVE: {ConnectionState.DISCONNECTED, ConnectionState.ERROR},
    ConnectionState.DISCONNECTED: {
        ConnectionState.CONNECTING,
        ConnectionState.RECONNECTING,
        ConnectionState.DISCOVERED,
        ConnectionState.ERROR,
    },
    ConnectionState.RECONNECTING: {
        ConnectionState.CONNECTING,
        ConnectionState.PAIRED,
        ConnectionState.DISCONNECTED,
        ConnectionState.ERROR,
    },
    ConnectionState.ERROR: {
        ConnectionState.CONNECTING,
        ConnectionState.DISCOVERED,
        ConnectionState.DISCONNECTED,
    },
}


@dataclass
class RobotConnection:
    """Represents a single connected ESP32 robot with its state and metadata."""

    robot_id: str
    name: str
    mac_address: str
    state: ConnectionState = ConnectionState.DISCOVERED
    rssi: int = -100
    battery_percent: int = 0
    is_charging: bool = False
    firmware_version: str = "unknown"
    last_heartbeat: float = 0.0
    connected_since: float = 0.0
    reconnect_attempts: int = 0
    max_reconnect_attempts: int = 10
    reconnect_delay: float = 1.0  # Starting delay, doubles each attempt
    max_reconnect_delay: float = 60.0
    message_queue: deque = field(default_factory=lambda: deque(maxlen=100))
    pending_acks: dict = field(default_factory=dict)
    sequence_counter: int = 0
    total_messages_sent: int = 0
    total_messages_received: int = 0
    total_bytes_sent: int = 0
    total_bytes_received: int = 0
    scans_today: int = 0
    config: dict = field(default_factory=lambda: {"language": "en"})
    _client: Any = None  # bleak.BleakClient reference
    _ble_device: Any = None  # bleak.backends.device.BLEDevice reference

    def transition_to(self, new_state: ConnectionState) -> bool:
        """Attempt a state transition. Returns True if valid."""
        if new_state in _VALID_TRANSITIONS.get(self.state, set()):
            old_state = self.state
            self.state = new_state

            if new_state == ConnectionState.ACTIVE:
                self.connected_since = time.time()
                self.reconnect_attempts = 0
                self.reconnect_delay = 1.0

            logger.info(
                f"Robot {self.name} ({self.mac_address}): "
                f"{old_state.value} -> {new_state.value}"
            )
            return True
        else:
            logger.warning(
                f"Invalid transition for {self.name}: "
                f"{self.state.value} -> {new_state.value}"
            )
            return False

    def next_sequence(self) -> int:
        """Get the next sequence number (wraps at 65535)."""
        self.sequence_counter = (self.sequence_counter + 1) % 65536
        return self.sequence_counter

    def enqueue_message(self, message: BLEMessage) -> bool:
        """Add a message to the outgoing queue."""
        if len(self.message_queue) >= self.message_queue.maxlen:
            logger.debug(f"Message queue full for robot {self.name}, dropping oldest")
            return False

        message.sequence = self.next_sequence()
        self.message_queue.append(message)
        return True

    def update_heartbeat(self):
        """Update the last heartbeat timestamp."""
        self.last_heartbeat = time.time()

    def update_sensor_data(self, data: dict):
        """Update robot sensor data from parsed sensor message."""
        self.battery_percent = data.get("battery_percent", self.battery_percent)
        self.is_charging = data.get("is_charging", self.is_charging)

    @property
    def is_alive(self) -> bool:
        """Check if the robot is responding (heartbeat within 15s)."""
        if self.last_heartbeat == 0:
            return self.state == ConnectionState.ACTIVE
        return (time.time() - self.last_heartbeat) < 15.0

    @property
    def connection_duration(self) -> float:
        """Get connection duration in seconds."""
        if self.connected_since == 0:
            return 0.0
        return time.time() - self.connected_since

    @property
    def next_reconnect_delay(self) -> float:
        """Calculate next reconnect delay with exponential backoff."""
        delay = self.reconnect_delay * (2 ** self.reconnect_attempts)
        return min(delay, self.max_reconnect_delay)

    def to_dict(self) -> dict:
        """Serialize robot connection info for API responses."""
        import datetime
        return {
            "robot_id": self.robot_id,
            "name": self.name,
            "mac_address": self.mac_address,
            "state": self.state.value,
            "connected": self.state == ConnectionState.ACTIVE,
            "rssi": self.rssi,
            "battery": self.battery_percent,
            "battery_percent": self.battery_percent,
            "is_charging": self.is_charging,
            "firmware_version": self.firmware_version,
            "is_alive": self.is_alive,
            "signal_strength": "Excellent" if self.rssi >= -60 else ("Good" if self.rssi >= -70 else ("Fair" if self.rssi >= -85 else "Poor")),
            "last_seen": datetime.datetime.fromtimestamp(self.last_heartbeat, datetime.timezone.utc).isoformat() if self.last_heartbeat > 0 else None,
            "connection_duration": round(self.connection_duration, 1),
            "messages_sent": self.total_messages_sent,
            "messages_received": self.total_messages_received,
            "scans_today": self.scans_today,
        }


class ConnectionManager:
    """
    Manages all active robot connections.

    Handles connection lifecycle, message routing, heartbeat monitoring,
    and automatic reconnection for multiple simultaneous robots.
    """

    def __init__(self, max_robots: int = 5, heartbeat_interval: float = 5.0):
        self.max_robots = max_robots
        self.heartbeat_interval = heartbeat_interval
        self._robots: dict[str, RobotConnection] = {}
        self._event_handlers: dict[str, list[Callable]] = {}
        self._heartbeat_task: Optional[asyncio.Task] = None
        self._running = False

    @property
    def robots(self) -> dict[str, RobotConnection]:
        """Get all tracked robots."""
        return self._robots.copy()

    @property
    def active_robots(self) -> list[RobotConnection]:
        """Get all actively connected robots."""
        return [
            r for r in self._robots.values()
            if r.state == ConnectionState.ACTIVE
        ]

    @property
    def robot_count(self) -> int:
        """Get total number of tracked robots."""
        return len(self._robots)

    def register_robot(
        self,
        robot_id: str,
        name: str,
        mac_address: str,
        rssi: int = -100,
        ble_device: Any = None,
    ) -> Optional[RobotConnection]:
        """Register a newly discovered robot."""
        if robot_id in self._robots:
            # Update existing robot
            robot = self._robots[robot_id]
            robot.rssi = rssi
            if ble_device:
                robot._ble_device = ble_device
            return robot

        if len(self._robots) >= self.max_robots:
            logger.warning(
                f"Max robots ({self.max_robots}) reached, cannot register {name}"
            )
            return None

        robot = RobotConnection(
            robot_id=robot_id,
            name=name,
            mac_address=mac_address,
            rssi=rssi,
            _ble_device=ble_device,
        )
        self._robots[robot_id] = robot
        logger.info(f"Registered robot: {name} ({mac_address}) RSSI={rssi}")
        self._emit("robot_discovered", robot)
        return robot

    def unregister_robot(self, robot_id: str):
        """Remove a robot from tracking."""
        if robot_id in self._robots:
            robot = self._robots.pop(robot_id)
            logger.info(f"Unregistered robot: {robot.name}")
            self._emit("robot_removed", robot)

    def get_robot(self, robot_id: str) -> Optional[RobotConnection]:
        """Get a robot connection by ID."""
        return self._robots.get(robot_id)

    def get_robot_by_mac(self, mac_address: str) -> Optional[RobotConnection]:
        """Get a robot connection by MAC address."""
        for robot in self._robots.values():
            if robot.mac_address.lower() == mac_address.lower():
                return robot
        return None

    async def send_message(self, robot_id: str, message: BLEMessage, response: bool = True) -> bool:
        """Send a BLE message to a specific robot."""
        robot = self._robots.get(robot_id)
        if not robot:
            logger.warning(f"Robot {robot_id} not found")
            return False

        if robot.state != ConnectionState.ACTIVE:
            logger.debug(f"Robot {robot.name} not active, queueing message")
            return robot.enqueue_message(message)

        try:
            if robot._client and robot._client.is_connected:
                from .protocol import BLE_CHAR_UUIDS, MessageType

                # Route speech audio chunk messages to the dedicated audio RX characteristic
                char_key = "cmd_tx"
                if message.msg_type == MessageType.SPEECH_AUDIO:
                    char_key = "audio_rx"

                encoded = message.encode()
                await robot._client.write_gatt_char(
                    BLE_CHAR_UUIDS[char_key],
                    encoded,
                    response=response,
                )
                robot.total_messages_sent += 1
                robot.total_bytes_sent += len(encoded)
                return True
        except Exception as e:
            logger.error(f"Failed to send to {robot.name}: {e}")
            robot.transition_to(ConnectionState.DISCONNECTED)
            self._emit("robot_disconnected", robot)
            return False

        return False

    async def broadcast_message(self, message: BLEMessage):
        """Send a message to all active robots."""
        tasks = [
            self.send_message(robot_id, message)
            for robot_id, robot in self._robots.items()
            if robot.state == ConnectionState.ACTIVE
        ]
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

    def handle_message(self, robot_id: str, message: BLEMessage):
        """Process an incoming message from a robot."""
        robot = self._robots.get(robot_id)
        if not robot:
            logger.warning(f"Message from unknown robot: {robot_id}")
            return

        robot.update_heartbeat()
        robot.total_messages_received += 1
        robot.total_bytes_received += len(message.payload) + 11

        logger.debug(f"Received BLE message from {robot.name}: type=0x{message.msg_type:02X}, size={len(message.payload)} bytes")

        if message.msg_type == MessageType.HEARTBEAT:
            self._emit("heartbeat", robot)

        elif message.msg_type == MessageType.SENSOR_DATA:
            from .protocol import parse_sensor_data

            data = parse_sensor_data(message.payload)
            robot.update_sensor_data(data)
            self._emit("sensor_data", robot, data)

        elif message.msg_type == MessageType.IMAGE_CHUNK:
            logger.info(f"Received IMAGE_CHUNK frame, payload size={len(message.payload)} bytes")
            self._emit("image_chunk", robot, message.payload, message.sequence)

        elif message.msg_type == MessageType.AUDIO_CHUNK:
            self._emit("audio_chunk", robot, message.payload, message.sequence)

        elif message.msg_type == MessageType.BUTTON_EVENT:
            self._emit("button_event", robot, message.payload)

        elif message.msg_type == MessageType.TOUCH_EVENT:
            self._emit("touch_event", robot, message.payload)

        elif message.msg_type == MessageType.CAMERA_CAPTURE_DONE:
            self._emit("capture_done", robot)

        elif message.msg_type == MessageType.ACK:
            seq = int.from_bytes(message.payload[:2], "little") if message.payload else 0
            robot.pending_acks.pop(seq, None)

        elif message.msg_type == MessageType.CHUNK_START:
            import struct
            if len(message.payload) >= 13:
                transfer_id, total_size, total_chunks, data_type = struct.unpack("<IIIB", message.payload[:13])
                logger.info(f"Received CHUNK_START notification: id={transfer_id}, size={total_size}, chunks={total_chunks}, type=0x{data_type:02X}")
                self._emit("chunk_start", robot, transfer_id, total_size, total_chunks, data_type)

        elif message.msg_type == MessageType.CHUNK_END:
            import struct
            if len(message.payload) >= 8:
                transfer_id, checksum = struct.unpack("<II", message.payload[:8])
                logger.info(f"Received CHUNK_END notification: id={transfer_id}, checksum=0x{checksum:08X}")
                self._emit("chunk_end", robot, transfer_id, checksum)

        elif message.msg_type == MessageType.ERROR_REPORT:
            err_msg = message.payload.decode("utf-8", errors="replace")
            logger.warning(f"⚠️ Robot error report from {robot.name}: {err_msg}")

        else:
            logger.debug(
                f"Unhandled message type 0x{message.msg_type:02X} "
                f"from {robot.name}"
            )

    def on(self, event: str, handler: Callable):
        """Register an event handler."""
        if event not in self._event_handlers:
            self._event_handlers[event] = []
        self._event_handlers[event].append(handler)

    def off(self, event: str, handler: Callable):
        """Remove an event handler."""
        if event in self._event_handlers:
            self._event_handlers[event] = [
                h for h in self._event_handlers[event] if h != handler
            ]

    def _emit(self, event: str, *args):
        """Emit an event to all registered handlers."""
        for handler in self._event_handlers.get(event, []):
            try:
                result = handler(*args)
                if asyncio.iscoroutine(result):
                    asyncio.create_task(result)
            except Exception as e:
                logger.error(f"Error in event handler for '{event}': {e}")

    async def start_heartbeat_monitor(self):
        """Start the background heartbeat monitoring task."""
        self._running = True
        self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())
        logger.info("Heartbeat monitor started")

    async def stop_heartbeat_monitor(self):
        """Stop the heartbeat monitoring task."""
        self._running = False
        if self._heartbeat_task:
            self._heartbeat_task.cancel()
            try:
                await self._heartbeat_task
            except asyncio.CancelledError:
                pass
        logger.info("Heartbeat monitor stopped")

    async def _heartbeat_loop(self):
        """Background task to monitor robot heartbeats."""
        while self._running:
            try:
                for robot in list(self._robots.values()):
                    if robot.state == ConnectionState.ACTIVE and not robot.is_alive:
                        logger.warning(
                            f"Robot {robot.name} heartbeat timeout"
                        )
                        robot.transition_to(ConnectionState.DISCONNECTED)
                        self._emit("robot_disconnected", robot)
                        # Trigger reconnection
                        asyncio.create_task(self._reconnect(robot))

                await asyncio.sleep(self.heartbeat_interval)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Heartbeat monitor error: {e}")
                await asyncio.sleep(1)

    async def _reconnect(self, robot: RobotConnection):
        """Attempt to reconnect to a disconnected robot."""
        if robot.reconnect_attempts >= robot.max_reconnect_attempts:
            logger.error(
                f"Max reconnection attempts reached for {robot.name}"
            )
            robot.transition_to(ConnectionState.ERROR)
            self._emit("robot_error", robot, "Max reconnection attempts reached")
            return

        robot.transition_to(ConnectionState.RECONNECTING)
        delay = robot.next_reconnect_delay
        robot.reconnect_attempts += 1

        logger.info(
            f"Reconnecting to {robot.name} in {delay:.1f}s "
            f"(attempt {robot.reconnect_attempts}/{robot.max_reconnect_attempts})"
        )

        await asyncio.sleep(delay)

        if not self._running:
            return

        self._emit("robot_reconnecting", robot)

    def get_all_status(self) -> list[dict]:
        """Get status of all tracked robots for API responses."""
        return [robot.to_dict() for robot in self._robots.values()]
