# AgroMate Hardware Rules

## TFT Display Pin Configuration
- The TFT RD (Read) pin is connected to **GPIO 20** on the ESP32-S3.
- It must be driven HIGH by GPIO before TFT initialization.
- The `TFT_RD` build flag in all `platformio.ini` files must be set to `20` (not `-1`).
- The `TFT_RD_PIN` in `config.h` must be `20`.

## ESP32-CAM Module 2
- The camera module is labeled "ESP32 cam module 2" / "ESP32-CAM Module 2" across the codebase.
- Communication uses UART on GPIO 4 (RX) and GPIO 5 (TX).
- The ESP32-CAM is flashed using the EN→GND bridge method (no FTDI adapter needed).

## GPIO 43/44 Flashing Conflict
- GPIO 43 (TFT_D0) and GPIO 44 (TFT_D1) are shared with the USB-UART bridge.
- These wires MUST be disconnected before uploading any firmware to the ESP32-S3.
- After uploading, reconnect them and press RST.
