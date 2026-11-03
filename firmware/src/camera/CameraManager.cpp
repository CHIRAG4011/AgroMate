/**
 * AgroMate Camera Manager — ESP32-CAM Module 2 UART Client Implementation
 *
 * ESP32-CAM Module 2 specifics:
 *   - OV2640 with hardware JPEG encoder (no software conversion needed!)
 *   - Communication via UART at 460800 bps (reliable on breadboard)
 *   - Binary frame protocol with sync markers for reliable transfer
 *   - JPEG output: typically 15-40KB for VGA, 5-10KB for QVGA
 *
 * Protocol:
 *   Command:  "CAPTURE\n" / "PREVIEW\n" / "STREAM\n" / "STOP\n" sent over Serial2 TX
 *   Response: [0xAA 0x55] [4-byte size LE] [JPEG data] [0x55 0xAA]
 */

#include "CameraManager.h"
#include "../ble/BLEClient.h"
extern AgroMateBLE ble;

// Use HardwareSerial for ESP32-CAM communication
HardwareSerial CamSerial(ESP32CAM_UART_NUM);

// Frame sync markers
static const uint8_t SYNC_START_0 = 0xAA;
static const uint8_t SYNC_START_1 = 0x55;
static const uint8_t SYNC_END_0   = 0x55;
static const uint8_t SYNC_END_1   = 0xAA;

HardwareSerial& CameraManager::getSerial() { return CamSerial; }

int CameraManager::begin() {
    if (_initialized && _jpgBuf) return 0;

    // Ensure JPEG buffer is allocated immediately
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }

    // Set RX pin with pullup so it doesn't float before the camera starts driving it
    pinMode(ESP32CAM_UART_RX, INPUT_PULLUP);

    // Initialize UART to ESP32-CAM Module 2 with a safe 16KB ring buffer (sufficient for 2Mbps stream)
    CamSerial.setRxBufferSize(CAM_UART_RX_BUF_SIZE);
    CamSerial.begin(ESP32CAM_BAUD, SERIAL_8N1, ESP32CAM_UART_RX, ESP32CAM_UART_TX);
    
    delay(100);  // Quick wait for UART initialization
    
    // Send a newline to clear any incomplete command on ESP32-CAM and flush RX
    CamSerial.println();
    delay(50);
    flushRx();
    
    // Ping the ESP32-CAM Module 2 with multiple retries to allow for camera sensor startup
    bool alive = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (ping()) {
            alive = true;
            break;
        }
        delay(250);
        CamSerial.println();
        delay(20);
        flushRx();
    }
    
    if (!alive) {
        // Fallback: test 115200 baud in case ESP32-CAM was flashed with 115200 baud
        CamSerial.end();
        delay(30);
        CamSerial.begin(115200, SERIAL_8N1, ESP32CAM_UART_RX, ESP32CAM_UART_TX);
        delay(30);
        CamSerial.println();
        delay(20);
        flushRx();
        for (int attempt = 0; attempt < 3; attempt++) {
            if (ping()) {
                alive = true;
                break;
            }
            delay(150);
        }
        if (!alive) {
            // Restore configured primary baud for future attempts
            CamSerial.end();
            delay(20);
            CamSerial.begin(ESP32CAM_BAUD, SERIAL_8N1, ESP32CAM_UART_RX, ESP32CAM_UART_TX);
            return 1;
        }
    }
    
    _initialized = true;
    // Lock camera node to QQVGA (160x120) for reliable capture & streaming
    setResolution("QQVGA");
    return 0;
}

