#include "Protocol.h"

uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

size_t encodeMessage(const BLEMessage& msg, uint8_t* outBuffer, size_t maxLen) {
    size_t requiredLen = HEADER_SIZE + msg.length + FOOTER_SIZE;
    if (requiredLen > maxLen) {
        return 0;
    }

    // Header
    outBuffer[0] = SYNC_BYTE_1;
    outBuffer[1] = SYNC_BYTE_2;
    
    uint16_t typeRaw = static_cast<uint16_t>(msg.msgType);
    outBuffer[2] = typeRaw & 0xFF;
    outBuffer[3] = (typeRaw >> 8) & 0xFF;
    
    outBuffer[4] = msg.sequence & 0xFF;
    outBuffer[5] = (msg.sequence >> 8) & 0xFF;
    
    outBuffer[6] = msg.length & 0xFF;
    outBuffer[7] = (msg.length >> 8) & 0xFF;
    outBuffer[8] = (msg.length >> 16) & 0xFF;
    outBuffer[9] = (msg.length >> 24) & 0xFF;

    // Payload
    if (msg.length > 0) {
        memcpy(outBuffer + HEADER_SIZE, msg.payload, msg.length);
    }

    // CRC
    uint32_t crc32 = calculateCRC32(outBuffer, HEADER_SIZE + msg.length);
    uint16_t crc16 = crc32 & 0xFFFF;
    
    outBuffer[HEADER_SIZE + msg.length] = crc16 & 0xFF;
    outBuffer[HEADER_SIZE + msg.length + 1] = (crc16 >> 8) & 0xFF;

    return requiredLen;
}

bool decodeMessage(const uint8_t* buffer, size_t len, BLEMessage& outMsg) {
    if (len < HEADER_SIZE + FOOTER_SIZE) {
        return false;
    }

    if (buffer[0] != SYNC_BYTE_1 || buffer[1] != SYNC_BYTE_2) {
        return false;
    }

    uint16_t typeRaw = buffer[2] | (buffer[3] << 8);
    outMsg.msgType = static_cast<MessageType>(typeRaw);
    
    outMsg.sequence = buffer[4] | (buffer[5] << 8);
    
    outMsg.length = buffer[6] | (buffer[7] << 8) | (buffer[8] << 16) | (buffer[9] << 24);

    if (outMsg.length > MAX_PAYLOAD_SIZE) {
        return false;
    }

    if (len < HEADER_SIZE + outMsg.length + FOOTER_SIZE) {
        return false;
    }

    // Check CRC
    uint32_t computedCrc = calculateCRC32(buffer, HEADER_SIZE + outMsg.length);
    uint16_t computedCrc16 = computedCrc & 0xFFFF;
    
    uint16_t receivedCrc16 = buffer[HEADER_SIZE + outMsg.length] | 
                             (buffer[HEADER_SIZE + outMsg.length + 1] << 8);

    if (computedCrc16 != receivedCrc16) {
        return false;
    }

    if (outMsg.length > 0) {
        memcpy(outMsg.payload, buffer + HEADER_SIZE, outMsg.length);
    }

    return true;
}
