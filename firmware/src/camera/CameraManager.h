#pragma once
/**
 * AgroMate Camera Manager — OV7670 parallel camera
 * 
 * The OV7670 does NOT have a hardware JPEG encoder, so images are
 * captured in YUV422 format and converted to JPEG via software.
 */

#include <Arduino.h>
#include "esp_camera.h"
#include "../config.h"

class CameraManager {
public:
    int begin();
    uint8_t* capture(size_t* outSize);
    camera_fb_t* captureRaw();
    void releaseFrame();
    bool isAvailable() { return _initialized; }
    camera_fb_t* getFB() { return _fb; }
    
    // Diagnostic info for on-screen debugging
    int lastError = 0;          // 0=ok, 1=fb_get_failed, 2=jpg_convert_failed
    size_t lastRawSize = 0;
    int warmupSuccessCount = 0;
    int warmupFailCount = 0;
private:
    bool _initialized = false;
    camera_fb_t* _fb = nullptr;
    uint8_t* _jpgBuf = nullptr;
};
