# AgroMate Hardware Configuration & Wiring Guide

This document lists the hardware components and pin configuration for wiring the ESP32-S3 robot controller.

## Bill of Materials (BOM)

| Component | Specification | Rationale |
|-----------|---------------|-----------|
| **Microcontroller** | ESP32-S3-DevKitC-1 (8MB Flash, No PSRAM) | Low-power, high-performance controller for data acquisition and UI rendering. |
| **Camera** | OV7670 Camera Module (Parallel, no FIFO) | Captures plant details in QVGA format. Frames are converted to JPEG in software. |
| **Display** | RM68140 3.5" 320x480 8-Bit Parallel TFT | Larger display area for diagnostics and animated AI face expressions. |
| **Microphone** | INMP441 I2S MEMS Microphone | High-fidelity digital voice recording for query dispatching. |
| **Speaker DAC** | MAX98357A I2S Class D Amplifier | Digital-to-analog converter driving the robot's voice speaker. |
| **RGB LEDs** | WS2812B Addressable LED strip (8 pixels) | Status indicator (Green = Pulse, Red = Solid Error, Cyan = Blinking, etc.) |
| **Buttons** | Onboard BOOT Button | Toggle screen views (Face animation ↔ Local telemetry dashboard). |

---

## ESP32-S3 System Pin-Outs — NO SHARED PINS

The following pin mappings are configured in `firmware/src/config.h` and must be followed exactly.
**Every GPIO is used by exactly one peripheral** — there are zero shared pins.

### 1. RM68140 Parallel TFT Display (11 GPIOs)
| Signal | GPIO | Notes |
|--------|------|-------|
| TFT_D0 | 43 | Data bus bit 0 |
| TFT_D1 | 44 | Data bus bit 1 |
| TFT_D2 | 39 | Data bus bit 2 |
| TFT_D3 | 40 | Data bus bit 3 |
| TFT_D4 | 41 | Data bus bit 4 |
| TFT_D5 | 42 | Data bus bit 5 |
| TFT_D6 | **48** | Data bus bit 6 |
| TFT_D7 | **45** | Data bus bit 7 |
| TFT_WR | 47 | Write strobe |
| TFT_DC | 38 | Data/Command select |
| TFT_CS | 21 | Chip Select |
| TFT_RST | 14 | Hardware Reset |
| TFT_RD | — | Not connected (write-only mode) |
| TFT_BL | — | Tied to 3.3V (always on) |

### 2. OV7670 Camera Module (13 GPIOs)
| Signal | GPIO | Notes |
|--------|------|-------|
| D0 | 4 | Data bit 0 |
| D1 | 5 | Data bit 1 |
| D2 | 6 | Data bit 2 |
| D3 | 7 | Data bit 3 |
| D4 | 15 | Data bit 4 |
| D5 | 16 | Data bit 5 |
| D6 | 17 | Data bit 6 |
| D7 | 18 | Data bit 7 |
| XCLK | 10 | Master clock output |
| PCLK | 13 | Pixel clock input |
| VSYNC | 12 | Vertical sync |
| HREF | 11 | Horizontal reference |
| SIOD (SDA) | 8 | SCCB/I2C Data |
| SIOC (SCL) | 9 | SCCB/I2C Clock |
| RESET | — | Tied to 3.3V |
| PWDN | — | Tied to GND |

### 3. I2S Audio — INMP441 Mic + MAX98357A Speaker (4 GPIOs)
Mic and speaker share BCLK and WS pins via time-multiplexed I2S. The driver uninstalls and reinstalls when switching between recording and playback.

| Signal | GPIO | Notes |
|--------|------|-------|
| BCLK | 2 | Bit clock (shared between mic and speaker) |
| WS/LRCLK | 1 | Word select (shared between mic and speaker) |
| SD_IN | 46 | INMP441 serial data → ESP32 |
| SD_OUT | 3 | ESP32 → MAX98357A DIN |

### 4. WS2812B RGB Status LEDs (1 GPIO)
| Signal | GPIO | Notes |
|--------|------|-------|
| DI (Data) | **26** | Dedicated pin, no conflicts |

### 5. Physical BOOT Button (1 GPIO)
| Signal | GPIO | Notes |
|--------|------|-------|
| BOOT | **0** | Onboard switch, active-low with internal pull-up. Interrupt-driven. |

---

## GPIO Usage Summary (30 GPIOs used)

| GPIO | Peripheral | Signal |
|------|-----------|--------|
| 0 | BOOT Button | BTN_MODE |
| 1 | Audio I2S | WS/LRCLK |
| 2 | Audio I2S | BCLK |
| 3 | Audio I2S | SD_OUT (Speaker) |
| 4 | Camera | D0 |
| 5 | Camera | D1 |
| 6 | Camera | D2 |
| 7 | Camera | D3 |
| 8 | Camera | SIOD (SDA) |
| 9 | Camera | SIOC (SCL) |
| 10 | Camera | XCLK |
| 11 | Camera | HREF |
| 12 | Camera | VSYNC |
| 13 | Camera | PCLK |
| 14 | TFT | RST |
| 15 | Camera | D4 |
| 16 | Camera | D5 |
| 17 | Camera | D6 |
| 18 | Camera | D7 |
| 21 | TFT | CS |
| 26 | WS2812B | DI |
| 38 | TFT | DC |
| 39 | TFT | D2 |
| 40 | TFT | D3 |
| 41 | TFT | D4 |
| 42 | TFT | D5 |
| 43 | TFT | D0 |
| 44 | TFT | D1 |
| 45 | TFT | D7 |
| 46 | Audio I2S | SD_IN (Mic) |
| 47 | TFT | WR |
| 48 | TFT | D6 |
