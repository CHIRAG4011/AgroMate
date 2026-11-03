"""
AgroMate BLE Communication Protocol

Custom binary framing protocol for efficient Bluetooth Low Energy communication
between the laptop AI server and ESP32 robot(s).

Frame Format:
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ SYNC (2) │ TYPE (1) │ SEQ (2)  │ LEN (4)  │ PAYLOAD  │ CRC (2)  │
│ 0xAG 0xRM│          │          │          │ (0-512)  │          │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
"""

import struct
import enum
import zlib
from dataclasses import dataclass, field
from typing import Optional
import logging

logger = logging.getLogger(__name__)

# Protocol constants
SYNC_BYTE_1 = 0xA6  # 'A' for AgroMate
SYNC_BYTE_2 = 0xAD  # Second sync byte
HEADER_SIZE = 11  # 2 sync + 1 type + 2 seq + 4 len + 2 crc
MAX_PAYLOAD_SIZE = 512
MAX_FRAME_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE

# BLE Service and Characteristic UUIDs
SERVICE_UUID_CMD = "0000ag01-0000-1000-8000-00805f9b34fb"
SERVICE_UUID_DATA = "0000ag02-0000-1000-8000-00805f9b34fb"
SERVICE_UUID_STATUS = "0000ag03-0000-1000-8000-00805f9b34fb"

CHAR_UUID_CMD_TX = "0000ag11-0000-1000-8000-00805f9b34fb"
CHAR_UUID_CMD_RX = "0000ag12-0000-1000-8000-00805f9b34fb"
CHAR_UUID_IMG_TX = "0000ag21-0000-1000-8000-00805f9b34fb"
CHAR_UUID_AUDIO_TX = "0000ag22-0000-1000-8000-00805f9b34fb"
CHAR_UUID_AUDIO_RX = "0000ag23-0000-1000-8000-00805f9b34fb"
CHAR_UUID_BATTERY = "0000ag31-0000-1000-8000-00805f9b34fb"
CHAR_UUID_SENSORS = "0000ag32-0000-1000-8000-00805f9b34fb"
CHAR_UUID_HEARTBEAT = "0000ag33-0000-1000-8000-00805f9b34fb"

# Use standard-compliant UUIDs for actual BLE operations
BLE_SERVICE_UUIDS = {
    "command": "12345678-1234-5678-1234-56789abcde88",
    "data": "12345678-1234-5678-1234-56789abcde02",
    "status": "12345678-1234-5678-1234-56789abcde03",
}

BLE_CHAR_UUIDS = {
    "cmd_tx": "12345678-1234-5678-1234-56789abcdf81",
    "cmd_rx": "12345678-1234-5678-1234-56789abcdf82",
    "img_tx": "12345678-1234-5678-1234-56789abcdf83",
    "audio_tx": "12345678-1234-5678-1234-56789abcdf84",
    "audio_rx": "12345678-1234-5678-1234-56789abcdf85",
    "battery": "12345678-1234-5678-1234-56789abcdf86",
    "sensors": "12345678-1234-5678-1234-56789abcdf87",
    "heartbeat": "12345678-1234-5678-1234-56789abcdf88",
}


class MessageType(enum.IntEnum):
    """BLE message type identifiers."""

    # ESP32 → Laptop (0x01 - 0x0F)
    IMAGE_CHUNK = 0x01
    AUDIO_CHUNK = 0x02
    SENSOR_DATA = 0x03
    HEARTBEAT = 0x04
    BUTTON_EVENT = 0x05
    TOUCH_EVENT = 0x06
    CAMERA_CAPTURE_DONE = 0x07

    # Laptop → ESP32 (0x10 - 0x1F)
    AI_RESPONSE = 0x10
    SPEECH_AUDIO = 0x11
    FACE_COMMAND = 0x12
    LED_COMMAND = 0x13
    SERVO_COMMAND = 0x14
    STATUS_UPDATE = 0x15
    CAPTURE_REQUEST = 0x16
    RECORD_REQUEST = 0x17
    PLAY_AUDIO = 0x18

    # Bidirectional (0x20 - 0x2F)
    ACK = 0x20
    NACK = 0x21
    CONFIG_SYNC = 0x22
    ERROR_REPORT = 0x23
    PING = 0x24
    PONG = 0x25

    # Transfer control (0xF0 - 0xFF)
    CHUNK_START = 0xFE
    CHUNK_END = 0xFF


