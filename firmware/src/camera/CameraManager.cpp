/**
 * AgroMate Camera Manager — OV7670 Implementation
 *
 * OV7670 specifics:
 *   - No hardware JPEG → capture as YUV422, software-convert to JPEG
 *   - XCLK frequency: 16 MHz (OV7670 supports up to 24 MHz)
 *   - Resolution: VGA (640x480) for maximum detail (~0.3MP native)
 *   - Frame buffers stored in PSRAM (or DRAM if no PSRAM)
 */

#include "CameraManager.h"
#include "img_converters.h"
#include <Wire.h>

int CameraManager::begin() {
  // ── Perform Hardware Reset and Power-Up ──
  if (CAM_PIN_PWDN >= 0) {
    pinMode(CAM_PIN_PWDN, OUTPUT);
    digitalWrite(CAM_PIN_PWDN, LOW); // Pull PWDN LOW to enable camera
    delay(10);
  }

  if (CAM_PIN_RESET >= 0) {
    Serial.println("[Camera] Performing physical reset pulse...");
    pinMode(CAM_PIN_RESET, OUTPUT);
    digitalWrite(CAM_PIN_RESET, LOW); // Reset camera
    delay(20);
    digitalWrite(CAM_PIN_RESET, HIGH); // Wake up camera
    delay(150);                        // Wait for hardware stabilization
  }

  // ── Start XCLK Clock via Camera Driver ──
  // The esp32-camera driver for ESP32-S3 supports internal LCD_CAM clock
  // output. We pass CAM_PIN_XCLK directly to the config below instead of manual
  // LEDC.

  // Enable internal pull-ups on SCCB (I2C) lines first
  pinMode(CAM_PIN_SIOD, INPUT_PULLUP);
  pinMode(CAM_PIN_SIOC, INPUT_PULLUP);
  delay(50);

  // Diagnostic check: verify that I2C lines are pulled HIGH
  if (digitalRead(CAM_PIN_SIOD) == LOW || digitalRead(CAM_PIN_SIOC) == LOW) {
    Serial.println("[Camera] Error: I2C/SCCB pins (SIOD/SIOC) are stuck LOW! "
                   "Check wiring.");
    return 1; // Return 1 (matches the stuck low error check in main.cpp)
  }

  // Set all camera data and control pins to INPUT_PULLUP to prevent floating
  // states during driver initialization (reduces boot-time noise
  // susceptibility)
  pinMode(CAM_PIN_VSYNC, INPUT_PULLUP);
  pinMode(CAM_PIN_HREF, INPUT_PULLUP);
  pinMode(CAM_PIN_PCLK, INPUT_PULLUP);
  pinMode(CAM_PIN_D0, INPUT_PULLUP);
  pinMode(CAM_PIN_D1, INPUT_PULLUP);
  pinMode(CAM_PIN_D2, INPUT_PULLUP);
  pinMode(CAM_PIN_D3, INPUT_PULLUP);
  pinMode(CAM_PIN_D4, INPUT_PULLUP);
  pinMode(CAM_PIN_D5, INPUT_PULLUP);
  pinMode(CAM_PIN_D6, INPUT_PULLUP);
  pinMode(CAM_PIN_D7, INPUT_PULLUP);

  // ── Camera detected, initialize driver ──
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk =
      CAM_PIN_XCLK; // Use hardware driver for internal XCLK generation
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = -1;
  config.pin_reset = -1;

  // OV7670: set XCLK to 10MHz for stable LCD_CAM DMA sampling
  config.xclk_freq_hz = 10000000;

  config.jpeg_quality = 12; // Default quality config

#ifdef BOARD_HAS_PSRAM
  config.pixel_format = PIXFORMAT_YUV422;
  config.frame_size = FRAMESIZE_VGA; // 640x480 — full native OV7670 resolution
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 2; // Double buffering for smoother captures
  config.grab_mode =
      CAMERA_GRAB_LATEST; // Always grab the latest frame (less motion blur)
  Serial.println("[Camera] Configured VGA (640x480) in PSRAM");
#else
  config.pixel_format =
      PIXFORMAT_YUV422; // Use true YUV422 format (2 bytes/pixel, 150KB total)
  config.frame_size = FRAMESIZE_QVGA;
  config.fb_location =
      CAMERA_FB_IN_DRAM; // Allocate frame buffer in internal DRAM
  config.fb_count = 1;   // Single buffering in DRAM
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  Serial.println(
      "[Camera] Configured YUV422 QVGA (320x240) in DRAM (No PSRAM)");
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] OV7670 init failed: 0x%x\n", err);
    esp_camera_deinit(); // Clean up I2C and LCD_CAM clocks completely
    _initialized = false;
    return (int)err; // Return the raw ESP32 error code
  }

  // Force internal pull-ups after the driver overrides them
  pinMode(CAM_PIN_VSYNC, INPUT_PULLUP);
  pinMode(CAM_PIN_HREF, INPUT_PULLUP);
  pinMode(CAM_PIN_PCLK, INPUT_PULLUP);
  pinMode(CAM_PIN_D0, INPUT_PULLUP);
  pinMode(CAM_PIN_D1, INPUT_PULLUP);
  pinMode(CAM_PIN_D2, INPUT_PULLUP);
  pinMode(CAM_PIN_D3, INPUT_PULLUP);
  pinMode(CAM_PIN_D4, INPUT_PULLUP);
  pinMode(CAM_PIN_D5, INPUT_PULLUP);
  pinMode(CAM_PIN_D6, INPUT_PULLUP);
  pinMode(CAM_PIN_D7, INPUT_PULLUP);

  // Adjust sensor settings for plant photography (if supported by sensor driver)
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
      if (s->set_brightness) s->set_brightness(s, 0);    // 0 is standard, prevents washed out foggy frames
      if (s->set_contrast)   s->set_contrast(s, 2);      // Max contrast for punchy colors & clear outlines
      if (s->set_saturation) s->set_saturation(s, 1);    // Vivid greens
      if (s->set_whitebal)   s->set_whitebal(s, 1);      // Auto white balance ON
      if (s->set_awb_gain)   s->set_awb_gain(s, 1);      // AWB gain ON
      Serial.println("[Camera] High-clarity sensor settings applied");
  }

  _initialized = true;
  Serial.println("[Camera] OV7670 initialized at QVGA YUV422");
  return 0;
}

