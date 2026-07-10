#pragma once
/**
 * AgroMate BLE Client — NimBLE-based BLE peripheral
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../config.h"
#include "Protocol.h"

// Callback type for incoming commands
typedef void (*CommandCallback)(uint8_t* data, size_t length);

class AgroMateBLE {
public:
    void begin(const char* deviceName);
    void update();
    bool isConnected();
    void sendHeartbeat();
    void sendSensorData(uint8_t batteryPct, uint16_t batteryMv, bool charging);
    void sendImage(uint8_t* data, size_t length);
    void sendAudio(uint8_t* data, size_t length);
    void sendDebugMessage(const char* text);
    void onCommand(CommandCallback callback);
    
    // Internal handler for GATTS writes
    void handleWriteEvent(const uint8_t* data, size_t length);

private:
    NimBLEServer* _server = nullptr;
    NimBLECharacteristic* _cmdTxChar = nullptr;  // Laptop → ESP
    NimBLECharacteristic* _cmdRxChar = nullptr;  // ESP → Laptop
    NimBLECharacteristic* _imgChar = nullptr;
    NimBLECharacteristic* _audioTxChar = nullptr;
    NimBLECharacteristic* _audioRxChar = nullptr;
    NimBLECharacteristic* _batteryChar = nullptr;
    NimBLECharacteristic* _sensorChar = nullptr;
    NimBLECharacteristic* _heartbeatChar = nullptr;

    bool _connected = false;
    CommandCallback _cmdCallback = nullptr;
    uint16_t _mtu = 23;
    uint16_t _txSeq = 0;

    // Rx Reassembly state
    bool _rxTransferActive = false;
    uint32_t _rxTransferId = 0;
    uint32_t _rxTotalSize = 0;
    uint32_t _rxTotalChunks = 0;
    uint32_t _rxChunksReceived = 0;
    uint8_t* _rxBuffer = nullptr;
    uint8_t _rxDataType = 0;

    void _sendChunked(NimBLECharacteristic* characteristic, uint8_t* data, size_t length, uint8_t dataType);
    void _sendMessage(NimBLECharacteristic* characteristic, MessageType type, const uint8_t* payload, size_t length);
    void _handleIncomingMessage(const BLEMessage& msg);
    void _resetRxReassembly();
};
