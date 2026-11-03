#pragma once
// ============================================
// AgroMate ESP32-S3 Firmware — Hardware Configuration
// ============================================
// Board: ESP32-S3 DevKitC-1 (8MB Flash, No PSRAM)
// Display: 3.5" RM68140 320x480 8-bit Parallel TFT
// Camera: ESP32 cam module 2 (OV2640, hardware JPEG, connected via UART)
// Audio In: INMP441 I2S MEMS Microphone
// Audio Out: MAX98357A I2S DAC + Class-D Amplifier
// ============================================

// ── Board Selection ──
#define BOARD_ESP32_S3

// ── Display (RM68140 320x480 8-bit Parallel TFT) ──
// Physical screen dimensions (Portrait mode)
#define SCREEN_ROTATION 0   // 0 = Portrait (320x480), 2 = Inverted Portrait
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480

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
#define TFT_RD_PIN 20  // Read strobe (GPIO 20, driven HIGH before TFT init)
#define TFT_DC_PIN 38  // Data/Command (a.k.a. RS)
#define TFT_CS_PIN 21  // Chip Select
#define TFT_RST_PIN 14 // Hardware reset
#define TFT_BL_PIN -1  // Backlight (hardwired to VCC)

// ── Camera (ESP32 cam module 2 via UART) ──
// The ESP32 cam module 2 is a separate microcontroller (ESP32 + OV2640) that handles
// camera initialization and hardware JPEG encoding internally.
// Communication is via high-speed UART — main board sends commands, receives JPEG.
#define CAMERA_MODEL_ESP32CAM
#define CAMERA_MODEL_ESP32CAM_2
#define ESP32CAM_UART_RX 4       // ESP32-S3 receives JPEG data from ESP32-CAM TX
#define ESP32CAM_UART_TX 5       // ESP32-S3 sends commands to ESP32-CAM RX
#define ESP32CAM_BAUD    460800  // 460.8 Kbps — reliable on breadboard wiring
#define ESP32CAM_UART_NUM 2      // Use HardwareSerial (Serial2)
// Pins 6-18 are now FREE (previously used by OV7670 parallel bus)

// ── Audio I2S (Dedicated pins for Mic and Speaker) ──
#define AUDIO_I2S_PORT I2S_NUM_1

// INMP441 Microphone (I2S RX)
#define MIC_I2S_BCLK 2      // Microphone Bit Clock
#define MIC_I2S_WS 1        // Microphone Word Select / LRCLK
#define AUDIO_I2S_SD_IN 46  // INMP441 serial data out → ESP32 data in

// MAX98357A Amplifier (I2S TX - Valid ESP32-S3 Pins)
#define SPK_I2S_BCLK 3      // Speaker Bit Clock (ESP32-S3 GPIO 3)
#define SPK_I2S_WS 35       // Speaker Word Select / LRCLK (ESP32-S3 GPIO 35)
#define AUDIO_I2S_SD_OUT 36 // ESP32-S3 data out → MAX98357A DIN (ESP32-S3 GPIO 36)
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BUFFER_SIZE 1024

// ── Buttons ──
#define BTN_MODE 0         // On-board BOOT button (GPIO 0: toggle screen / hold for scan)
#define BTN_EXTERNAL_PIN 7 // Dedicated External Push Button (GPIO 7: instant camera image capture)
// Scan and Talk functions also triggerable via BLE from phone/laptop app

// ── Battery Monitor ──
// No dedicated ADC pin available — all GPIOs allocated to peripherals.
// Battery monitoring disabled; can be re-enabled with an I2C INA219 sensor.
#define BATTERY_ADC_PIN -1 // Disabled
#define BATTERY_DIVIDER_RATIO 2.0
#define BATTERY_FULL_MV 4200
#define BATTERY_EMPTY_MV 3200
#define CHARGING_DET_PIN -1

// ── BLE Configuration ──
#define BLE_DEVICE_NAME "AgroMate"
#define BLE_SERVICE_UUID "12345678-1234-5678-1234-56789abcde88"
#define BLE_CHAR_CMD_TX "12345678-1234-5678-1234-56789abcdf81"
#define BLE_CHAR_CMD_RX "12345678-1234-5678-1234-56789abcdf82"
#define BLE_CHAR_IMG_TX "12345678-1234-5678-1234-56789abcdf83"
#define BLE_CHAR_AUDIO_TX "12345678-1234-5678-1234-56789abcdf84"
#define BLE_CHAR_AUDIO_RX "12345678-1234-5678-1234-56789abcdf85"
#define BLE_CHAR_BATTERY "12345678-1234-5678-1234-56789abcdf86"
#define BLE_CHAR_SENSORS "12345678-1234-5678-1234-56789abcdf87"
#define BLE_CHAR_HEARTBEAT "12345678-1234-5678-1234-56789abcdf88"
#define BLE_MTU_SIZE 512

// ── Timing ──
#define HEARTBEAT_INTERVAL_MS 5000
#define SENSOR_READ_INTERVAL_MS 10000
#define DISPLAY_FPS 40
#define IDLE_SLEEP_TIMEOUT_MS 300000 // 5 minutes
#define BLINK_MIN_INTERVAL_MS 3000
#define BLINK_MAX_INTERVAL_MS 15000

// ── FreeRTOS Task Priorities ──
#define TASK_BLE_PRIORITY 3
#define TASK_DISPLAY_PRIORITY 2
#define TASK_AUDIO_PRIORITY 3
#define TASK_SENSOR_PRIORITY 1
#define TASK_STACK_SIZE 8192
