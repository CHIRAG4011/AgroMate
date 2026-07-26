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
    bool isTxActive() { return _txActive; }
    
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
    size_t _rxBytesReceived = 0;  // running byte offset for sequential chunk writes
    uint8_t* _rxBuffer = nullptr;
    uint8_t _rxDataType = 0;

    // Cooperative Tx state
    bool _txActive = false;
    NimBLECharacteristic* _txChar = nullptr;
    uint8_t* _txData = nullptr;
    size_t _txLen = 0;
    uint8_t _txDataType = 0;
    size_t _txChunkSize = 0;
    size_t _txTotalChunks = 0;
    size_t _txCurrentChunk = 0;
    uint32_t _txTransferId = 0;
    uint32_t _txChecksum = 0;
    unsigned long _txLastPacketTime = 0;
    unsigned long _txPacketDelayMs = 40; // 40ms paced sending
    bool _isTxDataAllocated = false;

    void _startCooperativeSend(NimBLECharacteristic* characteristic, uint8_t* data, size_t length, uint8_t dataType);
    void _sendNextCooperativeChunk();
    void _abortCooperativeTx();
    void _sendMessage(NimBLECharacteristic* characteristic, MessageType type, const uint8_t* payload, size_t length);
    void _handleIncomingMessage(const BLEMessage& msg);
    void _resetRxReassembly();
};
