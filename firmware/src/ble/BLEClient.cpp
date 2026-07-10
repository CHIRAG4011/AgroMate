/**
 * AgroMate BLE Client Implementation
 */

#include "BLEClient.h"

// Forward declaration of callbacks
class CmdWriteCallback;
static CmdWriteCallback* cmdWriteCbInstance = nullptr;

// ── Server Callbacks ──
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    bool* connectedFlag;
    
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        *connectedFlag = true;
        Serial.printf("BLE Connected! Addr: %s\n", connInfo.getAddress().toString().c_str());
        // Request connection parameters compatible with Web Bluetooth
        // Min=24 (30ms), Max=48 (60ms), Latency=0, Timeout=400 (4s)
        // Note: 7.5ms (6,6) is too aggressive — many browsers reject it
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 400);
    }
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        *connectedFlag = false;
        Serial.printf("BLE Disconnected (reason=%d). Restarting advertising...\n", reason);
        NimBLEDevice::startAdvertising();
    }
    
    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
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
    
    // Create primary advertisement packet containing Flags, Name, and Service UUID.
    // Shortened device name to "AgroMate" (8 bytes) so everything fits within the 31-byte BLE limit.
    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);                   // General Discoverable, BR/EDR Not Supported (3 bytes)
    advData.setName("AgroMate");              // Device name (10 bytes with header)
    advData.addServiceUUID(BLE_SERVICE_UUID); // 128-bit Service UUID (18 bytes with header)
    pAdvertising->setAdvertisementData(advData);
    
    pAdvertising->enableScanResponse(false);  // No scan response required since it all fits
    pAdvertising->start();
    
    Serial.println("BLE advertising as 'AgroMate'");
}

void AgroMateBLE::update() {
    // NimBLE handles events internally
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
    _sendChunked(_imgChar, data, length, 0x01); // 0x01 = IMAGE_CHUNK data type
}

void AgroMateBLE::sendAudio(uint8_t* data, size_t length) {
    _sendChunked(_audioTxChar, data, length, 0x02); // 0x02 = AUDIO_CHUNK data type
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
        characteristic->notify();
    }

    free(frameBuf);
}

void AgroMateBLE::_sendChunked(NimBLECharacteristic* characteristic, uint8_t* data, size_t length, uint8_t dataType) {
    if (!isConnected() || !characteristic) return;
    
    uint16_t peerMtu = _server->getPeerMTU(_server->getPeerInfo(0).getConnHandle()) - 3;
    if (peerMtu < 30) peerMtu = 30;
    
    // Reserve space for message framing (12 bytes) and chunk payload header (6 bytes)
    size_t chunkSize = peerMtu - 18;
    if (chunkSize > 494) chunkSize = 494; // Cap at MAX_PAYLOAD_SIZE - 18
    
    size_t totalChunks = (length + chunkSize - 1) / chunkSize;
    uint32_t transferId = millis();
    uint32_t checksum = calculateCRC32(data, length);
    
    Serial.printf("Sending %u bytes in %u chunks (MTU=%u, chunkSize=%u)\n", length, totalChunks, peerMtu, chunkSize);
    
    // 1. Send CHUNK_START
    uint8_t startPayload[13];
    memcpy(startPayload, &transferId, 4);
    memcpy(startPayload + 4, &length, 4);
    memcpy(startPayload + 8, &totalChunks, 4);
    startPayload[12] = dataType;
    _sendMessage(characteristic, MessageType::CHUNK_START, startPayload, 13);
    delay(20);
    
    // 2. Send Chunks
    MessageType chunkType = (dataType == 0x01) ? MessageType::IMAGE_CHUNK : MessageType::AUDIO_CHUNK;
    uint8_t* chunkPayload = (uint8_t*)malloc(chunkSize + 6);
    if (!chunkPayload) {
        Serial.println("Chunked send aborted: Out of memory!");
        return;
    }

    memcpy(chunkPayload, &transferId, 4);
    
    for (size_t i = 0; i < totalChunks; i++) {
        if (!isConnected()) {
            Serial.println("[BLE] Disconnected during chunked send! Aborting.");
            break;
        }
        
        size_t offset = i * chunkSize;
        size_t thisChunkSize = min(chunkSize, length - offset);
        
        uint16_t chunkIndex = i;
        memcpy(chunkPayload + 4, &chunkIndex, 2);
        memcpy(chunkPayload + 6, data + offset, thisChunkSize);
        
        _sendMessage(characteristic, chunkType, chunkPayload, thisChunkSize + 6);
        
        // Paced sending to prevent Windows BLE stack buffer overflow and connection drops
        if (i % 4 == 3) {
            delay(30);
        } else {
            delay(15);
        }
    }
    
    free(chunkPayload);
    delay(20);
    
    // 3. Send CHUNK_END
    uint8_t endPayload[8];
    memcpy(endPayload, &transferId, 4);
    memcpy(endPayload + 4, &checksum, 4);
    _sendMessage(characteristic, MessageType::CHUNK_END, endPayload, 8);
    
    Serial.printf("Chunked send complete: %u bytes\n", length);
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
            break;
        }

        case MessageType::CHUNK_END: {
            if (msg.length >= 8) {
                uint32_t transferId, checksum;
                memcpy(&transferId, msg.payload, 4);
                memcpy(&checksum, msg.payload + 4, 4);
                
                if (_rxTransferActive && _rxTransferId == transferId) {
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
                _resetRxReassembly();
            }
            break;
        }

        case MessageType::SPEECH_AUDIO: {
            // This is a chunk message for the speech audio transfer
            if (msg.length >= 6) {
                uint32_t transferId;
                uint16_t chunkIndex;
                memcpy(&transferId, msg.payload, 4);
                memcpy(&chunkIndex, msg.payload + 4, 2);
                
                if (_rxTransferActive && _rxTransferId == transferId) {
                    size_t chunkDataLen = msg.length - 6;
                    size_t offset = chunkIndex * 504; // Sender uses chunk_size = MAX_PAYLOAD_SIZE - 8 = 504
                    
                    if (offset + chunkDataLen <= _rxTotalSize) {
                        memcpy(_rxBuffer + offset, msg.payload + 6, chunkDataLen);
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
    _rxTransferId = 0;
    _rxTotalSize = 0;
    _rxTotalChunks = 0;
    _rxChunksReceived = 0;
    _rxDataType = 0;
}
