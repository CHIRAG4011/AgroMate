# AgroMate Hardware Configuration & Wiring Guide

This document lists the hardware components and pin configuration for wiring the ESP32-S3 robot controller.

## Bill of Materials (BOM)

| Component | Specification | Rationale |
|-----------|---------------|-----------| 
| **Microcontroller** | ESP32-S3-DevKitC-1 (8MB Flash, No PSRAM) | Low-power, high-performance controller for data acquisition and UI rendering. |
| **Camera** | ESP32-CAM Module 2 (AI-Thinker, OV2640, 4MB PSRAM) | Captures plant images in hardware JPEG at up to VGA (640×480). Connected via UART. |
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
| TFT_RD | 20 | Read strobe (GPIO-driven HIGH before TFT init) |
| TFT_BL | — | Tied to 3.3V (always on) |

### 2. ESP32-CAM Module 2 (2 GPIOs — UART Communication)

The ESP32-CAM Module 2 is a **separate microcontroller** (ESP32 + OV2640 camera + 4MB PSRAM) that handles camera initialization and hardware JPEG encoding internally. The main ESP32-S3 communicates with it via high-speed UART (1 Mbps).

| Signal | GPIO | Notes |
|--------|------|-------|
| UART RX | 4 | ESP32-S3 receives JPEG data from ESP32-CAM TX |
| UART TX | 5 | ESP32-S3 sends commands to ESP32-CAM RX |

**ESP32-CAM Module 2 → ESP32-S3 Wiring:**

| ESP32-CAM Pin | ESP32-S3 Pin | Notes |
|---------------|-------------|-------|
| U0T (TX) | GPIO 4 | Camera sends JPEG frames to main board |
| U0R (RX) | GPIO 5 | Main board sends commands (CAPTURE, PING, etc.) |
| GND | GND | Common ground (required!) |
| 5V | 5V | Power from shared 5V rail |

> **Note:** GPIOs 6-18 are now **FREE** (previously used by the OV7670 parallel camera bus). These can be used for additional sensors or features.

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

## GPIO Usage Summary (19 GPIOs used, 11 freed from OV7670)

| GPIO | Peripheral | Signal |
|------|-----------|--------|
| 0 | BOOT Button | BTN_MODE |
| 1 | Audio I2S | WS/LRCLK |
| 2 | Audio I2S | BCLK |
| 3 | Audio I2S | SD_OUT (Speaker) |
| 4 | ESP32-CAM Module 2 UART | RX (receive JPEG) |
| 5 | ESP32-CAM Module 2 UART | TX (send commands) |
| 6-18 | **FREE** | Available for expansion |
| 14 | TFT | RST |
| 20 | TFT | RD (Read strobe, driven HIGH) |
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

---

## Flashing the ESP32-CAM Module 2

The ESP32-CAM has **no USB port**. There are two methods to flash it:

- **Method A (Recommended):** Use the ESP32-S3 DevKitC's onboard USB-UART bridge (no extra hardware needed)
- **Method B:** Use an external FTDI USB-to-Serial adapter

---

### Method A: Flash via ESP32-S3 DevKitC Bridge (Recommended)

The ESP32-S3 DevKitC-1 has a **USB-UART bridge chip** (CP2102/CH340) on the board. By holding the ESP32-S3 in reset (`EN → GND`), the bridge chip becomes a **standalone FTDI adapter** — no passthrough sketch needed!

#### How It Works

```
PC ──USB──► DevKitC USB-UART Bridge ──GPIO 43/44──► ESP32-CAM
                                     (ESP32-S3 disabled via EN→GND)
```

When `EN → GND`:
- The ESP32-S3 is held in **permanent reset** (all GPIOs become high-impedance)
- The USB-UART bridge chip stays powered and active on COM6
- GPIO 43 (bridge RX) and GPIO 44 (bridge TX) are free to wire to the ESP32-CAM

#### Flashing Wiring