uint8_t *CameraManager::capture(size_t *outSize) {
  if (!_initialized) {
    *outSize = 0;
    return nullptr;
  }

  _fb = esp_camera_fb_get();
  if (!_fb) {
    Serial.println("[Camera] Frame capture failed!");
    *outSize = 0;
    return nullptr;
  }

  Serial.printf("[Camera] Frame captured: %ux%u, %u bytes (YUV422)\n",
                _fb->width, _fb->height, _fb->len);

  // OV7670 always captures YUV422 — convert to JPEG via software
  if (_jpgBuf) {
    free(_jpgBuf);
    _jpgBuf = nullptr;
  }

  size_t jpgSize = 0;
  bool success =
      frame2jpg(_fb, 10, &_jpgBuf,
                &jpgSize); // Quality 10 = balanced detail and size for BLE
  if (success && _jpgBuf) {
    Serial.printf("[Camera] JPEG conversion: %u bytes\n", jpgSize);
    *outSize = jpgSize;
    return _jpgBuf;
  } else {
    Serial.println("[Camera] Software JPEG conversion failed!");
    *outSize = 0;
    return nullptr;
  }
}

camera_fb_t *CameraManager::captureRaw() {
  if (!_initialized)
    return nullptr;
  if (_fb) {
    esp_camera_fb_return(_fb);
    _fb = nullptr;
  }
  _fb = esp_camera_fb_get();
  return _fb;
}

void CameraManager::releaseFrame() {
  if (_fb) {
    esp_camera_fb_return(_fb);
    _fb = nullptr;
  }
  if (_jpgBuf) {
    free(_jpgBuf);
    _jpgBuf = nullptr;
  }
}
