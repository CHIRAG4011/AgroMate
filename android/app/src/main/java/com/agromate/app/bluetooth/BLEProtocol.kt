package com.agromate.app.bluetooth

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

enum class MessageType(val value: Short) {
    IMAGE_CHUNK(0x01),
    AUDIO_CHUNK(0x02),
    SENSOR_DATA(0x03),
    HEARTBEAT(0x04),
    BUTTON_EVENT(0x05),
    TOUCH_EVENT(0x06),
    CAMERA_CAPTURE_DONE(0x07),
    AI_RESPONSE(0x10),
    SPEECH_AUDIO(0x11),
    FACE_COMMAND(0x12),
    LED_COMMAND(0x13),
    SERVO_COMMAND(0x14),
    STATUS_UPDATE(0x15),
    CAPTURE_REQUEST(0x16),
    RECORD_REQUEST(0x17),
    PLAY_AUDIO(0x18),
    ACK(0x20),
    NACK(0x21),
    CONFIG_SYNC(0x22),
    ERROR_REPORT(0x23),
    PING(0x24),
    PONG(0x25),
    CHUNK_START(0xFE.toShort()),
    CHUNK_END(0xFF.toShort());

    companion object {
        fun fromValue(value: Short): MessageType? = values().find { it.value == value }
    }
}

data class BLEMessage(
    val msgType: MessageType,
    val sequence: Short,
    val payload: ByteArray
) {
    fun encode(): ByteArray {
        val headerSize = 10
        val footerSize = 2
        val buffer = ByteBuffer.allocate(headerSize + payload.size + footerSize).order(ByteOrder.LITTLE_ENDIAN)
        
        buffer.put(0xA6.toByte()) // SYNC_1
        buffer.put(0xAD.toByte()) // SYNC_2
        buffer.putShort(msgType.value)
        buffer.putShort(sequence)
        buffer.putInt(payload.size)
        buffer.put(payload)
        
        // Calculate CRC32 of header + payload
        val crc = CRC32()
        crc.update(buffer.array(), 0, headerSize + payload.size)
        val crc16 = (crc.value and 0xFFFFL).toShort()
        buffer.putShort(crc16)
        
        return buffer.array()
    }

    companion object {
        fun decode(bytes: ByteArray): BLEMessage? {
            if (bytes.size < 12) return null
            if (bytes[0] != 0xA6.toByte() || bytes[1] != 0xAD.toByte()) return null
            
            val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
            buffer.position(2)
            
            val typeVal = buffer.short
            val msgType = MessageType.fromValue(typeVal) ?: return null
            val sequence = buffer.short
            val length = buffer.int
            
            if (bytes.size < 10 + length + 2) return null
            
            val payload = ByteArray(length)
            buffer.get(payload)
            
            val crc = CRC32()
            crc.update(bytes, 0, 10 + length)
            val computedCrc = (crc.value and 0xFFFFL).toShort()
            val receivedCrc = buffer.short
            
            if (computedCrc != receivedCrc) return null
            
            return BLEMessage(msgType, sequence, payload)
        }
    }
}
