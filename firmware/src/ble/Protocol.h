#pragma once
/**
 * AgroMate BLE Protocol Framing
 * Handles binary packet formatting, sequence management, and CRC validation.
 */

#include <Arduino.h>

#define SYNC_BYTE_1 0xA6
#define SYNC_BYTE_2 0xAD
#define HEADER_SIZE 10
#define FOOTER_SIZE 2
#define MAX_PAYLOAD_SIZE 512

enum class MessageType : uint16_t {
    // ESP32 → Laptop (0x01 - 0x0F)
    IMAGE_CHUNK = 0x01,
    AUDIO_CHUNK = 0x02,
    SENSOR_DATA = 0x03,
    HEARTBEAT = 0x04,
    BUTTON_EVENT = 0x05,
    TOUCH_EVENT = 0x06,
    CAMERA_CAPTURE_DONE = 0x07,

    // Laptop → ESP32 (0x10 - 0x1F)
    AI_RESPONSE = 0x10,
    SPEECH_AUDIO = 0x11,
    FACE_COMMAND = 0x12,
    LED_COMMAND = 0x13,
    SERVO_COMMAND = 0x14,
    STATUS_UPDATE = 0x15,
    CAPTURE_REQUEST = 0x16,
    RECORD_REQUEST = 0x17,
    PLAY_AUDIO = 0x18,

    // Bidirectional (0x20 - 0x2F)
    ACK = 0x20,
    NACK = 0x21,
    CONFIG_SYNC = 0x22,
    ERROR_REPORT = 0x23,
    PING = 0x24,
    PONG = 0x25,

    // Transfer control (0xF0 - 0xFF)
    CHUNK_START = 0xFE,
    CHUNK_END = 0xFF
};

struct BLEMessage {
    MessageType msgType;
    uint16_t sequence;
    uint32_t length;
    uint8_t payload[MAX_PAYLOAD_SIZE];
};

// Encode a message into the provided buffer. Returns the total frame size.
size_t encodeMessage(const BLEMessage& msg, uint8_t* outBuffer, size_t maxLen);

// Decode a buffer into a BLEMessage. Returns true if successful.
bool decodeMessage(const uint8_t* buffer, size_t len, BLEMessage& outMsg);

// Standard CRC32 helper
uint32_t calculateCRC32(const uint8_t* data, size_t length);