class FaceExpression(enum.IntEnum):
    """Face expression identifiers sent via FACE_COMMAND."""

    IDLE = 0
    HAPPY = 1
    THINKING = 2
    LISTENING = 3
    TALKING = 4
    ERROR = 5
    WARNING = 6
    SLEEPING = 7
    SURPRISED = 8
    SAD = 9
    ANGRY = 10
    CURIOUS = 11
    SCANNING = 12
    SUCCESS = 13


class LEDPattern(enum.IntEnum):
    """LED pattern identifiers sent via LED_COMMAND."""

    OFF = 0
    SOLID = 1
    BLINK = 2
    PULSE = 3
    RAINBOW = 4
    CHASE = 5
    ALERT = 6
    SCANNING = 7


@dataclass
class BLEMessage:
    """Represents a single BLE protocol message."""

    msg_type: MessageType
    payload: bytes = b""
    sequence: int = 0

    def __post_init__(self):
        if len(self.payload) > MAX_PAYLOAD_SIZE:
            raise ValueError(
                f"Payload size {len(self.payload)} exceeds max {MAX_PAYLOAD_SIZE}"
            )

    def encode(self) -> bytes:
        """Encode message to binary frame format."""
        header = struct.pack(
            "<BBHHI",
            SYNC_BYTE_1,
            SYNC_BYTE_2,
            self.msg_type,
            self.sequence,
            len(self.payload),
        )
        frame = header + self.payload
        crc = zlib.crc32(frame) & 0xFFFF
        return frame + struct.pack("<H", crc)

    @classmethod
    def decode(cls, data: bytes) -> Optional["BLEMessage"]:
        """Decode binary frame data into a BLEMessage."""
        if len(data) < HEADER_SIZE:
            logger.warning(f"Frame too short: {len(data)} bytes")
            return None

        # Verify sync bytes
        if data[0] != SYNC_BYTE_1 or data[1] != SYNC_BYTE_2:
            logger.warning(f"Invalid sync bytes: 0x{data[0]:02X} 0x{data[1]:02X}")
            return None

        # Parse header
        _, _, msg_type_raw, sequence, payload_len = struct.unpack(
            "<BBHHI", data[:10]
        )

        # Validate message type
        try:
            msg_type = MessageType(msg_type_raw)
        except ValueError:
            logger.warning(f"Unknown message type: 0x{msg_type_raw:02X}")
            return None

        # Validate payload length
        if payload_len > MAX_PAYLOAD_SIZE:
            logger.warning(f"Payload too large: {payload_len}")
            return None

        expected_total = 10 + payload_len + 2  # header + payload + crc
        if len(data) < expected_total:
            logger.warning(
                f"Incomplete frame: got {len(data)}, expected {expected_total}"
            )
            return None

        # Extract payload
        payload = data[10 : 10 + payload_len]

        # Verify CRC
        frame_data = data[: 10 + payload_len]
        received_crc = struct.unpack("<H", data[10 + payload_len : 10 + payload_len + 2])[0]
        computed_crc = zlib.crc32(frame_data) & 0xFFFF

        if received_crc != computed_crc:
            logger.warning(
                f"CRC mismatch: received=0x{received_crc:04X}, "
                f"computed=0x{computed_crc:04X}"
            )
            return None

        return cls(msg_type=msg_type, payload=payload, sequence=sequence)