| ESP32-S3 Pin | ESP32-CAM Pin | Purpose |
|-------------|---------------|---------|
| **EN → GND** | — | Hold ESP32-S3 in reset (disables it) |
| **GPIO 43** | **U0T (TX)** | ESP32-CAM TX → Bridge RX → PC |
| **GPIO 44** | **U0R (RX)** | PC → Bridge TX → ESP32-CAM RX |
| **GND** | **GND** | Common ground (required!) |
| **5V** | **5V** | Power (or use external 5V supply) |
| — | **GPIO 0 → GND** | ⚠️ **Flash mode (connect only during flashing!)** |

```
ESP32-S3 DevKitC                    ESP32-CAM
┌──────────────┐                   ┌──────────────┐
│  EN → GND    │ (hold in reset)   │              │
│              │                   │              │
│  GPIO 43 ────┼───────────────────┼── U0T (TX)   │
│  GPIO 44 ────┼───────────────────┼── U0R (RX)   │
│  GND ────────┼───────────────────┼── GND        │
│  5V  ────────┼───────────────────┼── 5V         │
│              │                   │  GPIO 0 → GND│
└──────────────┘                   └──────────────┘
```

#### Step-by-Step Procedure

**Step 1:** Disconnect the ESP32-S3 from USB

**Step 2:** Wire as shown above:
- ESP32-S3 **EN → GND** (breadboard)
- ESP32-S3 **GPIO 43 → ESP32-CAM U0T**
- ESP32-S3 **GPIO 44 → ESP32-CAM U0R**
- **GND → GND** (common ground, use breadboard rail)
- **5V → 5V** (or use external power for ESP32-CAM)
- ESP32-CAM **GPIO 0 → GND** (flash mode)

**Step 3:** Plug the ESP32-S3 DevKitC into USB (powers on the bridge chip + ESP32-CAM)

**Step 4:** Flash the ESP32-CAM:
```bash
cd firmware/esp32cam
pio run -t upload
```

**Step 5:** After upload completes, disconnect everything:
- Remove **EN → GND** wire (ESP32-S3 boots again)
- Remove **GPIO 0 → GND** wire on ESP32-CAM (normal boot)
- Remove GPIO 43/44 wires (no longer needed)

**Step 6:** Rewire for **production** operation (see below) and press RST on ESP32-CAM

> ⚠️ **After flashing:** You must re-flash the ESP32-S3 with the AgroMate firmware if it was already flashed, since the EN→GND reset does not erase its firmware — it just held it paused.

---

### Method B: Flash via External FTDI Adapter

If you have a CP2102 or CH340 USB-to-Serial adapter, you can flash the ESP32-CAM directly.

#### Wiring (ESP32-CAM → FTDI)

| ESP32-CAM Pin | FTDI Pin | Notes |
|---------------|----------|-------|
| 5V | 5V (VCC) | ⚠️ Must be 5V, not 3.3V |
| GND | GND | Common ground |
| U0R (RX) | TX | FTDI TX → ESP32-CAM RX |
| U0T (TX) | RX | ESP32-CAM TX → FTDI RX |
| GPIO 0 | GND | ⚠️ **Connect to GND only during flashing!** |

#### Flashing Steps

1. Wire the FTDI adapter as shown above (GPIO 0 → GND for flash mode)
2. Open the `firmware/esp32cam/` project in PlatformIO
3. Set `upload_port` in `platformio.ini` to your FTDI's COM port
4. Run `pio run -t upload`
5. After upload completes: **disconnect GPIO 0 from GND**
6. Press the RST button on the ESP32-CAM to boot normally
7. Rewire for production operation (see below)

---

### Production Wiring (After Flashing)

After flashing, rewire the ESP32-CAM for normal UART communication with the ESP32-S3:

| ESP32-S3 Pin | ESP32-CAM Pin | Notes |
|-------------|---------------|-------|
| GPIO 4 (RX) | U0T (TX) | Camera sends JPEG data to main board |
| GPIO 5 (TX) | U0R (RX) | Main board sends commands to camera |
| GND | GND | Common ground |
| 5V | 5V | Power (or external supply + shared GND) |