// ── Full-quality VGA capture ──
// ── Comprehensive Camera Node Diagnostic Status ──
bool CameraManager::getCamStatus(CamStatusInfo* info) {
    if (!info) return false;
    info->online = false;
    info->pingMs = 0;
    info->pid = 0;
    info->rawResponse[0] = '\0';
    strcpy(info->sensorName, "NOT DETECTED");

    // 1. Measure Ping latency
    flushRx();
    unsigned long start = millis();
    CamSerial.println("PING");
    
    unsigned long pingTimeout = 1500;
    while (millis() - start < pingTimeout) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == 'P') {
                String resp = CamSerial.readStringUntil('\n');
                resp.trim();
                if (resp == "ONG" || resp.startsWith("ONG")) {
                    info->online = true;
                    _initialized = true;
                    if (!_jpgBuf) {
                        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
                    }
                    info->pingMs = millis() - start;
                    break;
                }
            }
        }
        delay(1);
    }
    
    if (!info->online) {
        return false;
    }

    // 2. Query detailed hardware STATUS
    flushRx();
    CamSerial.println("STATUS");
    
    start = millis();
    String resp = "";
    while (millis() - start < 2000) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == '\n') {
                resp.trim();
                if (resp.startsWith("OK:PID:")) {
                    break;
                }
                resp = "";
            } else if (c >= 32 && c <= 126) {
                resp += c;
            }
        }
        delay(1);
    }

    if (resp.length() > 0) {
        strncpy(info->rawResponse, resp.c_str(), sizeof(info->rawResponse) - 1);
        
        // Format: OK:PID:0x2145:PSRAM:YES:PS_OK:YES:HEAP:182104:RES:640x480
        int pidIdx = resp.indexOf("PID:0x");
        if (pidIdx >= 0) {
            String pidStr = resp.substring(pidIdx + 6, pidIdx + 10);
            info->pid = (uint16_t)strtol(pidStr.c_str(), NULL, 16);
            if (info->pid == 0x2145) strcpy(info->sensorName, "GC2145 2MP");
            else if (info->pid == 0x2642 || info->pid == 0x2640) strcpy(info->sensorName, "OV2640 2MP");
            else if (info->pid == 0x7725) strcpy(info->sensorName, "OV7725 VGA");
            else if (info->pid == 0x7670) strcpy(info->sensorName, "OV7670 VGA");
            else if (info->pid == 0x0000) strcpy(info->sensorName, "NO SENSOR");
            else snprintf(info->sensorName, sizeof(info->sensorName), "PID 0x%04X", info->pid);
        }
        
        int psIdx = resp.indexOf("PSRAM:");
        if (psIdx >= 0) {
            info->psramFound = resp.substring(psIdx + 6).startsWith("YES");
        }
        int psOkIdx = resp.indexOf("PS_OK:");
        if (psOkIdx >= 0) {
            info->psramOk = resp.substring(psOkIdx + 6).startsWith("YES");
        }
        
        int heapIdx = resp.indexOf("HEAP:");
        if (heapIdx >= 0) {
            info->freeHeap = resp.substring(heapIdx + 5).toInt();
        }
        
        int resIdx = resp.indexOf("RES:");
        if (resIdx >= 0) {
            String resStr = resp.substring(resIdx + 4);
            int xIdx = resStr.indexOf('x');
            if (xIdx > 0) {
                info->width = resStr.substring(0, xIdx).toInt();
                info->height = resStr.substring(xIdx + 1).toInt();
            }
        }
    }
    
    return true;
}

// ── Test Probe Frame Capture ──
bool CameraManager::testProbeFrame(size_t* outSize, unsigned long* outMs, char* outDetail, size_t maxDetailLen) {
    if (outSize) *outSize = 0;
    if (outMs) *outMs = 0;
    if (outDetail && maxDetailLen > 0) outDetail[0] = '\0';
    
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }
    if (!_jpgBuf) {
        if (outDetail) snprintf(outDetail, maxDetailLen, "OOM JPEG buffer");
        return false;
    }
    
    if (!_initialized) {
        if (ping()) {
            _initialized = true;
        } else {
            if (outDetail) snprintf(outDetail, maxDetailLen, "Camera not initialized");
            return false;
        }
    }
    
    flushRx();
    setResolution("QQVGA");
    unsigned long start = millis();
    CamSerial.println("CAPTURE");
    size_t frameSize = 0;
    bool got = receiveFrame(&frameSize, 8000);
    
    // Probe frame capture (native QVGA 320x240)
    unsigned long elapsed = millis() - start;
    if (outMs) *outMs = elapsed;
    if (outSize) *outSize = frameSize;
    
    if (got && frameSize > 0) {
        if (outDetail) {
            snprintf(outDetail, maxDetailLen, "%u B JPEG in %lu ms [OK]", (unsigned)frameSize, elapsed);
        }
        return true;
    } else {
        if (outDetail) {
            if (strlen(lastCamMsg) > 0 && (strstr(lastCamMsg, "ERROR") || strstr(lastCamMsg, "Panic") || strstr(lastCamMsg, "Crash"))) {
                snprintf(outDetail, maxDetailLen, "%.40s", lastCamMsg);
            } else if (lastError == 4) {
                snprintf(outDetail, maxDetailLen, "Rx err: %u/%u B (%lu ms)", (unsigned)lastBytesRead, (unsigned)lastExpectedSize, elapsed);
            } else if (lastError == 5) {
                snprintf(outDetail, maxDetailLen, "Bad header 0x%02X%02X (%u B)", _jpgBuf[0], _jpgBuf[1], (unsigned)frameSize);
            } else {
                snprintf(outDetail, maxDetailLen, "Err=%d %s (%lu ms)", lastError, lastCamMsg, elapsed);
            }
        }
        return false;
    }
}