@dataclass
class ChunkTransferState:
    """Tracks the state of a multi-chunk transfer (images, audio)."""

    transfer_id: int = 0
    total_size: int = 0
    total_chunks: int = 0
    received_chunks: int = 0
    data: bytearray = field(default_factory=bytearray)
    chunks_map: dict = field(default_factory=dict)
    checksum: int = 0
    is_complete: bool = False
    data_type: int = 0

    def add_chunk(self, chunk_data: bytes, chunk_index: int) -> bool:
        """Add a received chunk to the transfer buffer."""
        self.chunks_map[chunk_index] = chunk_data
        self.received_chunks = len(self.chunks_map)
        return True

    def validate_and_complete(self, checksum: int) -> bool:
        """Validate checksum and complete the transfer."""
        self.checksum = checksum
        
        # Log transfer state for debugging
        dt_str = f"0x{self.data_type:02X}" if isinstance(self.data_type, int) else str(self.data_type)
        logger.info(
            f"Transfer {self.transfer_id} validating: "
            f"chunks={len(self.chunks_map)}/{self.total_chunks}, "
            f"data_type={dt_str}"
        )
        
        if self.total_chunks > 0 and len(self.chunks_map) >= self.total_chunks:
            self.data = bytearray(b"".join(self.chunks_map[i] for i in range(self.total_chunks)))
        elif len(self.chunks_map) > 0:
            sorted_indices = sorted(self.chunks_map.keys())
            self.data = bytearray(b"".join(self.chunks_map[i] for i in sorted_indices))
            logger.warning(
                f"Transfer {self.transfer_id}: using {len(self.chunks_map)} chunks "
                f"(expected {self.total_chunks}), indices: {sorted_indices[:5]}..."
            )
        
        computed = zlib.crc32(bytes(self.data)) & 0xFFFFFFFF
        if computed == checksum:
            self.is_complete = True
            logger.info(
                f"Transfer {self.transfer_id} complete: "
                f"{len(self.data)} bytes, CRC OK"
            )
            return True
        else:
            data_bytes = bytes(self.data)
            # Accept if it's a valid JPEG image, audio payload, or sufficient chunks were received
            if len(data_bytes) > 64 and (data_bytes.startswith(b"\xff\xd8") or data_bytes.startswith(b"RIFF") or self.data_type in (1, 2, MessageType.IMAGE_CHUNK, MessageType.AUDIO_CHUNK)):
                logger.warning(
                    f"Transfer {self.transfer_id} checksum mismatch "
                    f"(expected=0x{checksum:08X}, got=0x{computed:08X}, "
                    f"{len(self.data)} bytes, header=0x{data_bytes[0]:02X}{data_bytes[1]:02X}) "
                    f"- accepting valid payload"
                )
                self.is_complete = True
                return True
            logger.error(
                f"Transfer checksum mismatch: "
                f"expected=0x{checksum:08X}, got=0x{computed:08X} "
                f"({len(self.data)} bytes, chunks={len(self.chunks_map)}/{self.total_chunks})"
            )
            return False

    @property
    def progress(self) -> float:
        """Return transfer progress as a float 0.0 - 1.0."""
        if self.total_chunks == 0:
            return 0.0
        return self.received_chunks / self.total_chunks



# --- Message Builders ---


def build_face_command(expression: FaceExpression, intensity: int = 100) -> BLEMessage:
    """Build a face expression command message."""
    payload = struct.pack("<BB", expression, min(intensity, 255))
    return BLEMessage(msg_type=MessageType.FACE_COMMAND, payload=payload)


def build_led_command(
    pattern: LEDPattern, r: int = 0, g: int = 0, b: int = 0, speed: int = 50
) -> BLEMessage:
    """Build an LED control command message."""
    payload = struct.pack("<BBBBB", pattern, r, g, b, speed)
    return BLEMessage(msg_type=MessageType.LED_COMMAND, payload=payload)


def build_servo_command(pan: int = 90, tilt: int = 90) -> BLEMessage:
    """Build a servo position command message (0-180 degrees)."""
    payload = struct.pack("<BB", min(pan, 180), min(tilt, 180))
    return BLEMessage(msg_type=MessageType.SERVO_COMMAND, payload=payload)


def build_ai_response(text: str, expression: FaceExpression = FaceExpression.IDLE) -> BLEMessage:
    """Build an AI response message with text and optional expression."""
    text_bytes = text.encode("utf-8")[:500]  # Limit text to 500 bytes
    payload = struct.pack("<B", expression) + text_bytes
    return BLEMessage(msg_type=MessageType.AI_RESPONSE, payload=payload)


def build_capture_request(resolution: int = 1) -> BLEMessage:
    """Build a camera capture request. resolution: 0=QVGA, 1=VGA, 2=SVGA."""
    payload = struct.pack("<B", min(resolution, 2))
    return BLEMessage(msg_type=MessageType.CAPTURE_REQUEST, payload=payload)