> ⚠️ **Flashing uses GPIO 43/44** but **production uses GPIO 4/5**. Make sure to rewire after flashing!

---

### Power Troubleshooting

If the ESP32-CAM does **not power on** from the ESP32-S3's 5V pin:

| Issue | Fix |
|-------|-----|
| **Insufficient USB current** | USB may not supply enough for both boards (~800mA total). Use a powered USB hub or external 5V supply. |
| **Try 3.3V instead** | Connect ESP32-S3 `3V3` → ESP32-CAM `3V3` (bypasses onboard regulator). |
| **Loose jumper wire** | Swap the wire. Use female-to-female DuPont wires instead of breadboard. |
| **Wrong pin** | Some boards label pins differently. Verify with a multimeter. |
| **Separate power source** | Power ESP32-CAM from a separate USB/power bank. **GND must still be shared.** |

**Quick test:** The **red LED on the back** of the ESP32-CAM should light up when powered. If it doesn't, the power connection is bad.

---

## TFT Display Diagnostics & Color Test Tool

If the 3.5" TFT display shows a **solid white screen** or does not respond, use the dedicated diagnostic tool in `tools/tft_test/`.

### Why the Screen Stays White (Root Causes & Solutions)

1. **RD Pin is Floating or Grounded (Most Common Cause):**
   - 8-bit parallel TFT modules require the **RD (Read)** pin to be **driven HIGH** for write operations.
   - If RD is floating or connected to GND, the controller locks up in READ mode and rejects all commands and pixel data.
   - **Fix:** Connect the TFT `RD` pin to **ESP32-S3 GPIO 20**. The firmware drives it HIGH during boot.

2. **GPIO 43/44 Flashing Conflict:**
   - On the ESP32-S3 DevKitC-1, GPIO 43 (TX) and GPIO 44 (RX) are wired to the onboard USB-UART bridge (COM6).
   - If TFT D0 and D1 are connected during firmware flashing, the upload fails with `Timed out waiting for packet header`.
   - **Fix:** Unplug D0 (GPIO 43) and D1 (GPIO 44) while flashing, then plug them back in after flashing is done.

3. **Controller Driver Mismatch:**
   - Most 3.5" 480x320 panels use RM68140, but clones often use ILI9486, ILI9488, or ST7796.
   - The test tool includes pre-configured targets for all 4 controllers.

### How to Flash and Run the Color Test Tool

**Step 1:** Disconnect D0 (GPIO 43) and D1 (GPIO 44) wires from the ESP32-S3.

**Step 2:** Ensure TFT `RD` is connected to **ESP32-S3 GPIO 20**.

**Step 3:** Upload the test firmware (starts with RM68140):
```bash
cd tools/tft_test
pio run -t upload -e rm68140
```

**Step 4:** Reconnect D0 to GPIO 43 and D1 to GPIO 44.

**Step 5:** Press the **RST** button on the ESP32-S3 DevKitC.

### What You Should See
- The screen will cycle through full-screen test colors:
  1. **RED** (0xF800) — 1.5s
  2. **GREEN** (0x07E0) — 1.5s
  3. **BLUE** (0x001F) — 1.5s
  4. **YELLOW** (0xFFE0) — 1.5s
  5. **CYAN** (0x07FF) — 1.5s
  6. **MAGENTA** (0xF81F) — 1.5s
  7. **WHITE** (0xFFFF) — 1.5s
  8. **BLACK** (0x0000) — 1.5s
  9. **SMPTE 8-Color Bars** test pattern with white border and pinout information.
- Status indicator LED on GPIO 26 toggles on each color change so you can verify the CPU is active.

### Testing Alternative Drivers (if screen stays white with RM68140)
If the screen remains white with `rm68140`, test the other common controller targets:
```bash
# Try ILI9486
pio run -t upload -e ili9486

# Try ILI9488
pio run -t upload -e ili9488

# Try ST7796
pio run -t upload -e st7796
```
*(Always disconnect GPIO 43/44 before running the upload command!)*