// ── Full-quality VGA capture ──
uint8_t* CameraManager::capture(size_t* outSize) {
    lastError = 0;
    lastJpegSize = 0;
    lastCaptureMs = 0;
    
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }
    if (!_jpgBuf) {
        lastError = -2;
        *outSize = 0;
        return nullptr;
    }
    
    if (!_initialized) {
        // Multi-attempt probe to reconnect with camera node
        for (int retry = 0; retry < 3; retry++) {
            if (ping()) {
                _initialized = true;
                break;
            }
            delay(150);
        }
        if (!_initialized) {
            // Last-ditch: probe with STATUS command
            flushRx();
            CamSerial.println("STATUS");
            delay(100);
            if (CamSerial.available()) {
                String s = CamSerial.readStringUntil('\n');
                if (s.indexOf("OK") >= 0 || s.indexOf("PID") >= 0) {
                    _initialized = true;
                }
            }
        }
        if (!_initialized) {
            // Fallback: test 115200 baud
            CamSerial.end();
            delay(30);
            CamSerial.begin(115200, SERIAL_8N1, ESP32CAM_UART_RX, ESP32CAM_UART_TX);
            delay(30);
            for (int retry = 0; retry < 2; retry++) {
                if (ping()) {
                    _initialized = true;
                    break;
                }
                delay(100);
            }
            if (!_initialized) {
                // Restore 460800 baud
                CamSerial.end();
                delay(20);
                CamSerial.begin(ESP32CAM_BAUD, SERIAL_8N1, ESP32CAM_UART_RX, ESP32CAM_UART_TX);
            }
        }
        if (!_initialized) {
            lastError = -1;
            snprintf(lastCamMsg, sizeof(lastCamMsg), "CAM UART no reply (RX4, TX5)");
            *outSize = 0;
            return nullptr;
        }
    }
    
    // If currently streaming, stop first
    if (_streaming) {
        stopStream();
        delay(100);
    }
    
    unsigned long startMs = millis();
    flushRx();
    CamSerial.println("CAPTURE");
    
    size_t frameSize = 0;
    // 5-second frame capture timeout (generous for camera sensor capture + JPEG encode)
    if (receiveFrame(&frameSize, 5000)) {
        lastJpegSize = frameSize;
        lastCaptureMs = millis() - startMs;
        *outSize = frameSize;
        return _jpgBuf;
    }
    
    // If capture failed, flush, re-enforce QQVGA, and retry once with 4s timeout
    delay(50);
    flushRx();
    setResolution("QQVGA");
    startMs = millis();
    CamSerial.println("CAPTURE");
    if (receiveFrame(&frameSize, 4000)) {
        lastJpegSize = frameSize;
        lastCaptureMs = millis() - startMs;
        *outSize = frameSize;
        return _jpgBuf;
    }
    
    *outSize = 0;
    return nullptr;
}

// ── Quick QVGA preview for TFT display ──
uint8_t* CameraManager::capturePreview(size_t* outSize) {
    lastError = 0;
    
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }
    if (!_jpgBuf) {
        lastError = -2;
        *outSize = 0;
        return nullptr;
    }
    if (!_initialized) {
        if (ping()) {
            _initialized = true;
        } else {
            lastError = -1;
            *outSize = 0;
            return nullptr;
        }
    }
    
    flushRx();
    CamSerial.println("PREVIEW");
    
    size_t frameSize = 0;
    if (receiveFrame(&frameSize, 20000)) {
        lastJpegSize = frameSize;
        *outSize = frameSize;
        return _jpgBuf;
    }
    
    *outSize = 0;
    return nullptr;
}