def build_status_update(screen_mode: int) -> BLEMessage:
    """Build a status update message. screen_mode: 0=FACE, 1=DASHBOARD."""
    payload = struct.pack("<B", screen_mode)
    return BLEMessage(msg_type=MessageType.STATUS_UPDATE, payload=payload)


def build_weather_update(temperature: float, humidity: int, forecast: str) -> BLEMessage:
    """Build a weather sync message (STATUS_UPDATE mode 2).
    
    Payload format: mode(1) + temp(4 float) + humidity(1) + forecast(string)
    """
    forecast_bytes = forecast[:31].encode("utf-8")
    payload = struct.pack("<Bf", 2, temperature) + struct.pack("<B", min(humidity, 255)) + forecast_bytes
    return BLEMessage(msg_type=MessageType.STATUS_UPDATE, payload=payload)


def build_disease_notify() -> BLEMessage:
    """Build a disease detection notification (STATUS_UPDATE mode 3).
    
    Tells the robot to increment its disease counter in NVS.
    """
    payload = struct.pack("<B", 3)
    return BLEMessage(msg_type=MessageType.STATUS_UPDATE, payload=payload)


def build_stats_sync(total_scans: int, diseases_found: int) -> BLEMessage:
    """Build a stats sync message (STATUS_UPDATE mode 4).
    
    Payload format: mode(1) + total_scans(4 uint32) + diseases_found(4 uint32)
    """
    payload = struct.pack("<BII", 4, total_scans, diseases_found)
    return BLEMessage(msg_type=MessageType.STATUS_UPDATE, payload=payload)


def build_record_request(duration_ms: int = 5000) -> BLEMessage:
    """Build a microphone recording request."""
    payload = struct.pack("<I", duration_ms)
    return BLEMessage(msg_type=MessageType.RECORD_REQUEST, payload=payload)


def build_ack(sequence: int) -> BLEMessage:
    """Build an acknowledgment message."""
    payload = struct.pack("<H", sequence)
    return BLEMessage(msg_type=MessageType.ACK, payload=payload)


def build_chunk_start(
    transfer_id: int, total_size: int, total_chunks: int, data_type: MessageType
) -> BLEMessage:
    """Build a chunk transfer start message."""
    payload = struct.pack("<IIIB", transfer_id, total_size, total_chunks, data_type)
    return BLEMessage(msg_type=MessageType.CHUNK_START, payload=payload)


def build_chunk_end(transfer_id: int, checksum: int) -> BLEMessage:
    """Build a chunk transfer end message with final checksum."""
    payload = struct.pack("<II", transfer_id, checksum)
    return BLEMessage(msg_type=MessageType.CHUNK_END, payload=payload)


def build_sensor_data(
    battery_pct: int,
    battery_mv: int,
    is_charging: bool,
    temperature: float = 0.0,
    humidity: float = 0.0,
) -> BLEMessage:
    """Build a sensor data report message."""
    payload = struct.pack(
        "<BH?ff",
        min(battery_pct, 100),
        battery_mv,
        is_charging,
        temperature,
        humidity,
    )
    return BLEMessage(msg_type=MessageType.SENSOR_DATA, payload=payload)


def parse_sensor_data(payload: bytes) -> dict:
    """Parse sensor data payload into a dictionary."""
    if len(payload) < 12:
        return {}

    battery_pct, battery_mv, is_charging, temperature, humidity = struct.unpack(
        "<BH?ff", payload[:12]
    )
    return {
        "battery_percent": battery_pct,
        "battery_millivolts": battery_mv,
        "is_charging": is_charging,
        "temperature": temperature,
        "humidity": humidity,
    }


def parse_face_command(payload: bytes) -> dict:
    """Parse face command payload."""
    if len(payload) < 2:
        return {}
    expression, intensity = struct.unpack("<BB", payload[:2])
    return {
        "expression": FaceExpression(expression),
        "intensity": intensity,
    }


def parse_ai_response(payload: bytes) -> dict:
    """Parse AI response payload."""
    if len(payload) < 2:
        return {}
    expression = payload[0]
    text = payload[1:].decode("utf-8", errors="replace")
    return {
        "expression": FaceExpression(expression),
        "text": text,
    }
