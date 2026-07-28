/**
 * AgroMate BLE Client Implementation
 */

#include "BLEClient.h"
#include "../audio/AudioManager.h"
#include "../display/FaceRenderer.h"

extern AudioManager audio;
extern FaceRenderer face;

// Forward declaration of callbacks
class CmdWriteCallback;
static CmdWriteCallback* cmdWriteCbInstance = nullptr;

// ── Server Callbacks ──
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    bool* connectedFlag;
    uint16_t* mtuField;
    
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        *connectedFlag = true;
        if (mtuField) {
            *mtuField = connInfo.getMTU();
        }
        Serial.printf("BLE Connected! Addr: %s, MTU: %u\n", connInfo.getAddress().toString().c_str(), connInfo.getMTU());
        // Request connection parameters compatible with Web Bluetooth and large data transfers
        // Min=24 (30ms), Max=48 (60ms), Latency=0, Timeout=1000 (10s)
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 1000);
    }
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        *connectedFlag = false;
        if (mtuField) {
            *mtuField = 23;
        }
        Serial.printf("BLE Disconnected (reason=%d). Restarting advertising...\n", reason);
        NimBLEDevice::startAdvertising();
    }
    
    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        if (mtuField) {
            *mtuField = MTU;
        }
        Serial.printf("MTU updated: %u\n", MTU);
    }
};

// ── Characteristic Write Callback ──
class CmdWriteCallback : public NimBLECharacteristicCallbacks {
private:
    AgroMateBLE* _ble;
public:
    CmdWriteCallback(AgroMateBLE* ble) : _ble(ble) {}
    
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        NimBLEAttValue value = pCharacteristic->getValue();
        if (value.length() > 0 && _ble) {
            _ble->handleWriteEvent((const uint8_t*)value.data(), value.length());
        }
    }
};

// Static instances
static ServerCallbacks serverCbs;