// ── Fast Live Viewfinder Capture (Low latency, tight timeouts) ──
uint8_t* CameraManager::capturePreviewLive(size_t* outSize) {
    lastError = 0;
    *outSize = 0;
    
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }
    if (!_jpgBuf || !_initialized) {
        return nullptr;
    }
    
    // Quick non-blocking drain of any leftover bytes in UART buffer
    drainRx();
    
    unsigned long startMs = millis();
    CamSerial.println("CAPTURE");
    
    size_t frameSize = 0;
    // For live viewfinder, frame arrives in ~150-350ms. Timeout at 1200ms.
    if (receiveFrame(&frameSize, 1200)) {
        lastJpegSize = frameSize;
        lastCaptureMs = millis() - startMs;
        *outSize = frameSize;
        return _jpgBuf;
    }
    
    *outSize = 0;
    return nullptr;
}

// ── Start continuous QVGA streaming ──
bool CameraManager::startStream() {
    if (!_initialized) return false;
    if (!_jpgBuf) {
        _jpgBuf = (uint8_t*)malloc(MAX_JPEG_SIZE);
    }
    
    drainRx();
    CamSerial.println("STREAM");
    
    // Wait for "OK:STREAM_STARTED" response (tight 250ms check)
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 250) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == '\n') {
                response.trim();
                if (response.indexOf("STREAM_STARTED") >= 0) {
                    _streaming = true;
                    return true;
                }
                response = "";
            } else if (c >= 32 && c <= 126) {
                response += c;
            }
        }
        ble.update();
        delay(1);
    }
    return false;
}

// ── Stop streaming ──
bool CameraManager::stopStream() {
    if (!_initialized) return false;
    
    CamSerial.println("STOP");
    _streaming = false;
    
    // Non-blocking quick drain of any in-flight bytes
    unsigned long start = millis();
    while (millis() - start < 100) {
        drainRx();
        ble.update();
        delay(2);
    }
    drainRx();
    return true;
}

// ── Read one frame from the stream ──
bool CameraManager::readStreamFrame(size_t* outSize) {
    if (!_initialized || !_jpgBuf || !_streaming) {
        *outSize = 0;
        return false;
    }
    
    return receiveFrame(outSize, 1200);
}

// ── Core frame receiver used by all capture methods ──
bool CameraManager::receiveFrame(size_t* outSize, unsigned long timeoutMs) {
    *outSize = 0;
    lastError = 0;
    lastBytesRead = 0;
    lastExpectedSize = 0;
    
    // Wait for sync header [0xAA 0x55]
    if (!waitForSync(timeoutMs)) {
        lastError = 1;  // Timeout waiting for sync
        return false;
    }
    
    // Read 4-byte frame size (little-endian)
    uint8_t sizeBuf[4];
    if (!readBytes(sizeBuf, 4, 1000)) {
        lastError = 2;  // Sync error (couldn't read size)
        return false;
    }
    
    uint32_t frameSize = sizeBuf[0] | (sizeBuf[1] << 8) | (sizeBuf[2] << 16) | (sizeBuf[3] << 24);
    
    // Validate frame size
    if (frameSize == 0 || frameSize > MAX_JPEG_SIZE) {
        lastError = 3;  // Size error
        return false;
    }
    
    // Read JPEG data with timeout matching the requested frame timeout
    if (!readBytes(_jpgBuf, frameSize, timeoutMs)) {
        lastError = 4;  // Incomplete data
        return false;
    }
    
    // Read and verify end sync [0x55 0xAA]
    uint8_t endSync[2];
    readBytes(endSync, 2, 200);
    
    // Verify JPEG header (0xFF 0xD8)
    if (frameSize >= 2 && _jpgBuf[0] == 0xFF && _jpgBuf[1] == 0xD8) {
        _jpgSize = frameSize;
        lastJpegSize = frameSize;
        *outSize = frameSize;
        return true;
    } else {
        lastError = 5;  // Bad header (not 0xFFD8)
        return false;
    }
}

void CameraManager::releaseFrame() {
    // Keep _jpgBuf permanently allocated (96KB) to prevent heap fragmentation,
    // memory leaks, or use-after-free crashes with BLEClient.
    _jpgSize = 0;
}

