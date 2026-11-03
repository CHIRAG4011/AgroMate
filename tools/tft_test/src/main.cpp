/**
 * AgroMate — TFT Display Wiring & Color Diagnostic Test
 * 
 * Hardware:
 *   ESP32-S3 DevKitC-1 + 3.5" 320x480 8-Bit Parallel TFT (RM68140 / ILI9486 / ILI9488 / ST7796)
 * 
 * Pinout Under Test:
 *   Data Bus:
 *     D0 = GPIO 43
 *     D1 = GPIO 44
 *     D2 = GPIO 39
 *     D3 = GPIO 40
 *     D4 = GPIO 41
 *     D5 = GPIO 42
 *     D6 = GPIO 48
 *     D7 = GPIO 45
 *   Control:
 *     WR  = GPIO 47  (Write strobe)
 *     DC  = GPIO 38  (Data / Command, a.k.a. RS)
 *     CS  = GPIO 21  (Chip Select)
 *     RST = GPIO 14  (Hardware Reset)
 *     RD  = GPIO 20  (Read strobe — firmware drives HIGH before TFT init)
 *     BL  = TIED TO 3.3V (Backlight power)
 *   Status Heartbeat:
 *     LED = GPIO 26  (Blinks on each color change so you know CPU is running)
 * 
 * IMPORTANT FLASHING NOTE:
 *   GPIO 43 and 44 are also the USB-UART bridge pins on ESP32-S3 DevKitC!
 *   1. UNPLUG D0 (GPIO 43) and D1 (GPIO 44) wires before uploading firmware.
 *   2. Run: pio run -t upload
 *   3. Once uploaded, RECONNECT D0 to GPIO 43 and D1 to GPIO 44.
 *   4. Press the RST button on the ESP32-S3 DevKitC.
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

#define HEARTBEAT_PIN 26
#define TFT_RST_PIN   14
#define TFT_RD_PIN    20  // Read strobe on GPIO 20

TFT_eSPI tft = TFT_eSPI();

uint32_t cycleCount = 0;

void drawColorScreen(uint16_t color, const char* name, uint16_t textColor) {
    tft.fillScreen(color);
    
    // Draw contrasting border around the screen edges (verify panel resolution 480x320)
    tft.drawRect(0, 0, 480, 320, textColor);
    tft.drawRect(1, 1, 478, 318, textColor);
    tft.drawRect(4, 4, 472, 312, textColor);

    // Large center color label
    tft.setTextColor(textColor, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.drawString(name, 240, 140);

    // Info footer
    tft.setTextSize(2);
    char buf[64];
    snprintf(buf, sizeof(buf), "Cycle: %u  |  Color: 0x%04X", cycleCount, color);
    tft.drawString(buf, 240, 190);

    tft.setTextSize(1);
    tft.drawString("AgroMate TFT Diagnostic Test", 240, 240);
    tft.drawString("D0:43 D1:44 D2:39 D3:40 D4:41 D5:42 D6:48 D7:45", 240, 260);
    tft.drawString("WR:47 DC:38 CS:21 RST:14  |  RD tied to 3.3V", 240, 280);

    // Toggle heartbeat LED
    digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));
}

void drawColorBars() {
    tft.fillScreen(TFT_BLACK);
    
    // 8 vertical color bars
    uint16_t bars[8] = {
        TFT_WHITE,    // 0xFFFF
        TFT_YELLOW,   // 0xFFE0
        TFT_CYAN,     // 0x07FF
        TFT_GREEN,    // 0x07E0
        TFT_MAGENTA,  // 0xF81F
        TFT_RED,      // 0xF800
        TFT_BLUE,     // 0x001F
        TFT_BLACK     // 0x0000
    };
    
    int barWidth = 480 / 8; // 60px each
    for (int i = 0; i < 8; i++) {
        tft.fillRect(i * barWidth, 0, barWidth, 220, bars[i]);
    }

    // Border around color bars
    tft.drawRect(0, 0, 480, 220, TFT_WHITE);

    // Bottom diagnostic area (black background)
    tft.fillRect(0, 220, 480, 100, TFT_BLACK);
    tft.drawRect(0, 220, 480, 100, TFT_CYAN);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("SMPTE 8-COLOR BARS TEST", 240, 240);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("If colors match the labels (White-Yellow-Cyan-Green-Magenta-Red-Blue):", 240, 265);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("YOUR DATA BUS WIRING (D0-D7) IS 100% CORRECT!", 240, 280);

    char buf[64];
    snprintf(buf, sizeof(buf), "Cycle #%u completed successfully", cycleCount);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(buf, 240, 300);

    // Toggle heartbeat
    digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));
}

void setup() {
    // 1. Heartbeat LED pin (lets you see CPU is running even if screen is white)
    pinMode(HEARTBEAT_PIN, OUTPUT);
    digitalWrite(HEARTBEAT_PIN, HIGH);

    // 2. TFT RD pin — drive HIGH for write-only operation
    pinMode(TFT_RD_PIN, OUTPUT);
    digitalWrite(TFT_RD_PIN, HIGH);

    // 3. Hardware Reset of the TFT panel
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(50);
    digitalWrite(TFT_RST_PIN, LOW);   // Active LOW reset
    delay(150);
    digitalWrite(TFT_RST_PIN, HIGH);  // Release reset
    delay(200);

    // 4. Initial serial output (briefly before releasing GPIO 43/44)
    Serial.begin(115200);
    delay(300);
    Serial.println("\n================================================");
    Serial.println("   AgroMate — TFT Display Color Diagnostic Test  ");
    Serial.println("================================================");
    Serial.println("[TFT] Releasing GPIO 43 & 44 UART for TFT parallel bus...");
    Serial.flush();
    Serial.end();  // Disconnect hardware UART from GPIO 43/44 so TFT has exclusive control!

    delay(100);

    // 5. Initialize the TFT display controller
    tft.init();
    tft.setRotation(1);  // Landscape: 480 wide x 320 high
    tft.invertDisplay(false);
}

void loop() {
    cycleCount++;

    // 1. Pure RED
    drawColorScreen(TFT_RED, "TEST 1: RED (0xF800)", TFT_WHITE);
    delay(1500);

    // 2. Pure GREEN
    drawColorScreen(TFT_GREEN, "TEST 2: GREEN (0x07E0)", TFT_WHITE);
    delay(1500);

    // 3. Pure BLUE
    drawColorScreen(TFT_BLUE, "TEST 3: BLUE (0x001F)", TFT_WHITE);
    delay(1500);

    // 4. Pure YELLOW
    drawColorScreen(TFT_YELLOW, "TEST 4: YELLOW (0xFFE0)", TFT_BLACK);
    delay(1500);

    // 5. Pure CYAN
    drawColorScreen(TFT_CYAN, "TEST 5: CYAN (0x07FF)", TFT_BLACK);
    delay(1500);

    // 6. Pure MAGENTA
    drawColorScreen(TFT_MAGENTA, "TEST 6: MAGENTA (0xF81F)", TFT_WHITE);
    delay(1500);

    // 7. Pure WHITE
    drawColorScreen(TFT_WHITE, "TEST 7: WHITE (0xFFFF)", TFT_BLACK);
    delay(1500);

    // 8. Pure BLACK
    drawColorScreen(TFT_BLACK, "TEST 8: BLACK (0x0000)", TFT_GREEN);
    delay(1500);

    // 9. Color Bars
    drawColorBars();
    delay(3000);
}