void AgroMateBLE::begin(const char* deviceName) {
    Serial.println("Initializing NimBLE...");
    
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(BLE_MTU_SIZE);
    
    _server = NimBLEDevice::createServer();
    serverCbs.connectedFlag = &_connected;
    serverCbs.mtuField = &_mtu;
    _server->setCallbacks(&serverCbs);
    
    // ── Command Service ──
    NimBLEService* cmdService = _server->createService(BLE_SERVICE_UUID);
    
    _cmdTxChar = cmdService->createCharacteristic(
        BLE_CHAR_CMD_TX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    cmdWriteCbInstance = new CmdWriteCallback(this);
    _cmdTxChar->setCallbacks(cmdWriteCbInstance);
    
    _cmdRxChar = cmdService->createCharacteristic(
        BLE_CHAR_CMD_RX,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    _imgChar = cmdService->createCharacteristic(
        BLE_CHAR_IMG_TX,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    _audioTxChar = cmdService->createCharacteristic(
        BLE_CHAR_AUDIO_TX,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    _audioRxChar = cmdService->createCharacteristic(
        BLE_CHAR_AUDIO_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _audioRxChar->setCallbacks(cmdWriteCbInstance);
    
    _batteryChar = cmdService->createCharacteristic(
        BLE_CHAR_BATTERY,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    _sensorChar = cmdService->createCharacteristic(
        BLE_CHAR_SENSORS,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    _heartbeatChar = cmdService->createCharacteristic(
        BLE_CHAR_HEARTBEAT,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    
    cmdService->start();
    
    // ── Start Advertising ──
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->clearData();
    
    // Primary advertisement packet: Flags and 128-bit Service UUID
    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);                   // General Discoverable, BR/EDR Not Supported (3 bytes)
    advData.addServiceUUID(BLE_SERVICE_UUID); // 128-bit Service UUID (18 bytes with header)
    pAdvertising->setAdvertisementData(advData);
    
    // Scan response packet: Local Name
    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName(deviceName);     // Device name in scan response
    pAdvertising->setScanResponseData(scanResponseData);
    
    pAdvertising->enableScanResponse(true);   // Enable scan response for name discovery
    pAdvertising->start();
    
    Serial.printf("BLE advertising as '%s'\n", deviceName);
}

void AgroMateBLE::update() {
    if (!_txActive) return;
    
    unsigned long now = millis();
    if (now - _txLastPacketTime < _txPacketDelayMs) return;
    _txLastPacketTime = now;
    
    _sendNextCooperativeChunk();
}

bool AgroMateBLE::isConnected() {
    return _connected && _server->getConnectedCount() > 0;
}

void AgroMateBLE::onCommand(CommandCallback callback) {
    _cmdCallback = callback;
}

void AgroMateBLE::sendHeartbeat() {
    if (!isConnected()) return;
    _sendMessage(_heartbeatChar, MessageType::HEARTBEAT, nullptr, 0);
}

void AgroMateBLE::sendSensorData(uint8_t batteryPct, uint16_t batteryMv, bool charging) {
    if (!isConnected()) return;
    
    uint8_t payload[12];
    payload[0] = batteryPct;
    payload[1] = batteryMv & 0xFF;
    payload[2] = (batteryMv >> 8) & 0xFF;
    payload[3] = charging ? 1 : 0;
    
    float temp = 0.0f;
    float hum = 0.0f;
    memcpy(payload + 4, &temp, 4);
    memcpy(payload + 8, &hum, 4);
    
    // Send to battery characteristics for backward compatibility
    _batteryChar->setValue(payload, 4);
    _batteryChar->notify();
    
    // Send standard SENSOR_DATA message
    _sendMessage(_sensorChar, MessageType::SENSOR_DATA, payload, 12);
}

void AgroMateBLE::sendImage(uint8_t* data, size_t length) {
    _startCooperativeSend(_imgChar, data, length, 0x01); // 0x01 = IMAGE_CHUNK data type
}

void AgroMateBLE::sendAudio(uint8_t* data, size_t length) {
    _startCooperativeSend(_audioTxChar, data, length, 0x02); // 0x02 = AUDIO_CHUNK data type
}

void AgroMateBLE::sendDebugMessage(const char* text) {
    if (!isConnected()) return;
    _sendMessage(_cmdRxChar, MessageType::ERROR_REPORT, (const uint8_t*)text, strlen(text));
}

void AgroMateBLE::_sendMessage(NimBLECharacteristic* characteristic, MessageType type, const uint8_t* payload, size_t length) {
    if (!isConnected() || !characteristic) return;

    BLEMessage msg;
    msg.msgType = type;
    msg.sequence = _txSeq++;
    msg.length = length;
    if (length > 0 && payload) {
        memcpy(msg.payload, payload, length);
    }

    size_t frameSize = HEADER_SIZE + length + FOOTER_SIZE;
    uint8_t* frameBuf = (uint8_t*)malloc(frameSize);
    if (!frameBuf) {
        Serial.println("BLE send failed: Out of memory!");
        return;
    }

    size_t encodedSize = encodeMessage(msg, frameBuf, frameSize);
    if (encodedSize > 0) {
        characteristic->setValue(frameBuf, encodedSize);
        
        // Retry loop to handle NimBLE congestion and prevent packet drops
        int retries = 0;
        while (!characteristic->notify()) {
            if (!isConnected()) break;
            delay(1);
            retries++;
            if (retries > 100) {
                Serial.println("[BLE] Notification notify failed (timeout)");
                break;
            }
        }
    }

    free(frameBuf);
}

void AgroMateBLE::_startCooperativeSend(NimBLECharacteristic* characteristic, uint8_t* data, size_t length, uint8_t dataType) {
    if (!isConnected() || !characteristic) return;
    if (_txActive) {
        Serial.println("[BLE] Cooperative TX already active, ignoring new request");
        return;
    }
    
    if (dataType == 0x01) {
        // Direct pointer reference to avoid duplicating camera frame buffer in heap memory
        _txData = data;
        _isTxDataAllocated = false;
    } else {
        _txData = (uint8_t*)malloc(length);
        if (!_txData) {
            Serial.println("[BLE] Failed to allocate transmission buffer");
            return;
        }
        memcpy(_txData, data, length);
        _isTxDataAllocated = true;
    }
    _txLen = length;
    _txChar = characteristic;
    _txDataType = dataType;
    
    // We base chunk size on current MTU, accounting for framing overhead (12 bytes),
    // chunk header (6 bytes), and ATT notification packet overhead (3 bytes) with some headroom.
    if (_mtu >= 28) {
        _txChunkSize = min((size_t)(_mtu - 28), (size_t)MAX_PAYLOAD_SIZE - 8);
    } else {
        _txChunkSize = 5;
    }
    _txTotalChunks = (length + _txChunkSize - 1) / _txChunkSize;
    _txTransferId = millis();
    _txChecksum = calculateCRC32(_txData, _txLen);
    _txCurrentChunk = 0;
    _txLastPacketTime = 0;
    _txActive = true;
    
    Serial.printf("[BLE] Started cooperative send: %u bytes, %u chunks\n", _txLen, _txTotalChunks);
}

void AgroMateBLE::_sendNextCooperativeChunk() {
    if (!isConnected()) {
        _abortCooperativeTx();
        return;
    }
    
    if (_txCurrentChunk == 0) {
        // Send CHUNK_START (first tick)
        uint8_t startPayload[13];
        memcpy(startPayload, &_txTransferId, 4);
        memcpy(startPayload + 4, &_txLen, 4);
        memcpy(startPayload + 8, &_txTotalChunks, 4);
        startPayload[12] = _txDataType;
        _sendMessage(_txChar, MessageType::CHUNK_START, startPayload, 13);
        _txLastPacketTime = millis();
        _txCurrentChunk = 1; // move to first data chunk on next tick
        return;
    }
    
    if (_txCurrentChunk <= _txTotalChunks) {
        // Send data chunk
        size_t chunkIdx = _txCurrentChunk - 1;
        size_t offset = chunkIdx * _txChunkSize;
        size_t thisChunkSize = min(_txChunkSize, _txLen - offset);
        
        MessageType chunkType = (_txDataType == 0x01) ? MessageType::IMAGE_CHUNK : MessageType::AUDIO_CHUNK;
        uint8_t* chunkPayload = (uint8_t*)malloc(_txChunkSize + 6);
        if (!chunkPayload) {
            Serial.println("[BLE] Out of memory allocating chunk payload!");
            _abortCooperativeTx();
            return;
        }
        
        memcpy(chunkPayload, &_txTransferId, 4);
        uint16_t idx16 = chunkIdx;
        memcpy(chunkPayload + 4, &idx16, 2);
        memcpy(chunkPayload + 6, _txData + offset, thisChunkSize);
        
        _sendMessage(_txChar, chunkType, chunkPayload, thisChunkSize + 6);
        free(chunkPayload);
        
        _txCurrentChunk++;
        return;
    }
    
    // Send CHUNK_END
    uint8_t endPayload[8];
    memcpy(endPayload, &_txTransferId, 4);
    memcpy(endPayload + 4, &_txChecksum, 4);
    _sendMessage(_txChar, MessageType::CHUNK_END, endPayload, 8);
    
    Serial.printf("[BLE] Cooperative transfer complete: %u bytes\n", _txLen);
    _abortCooperativeTx();
}

void AgroMateBLE::_abortCooperativeTx() {
    if (_txData && _isTxDataAllocated) {
        free(_txData);
    }
    _txData = nullptr;
    _txActive = false;
}

void AgroMateBLE::handleWriteEvent(const uint8_t* data, size_t length) {
    BLEMessage msg;
    if (!decodeMessage(data, length, msg)) {
        Serial.printf("BLE frame decode error! size=%d\n", length);
        return;
    }

    _handleIncomingMessage(msg);
}

void AgroMateBLE::_handleIncomingMessage(const BLEMessage& msg) {
    switch (msg.msgType) {
        case MessageType::CHUNK_START: {
            if (msg.length >= 13) {
                uint32_t transferId, totalSize, totalChunks;
                uint8_t dataType;
                memcpy(&transferId, msg.payload, 4);
                memcpy(&totalSize, msg.payload + 4, 4);
                memcpy(&totalChunks, msg.payload + 8, 4);
                dataType = msg.payload[12];
                
                _resetRxReassembly();

                if (dataType == static_cast<uint8_t>(MessageType::SPEECH_AUDIO)) {
                    // Streaming audio over BLE - stream directly into I2S without allocating large RAM buffer
                    _rxIsAudioStreaming = true;
                    _rxTransferActive = true;
                    _rxTransferId = transferId;
                    _rxTotalSize = totalSize;
                    _rxTotalChunks = totalChunks;
                    _rxChunksReceived = 0;
                    _rxDataType = dataType;
                    Serial.printf("[BLE] Incoming SPEECH_AUDIO stream: id=%u, size=%u, chunks=%u\n",
                                  transferId, totalSize, totalChunks);
                    audio.startAudioStream();
                    face.setExpression(Expression::TALKING);
                } else {
                    _rxBuffer = (uint8_t*)malloc(totalSize);
                    if (_rxBuffer) {
                        _rxTransferActive = true;
                        _rxTransferId = transferId;
                        _rxTotalSize = totalSize;
                        _rxTotalChunks = totalChunks;
                        _rxChunksReceived = 0;
                        _rxDataType = dataType;
                        Serial.printf("Incoming chunked transfer started: id=%u, size=%u, chunks=%u, type=0x%02X\n",
                                      transferId, totalSize, totalChunks, dataType);
                    } else {
                        Serial.printf("Failed to allocate RX reassembly buffer of size %u!\n", totalSize);
                    }
                }
            }
            break;
        }

        case MessageType::CHUNK_END: {
            if (msg.length >= 8) {
                uint32_t transferId, checksum;
                memcpy(&transferId, msg.payload, 4);
                memcpy(&checksum, msg.payload + 4, 4);
                
                if (_rxTransferActive && _rxTransferId == transferId) {
                    if (_rxIsAudioStreaming) {
                        Serial.println("[BLE] SPEECH_AUDIO stream finished receiving!");
                        audio.stopAudioStream();
                    } else if (_rxBuffer) {
                        uint32_t computed = calculateCRC32(_rxBuffer, _rxTotalSize);
                        if (computed == checksum) {
                            Serial.printf("Reassembly complete! Size: %u. Verifying checksum OK.\n", _rxTotalSize);
                            
                            // Pass reassembled buffer to user command callback
                            if (_cmdCallback) {
                                uint8_t* temp = (uint8_t*)malloc(_rxTotalSize + 1);
                                if (temp) {
                                    temp[0] = _rxDataType;
                                    memcpy(temp + 1, _rxBuffer, _rxTotalSize);
                                    _cmdCallback(temp, _rxTotalSize + 1);
                                    free(temp);
                                }
                            }
                        } else {
                            Serial.printf("CRC32 mismatch on reassembled data! Computed: 0x%08X, expected: 0x%08X\n",
                                          computed, checksum);
                        }
                    }
                }
                _resetRxReassembly();
            }
            break;
        }

        case MessageType::SPEECH_AUDIO: {
            // This is a chunk message for the speech audio transfer
            if (msg.length >= 6 && _rxTransferActive) {
                uint32_t transferId;
                memcpy(&transferId, msg.payload, 4);
                
                if (_rxTransferId == transferId) {
                    size_t chunkDataLen = msg.length - 6; // subtract 4-byte transferId + 2-byte chunkIndex
                    
                    if (_rxIsAudioStreaming) {
                        audio.writeAudioStream(msg.payload + 6, chunkDataLen);
                        _rxChunksReceived++;
                    } else if (_rxBuffer && _rxBytesReceived + chunkDataLen <= _rxTotalSize) {
                        memcpy(_rxBuffer + _rxBytesReceived, msg.payload + 6, chunkDataLen);
                        _rxBytesReceived += chunkDataLen;
                        _rxChunksReceived++;
                    }
                }
            }
            break;
        }

        default: {
            // Normal message, decapsulate and pass to command callback
            if (_cmdCallback) {
                uint8_t* temp = (uint8_t*)malloc(msg.length + 1);
                if (temp) {
                    temp[0] = static_cast<uint8_t>(msg.msgType);
                    if (msg.length > 0) {
                        memcpy(temp + 1, msg.payload, msg.length);
                    }
                    _cmdCallback(temp, msg.length + 1);
                    free(temp);
                }
            }
            break;
        }
    }
}

void AgroMateBLE::_resetRxReassembly() {
    if (_rxBuffer) {
        free(_rxBuffer);
        _rxBuffer = nullptr;
    }
    _rxTransferActive = false;
    _rxIsAudioStreaming = false;
    _rxTransferId = 0;
    _rxTotalSize = 0;
    _rxTotalChunks = 0;
    _rxChunksReceived = 0;
    _rxBytesReceived = 0;
    _rxDataType = 0;
}