bool CameraManager::ping() {
    flushRx();
    // Clear any partial command buffer on ESP32-CAM first
    CamSerial.println();
    delay(20);
    drainRx();
    
    // We try up to 2 ping packets within 2.5 seconds
    for (int pingTry = 0; pingTry < 2; pingTry++) {
        CamSerial.println("PING");
        CamSerial.flush();
        
        unsigned long start = millis();
        String response = "";
        while (millis() - start < 1200) {
            if (CamSerial.available()) {
                char c = CamSerial.read();
                if (c == '\n' || c == '\r') {
                    response.trim();
                    if (response.indexOf("PONG") >= 0 || response.indexOf("CAMERA_") >= 0 || response.indexOf("OK") >= 0 || response.indexOf("READY") >= 0) {
                        _initialized = true;
                        return true;
                    }
                    if (response.length() > 0) {
                        strncpy(lastCamMsg, response.c_str(), sizeof(lastCamMsg) - 1);
                    }
                    response = "";
                } else if (c >= 32 && c <= 126) {
                    if (response.length() < 60) response += c;
                    // Dynamic early check: if PONG appeared anywhere, success!
                    if (response.indexOf("PONG") >= 0) {
                        _initialized = true;
                        return true;
                    }
                }
            }
            ble.update();
            delay(1);
        }
        delay(50);
    }
    return false;
}

bool CameraManager::setFlash(bool on) {
    if (!_initialized) return false;
    flushRx();
    CamSerial.println(on ? "FLASH:ON" : "FLASH:OFF");
    
    // Wait for "OK" response
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 1000) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == '\n') {
                response.trim();
                if (response == "OK" || response.startsWith("OK")) {
                    return true;
                }
                response = "";
            } else if (c >= 32 && c <= 126) {
                response += c;
            }
        }
        delay(1);
    }
    return false;
}

bool CameraManager::setResolution(const char* res) {
    if (!_initialized) return false;
    flushRx();
    
    String cmd = "RESOLUTION:";
    cmd += res;
    CamSerial.println(cmd);
    
    // Wait for "OK" response (can take up to 2.5s as sensor flushes frames)
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 2500) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == '\n') {
                response.trim();
                if (response == "OK" || response.startsWith("OK")) {
                    ESP_LOGI(TAG, "Resolution set to %s (resp: %s)", res, response.c_str());
                    return true;
                }
                if (response.startsWith("ERROR")) {
                    ESP_LOGE(TAG, "Failed to set resolution: %s", response.c_str());
                    return false;
                }
                response = "";
            } else if (c >= 32 && c <= 126) {
                response += c;
            }
        }
        delay(1);
    }
    return false;
}

// ── Private helpers ──

bool CameraManager::waitForSync(unsigned long timeoutMs) {
    unsigned long start = millis();
    unsigned long lastDataTime = start;  // Reset timeout on any received data
    int state = 0;  // 0=waiting for 0xAA, 1=waiting for 0x55
    String line = "";
    char lastLine[64] = {0};
    lastCamMsg[0] = '\0';
    size_t totalRx = 0;
    uint8_t firstBytes[4] = {0};
    size_t xCount = 0;
    
    // Use a "silence timeout" — reset the clock whenever we receive any data
    // This prevents timeout during long JPEG encoding where the ESP32-CAM
    const unsigned long SILENCE_TIMEOUT_MS = 15000;
    char lastPanicMsg[80] = {0};
    
    while (true) {
        unsigned long now = millis();
        
        // Check both total timeout AND silence timeout
        if (now - start > timeoutMs) break;
        if (now - lastDataTime > SILENCE_TIMEOUT_MS) break;

        if (CamSerial.available()) {
            uint8_t b = CamSerial.read();
            if (totalRx < 4) firstBytes[totalRx] = b;
            totalRx++;
            lastDataTime = millis();
            if (b == 'x' || b == 0x78) xCount++;
            
            if (state == 0 && b == SYNC_START_0) {
                state = 1;
            } else if (state == 1 && b == SYNC_START_1) {
                return true;  // Found sync [0xAA 0x55]!
            } else {
                state = (b == SYNC_START_0) ? 1 : 0;
                if (b == '\n') {
                    line.trim();
                    if (line.length() > 0) {
                        if (line.indexOf("panic") >= 0 || line.indexOf("Guru") >= 0 || 
                            line.indexOf("Exception") >= 0 || line.indexOf("abort") >= 0 ||
                            line.indexOf("wdt") >= 0) {
                            int pOpen = line.indexOf('(');
                            int pClose = line.indexOf(')', pOpen);
                            if (pOpen >= 0 && pClose > pOpen) {
                                String reason = line.substring(pOpen + 1, pClose);
                                snprintf(lastPanicMsg, sizeof(lastPanicMsg), "%.30s", reason.c_str());
                            } else {
                                strncpy(lastPanicMsg, line.c_str(), sizeof(lastPanicMsg) - 1);
                            }
                            strncpy(lastCamMsg, lastPanicMsg, sizeof(lastCamMsg) - 1);
                        } else if (line.startsWith("ERROR:")) {
                            strncpy(lastCamMsg, line.c_str(), sizeof(lastCamMsg) - 1);
                            return false; // Fast abort without waiting for 15s timeout
                        }
                        line = "";
                    }
                } else if (b >= 32 && b <= 126) {
                    if (line.length() < 60) line += (char)b;
                }
            }
        } else {
            ble.update();
            static unsigned long lastSyncHb = 0;
            if (millis() - lastSyncHb >= 4000) {
                lastSyncHb = millis();
                if (ble.isConnected()) {
                    ble.sendHeartbeat();
                }
            }
            delay(1);  // Yield to allow UART background DMA to fill
        }
    }
    
    // Check if the CAM crashed / rebooted
    bool camRebooted = (xCount > 25) || line.startsWith("rst:") || line.indexOf("boot:") >= 0 || line.startsWith("Rebooting");
    if (camRebooted) {
        if (lastPanicMsg[0] != '\0') {
            snprintf(lastCamMsg, sizeof(lastCamMsg), "Panic: %s", lastPanicMsg);
        } else {
            snprintf(lastCamMsg, sizeof(lastCamMsg), "CAM CRASH/REBOOT (power dip)");
        }
    } else if (lastCamMsg[0] == '\0') {
        if (totalRx == 0) {
            snprintf(lastCamMsg, sizeof(lastCamMsg), "NO DATA (0 bytes)");
        } else {
            snprintf(lastCamMsg, sizeof(lastCamMsg), "Sync timeout (rx %u B)", (unsigned)totalRx);
        }
    }
    return false;
}

