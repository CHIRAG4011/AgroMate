#pragma once
// ============================================
// AgroMate ESP32-S3 Firmware — Hardware Configuration
// ============================================
// Board: ESP32-S3 DevKitC-1 (8MB Flash, No PSRAM)
// Display: 3.5" RM68140 320x480 8-bit Parallel TFT
// Camera: OV7670 (parallel, no JPEG encoder)
// Audio In: INMP441 I2S MEMS Microphone
// Audio Out: MAX98357A I2S DAC + Class-D Amplifier
// ============================================

// ── Board Selection ──
#define BOARD_ESP32_S3

// ── Display (RM68140 480x320 8-bit Parallel TFT) ──
// Physical screen dimensions (landscape after rotation)
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// Face sprite dimensions (rendered in center of screen)
// 320x160 @ 16-bit = 100KB — fits comfortably in internal SRAM
#define FACE_SPRITE_W 320
#define FACE_SPRITE_H 160

// TFT parallel data bus (must match build_flags in platformio.ini)
// ── NO SHARED PINS — each GPIO is used by exactly one peripheral ──
#define TFT_D0_PIN 43
#define TFT_D1_PIN 44
#define TFT_D2_PIN 39
#define TFT_D3_PIN 40
#define TFT_D4_PIN 41
#define TFT_D5_PIN 42
#define TFT_D6_PIN 48
#define TFT_D7_PIN 45

// TFT control pins
#define TFT_WR_PIN 47  // Write strobe
#define TFT_RD_PIN -1  // Read strobe (not connected — write-only)
#define TFT_DC_PIN 38  // Data/Command (a.k.a. RS)
#define TFT_CS_PIN 21  // Chip Select
#define TFT_RST_PIN 14 // Hardware reset
#define TFT_BL_PIN -1  // Backlight (hardwired to VCC)

// ── Camera (OV7670 — 8-bit parallel + SCCB/I2C) ──
#define CAMERA_MODEL_OV7670

#define CAM_PIN_PWDN -1  // Not connected (tie to GND)
#define CAM_PIN_RESET -1 // Physically tied to 3.3V
#define CAM_PIN_XCLK 10  // Master clock output
#define CAM_PIN_SIOD 8   // SCCB Data (I2C SDA)
#define CAM_PIN_SIOC 9   // SCCB Clock (I2C SCL)
#define CAM_PIN_D7 18    // Data bit 7
#define CAM_PIN_D6 17    // Data bit 6
#define CAM_PIN_D5 16    // Data bit 5
#define CAM_PIN_D4 15    // Data bit 4
#define CAM_PIN_D3 7     // Data bit 3
#define CAM_PIN_D2 6     // Data bit 2
#define CAM_PIN_D1 5     // Data bit 1
#define CAM_PIN_D0 4     // Data bit 0
#define CAM_PIN_VSYNC 12 // Vertical sync
#define CAM_PIN_HREF 11  // Horizontal reference
#define CAM_PIN_PCLK 13  // Pixel clock

// ── Audio I2S (Time-Multiplexed — INMP441 + MAX98357A share I2S0) ──
// Both mic and speaker share BCLK and WS pins on I2S_NUM_0.
// The driver switches between RX mode (recording) and TX mode (playback).
#define AUDIO_I2S_PORT I2S_NUM_1
#define AUDIO_I2S_BCLK 2   // Bit clock
#define AUDIO_I2S_WS 1     // Word select / LRCLK
#define AUDIO_I2S_SD_IN 46 // INMP441 serial data out → ESP32 data in
#define AUDIO_I2S_SD_OUT 3 // ESP32 data out → MAX98357A DIN
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BUFFER_SIZE 1024

// ── RGB LEDs (WS2812B) ──
#define LED_PIN 26 // Dedicated GPIO for LED data (no conflicts)
#define LED_COUNT 8
#define LED_BRIGHTNESS 50

// ── Buttons ──
#define BTN_MODE 0 // BOOT button on DevKitC-1 (dedicated — no conflicts)
// Scan and Talk functions triggered via BLE from phone/laptop app

// ── Battery Monitor ──
// No dedicated ADC pin available — all GPIOs allocated to peripherals.
// Battery monitoring disabled; can be re-enabled with an I2C INA219 sensor.
#define BATTERY_ADC_PIN -1 // Disabled
#define BATTERY_DIVIDER_RATIO 2.0
#define BATTERY_FULL_MV 4200
#define BATTERY_EMPTY_MV 3200
#define CHARGING_DET_PIN -1

// ── BLE Configuration ──
#define BLE_DEVICE_NAME "AgroMate-Robot"
#define BLE_SERVICE_UUID "12345678-1234-5678-1234-56789abcde01"
#define BLE_CHAR_CMD_TX "12345678-1234-5678-1234-56789abcdf01"
#define BLE_CHAR_CMD_RX "12345678-1234-5678-1234-56789abcdf02"
#define BLE_CHAR_IMG_TX "12345678-1234-5678-1234-56789abcdf03"
#define BLE_CHAR_AUDIO_TX "12345678-1234-5678-1234-56789abcdf04"
#define BLE_CHAR_AUDIO_RX "12345678-1234-5678-1234-56789abcdf05"
#define BLE_CHAR_BATTERY "12345678-1234-5678-1234-56789abcdf06"
#define BLE_CHAR_SENSORS "12345678-1234-5678-1234-56789abcdf07"
#define BLE_CHAR_HEARTBEAT "12345678-1234-5678-1234-56789abcdf08"
#define BLE_MTU_SIZE 512

// ── Timing ──
#define HEARTBEAT_INTERVAL_MS 5000
#define SENSOR_READ_INTERVAL_MS 10000
#define DISPLAY_FPS 30
#define IDLE_SLEEP_TIMEOUT_MS 300000 // 5 minutes
#define BLINK_MIN_INTERVAL_MS 6000
#define BLINK_MAX_INTERVAL_MS 15000

// ── FreeRTOS Task Priorities ──
#define TASK_BLE_PRIORITY 3
#define TASK_DISPLAY_PRIORITY 2
#define TASK_AUDIO_PRIORITY 3
#define TASK_SENSOR_PRIORITY 1
#define TASK_STACK_SIZE 8192
