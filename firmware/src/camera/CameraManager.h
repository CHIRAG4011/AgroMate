#pragma once
/**
 * AgroMate Camera Manager — ESP32-CAM Module 2 UART Client
 * 
 * Communicates with a separate ESP32 cam module 2 over high-speed UART.
 * The ESP32-CAM module 2 handles camera hardware (OV2640) and JPEG encoding internally.
 * This manager sends commands and receives JPEG image data.
 * 
 * Protocol:
 *   TX (to ESP32-CAM):  "CAPTURE\n", "PREVIEW\n", "STREAM\n", "STOP\n", "PING\n", etc.
 *   RX (from ESP32-CAM): [0xAA][0x55][4-byte size LE][JPEG data][0x55][0xAA]
 */

#include <Arduino.h>
#include "../config.h"

// Maximum JPEG frame size (96KB accommodates full VGA 640x480 JPEG from GC2145)
#define MAX_JPEG_SIZE (96 * 1024)
#define CAM_UART_RX_BUF_SIZE 16384

struct CamStatusInfo {
    bool online = false;
    unsigned long pingMs = 0;
    uint16_t pid = 0;
    char sensorName[28] = "NOT DETECTED";
    bool psramFound = false;
    bool psramOk = false;
    uint32_t freeHeap = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    char rawResponse[96] = {0};
};

class CameraManager {
public:
    int begin();
    
    // Diagnostic query & probe functions
    bool getCamStatus(CamStatusInfo* info);
    bool testProbeFrame(size_t* outSize, unsigned long* outMs, char* outDetail, size_t maxDetailLen);
    
    // Full-quality VGA capture (for AI analysis, BLE transfer)
    uint8_t* capture(size_t* outSize);
    
    // Quick QVGA preview capture (for TFT display, ~5-10KB)
    uint8_t* capturePreview(size_t* outSize);
    
    // Streaming control (for live TFT preview)
    bool startStream();   // Switch to QVGA + start continuous streaming
    bool stopStream();    // Stop streaming + restore VGA
    bool readStreamFrame(size_t* outSize);  // Read one frame during streaming
    
    void releaseFrame();
    bool isAvailable() { return _initialized; }
    
    // Control commands
    bool setFlash(bool on);
    bool setResolution(const char* res);  // "QVGA", "VGA", "SVGA", "XGA"
    bool ping();
    bool resetCamera();
    bool reinit();
    
    // Buffer access (for TFT rendering and BLE transfer)
    uint8_t* getBuffer() { return _jpgBuf; }
    
    // UART access & fast live capture (for Viewfinder)
    void flushRx();
    void drainRx();
    uint8_t* capturePreviewLive(size_t* outSize);
    HardwareSerial& getSerial();
    
    // Diagnostic info for on-screen debugging
    int lastError = 0;          // 0=ok, -1=not_init, 1=timeout, 2=sync_error, 3=size_error
    char lastCamMsg[64] = {0};  // Text response from ESP32-CAM (e.g. ERROR:FB_GET_FAILED)
    size_t lastJpegSize = 0;
    size_t lastBytesRead = 0;
    size_t lastExpectedSize = 0;
    int warmupSuccessCount = 0;
    int warmupFailCount = 0;
    unsigned long lastCaptureMs = 0;  // Time taken for last capture
    
private:
    bool _initialized = false;
    bool _streaming = false;
    uint8_t* _jpgBuf = nullptr;
    size_t _jpgSize = 0;
    
    // Internal frame receiver — used by capture, capturePreview, and readStreamFrame
    bool receiveFrame(size_t* outSize, unsigned long timeoutMs);
    
    bool waitForSync(unsigned long timeoutMs);
    bool readBytes(uint8_t* buf, size_t len, unsigned long timeoutMs);
};