bool CameraManager::readBytes(uint8_t* buf, size_t len, unsigned long timeoutMs) {
    unsigned long start = millis();
    unsigned long lastRx = millis();
    size_t received = 0;
    
    while (received < len) {
        int avail = CamSerial.available();
        if (avail > 0) {
            size_t toRead = min((size_t)avail, len - received);
            size_t got = CamSerial.read(buf + received, toRead);
            if (got > 0) {
                received += got;
                lastRx = millis();
            }
        } else {
            ble.update();
            static unsigned long lastReadHb = 0;
            if (millis() - lastReadHb >= 4000) {
                lastReadHb = millis();
                if (ble.isConnected()) {
                    ble.sendHeartbeat();
                }
            }
            delay(1);
        }
        
        // Timeout if no new data received for 1000 ms while stream is active
        if (millis() - lastRx > 1000) {
            break;
        }
        // Overall safety timeout
        if (millis() - start > timeoutMs) {
            break;
        }
    }
    
    lastBytesRead = received;
    lastExpectedSize = len;
    return (received == len);
}

void CameraManager::flushRx() {
    unsigned long start = millis();
    while (millis() - start < 50) {
        if (CamSerial.available()) {
            CamSerial.read();
            start = millis();  // Reset timer on each byte — drain fully
        }
        delay(0);
    }
    ble.update();
}

void CameraManager::drainRx() {
    while (CamSerial.available()) {
        CamSerial.read();
    }
}

bool CameraManager::resetCamera() {
    flushRx();
    CamSerial.println("RESET");
    CamSerial.flush();
    
    // Wait up to 1200ms for CAM reset response or boot banner
    unsigned long start = millis();
    String resp = "";
    bool gotAck = false;
    while (millis() - start < 1200) {
        if (CamSerial.available()) {
            char c = CamSerial.read();
            if (c == '\n') {
                resp.trim();
                if (resp.indexOf("RESET") >= 0 || resp.indexOf("OK") >= 0 || resp.indexOf("READY") >= 0) {
                    gotAck = true;
                }
                resp = "";
            } else if (c >= 32 && c <= 126) {
                resp += c;
            }
        }
        ble.update();
        delay(2);
    }
    
    // Fallback if RESET was not recognized (e.g. older CAM firmware): send REINIT
    if (!gotAck) {
        CamSerial.println("REINIT");
        CamSerial.flush();
        delay(300);
    }
    
    flushRx();
    return true;
}

bool CameraManager::reinit() {
    flushRx();
    CamSerial.println("REINIT");
    CamSerial.flush();
    delay(300);
    flushRx();
    return true;
}

