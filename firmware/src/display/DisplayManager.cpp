/**
 * AgroMate Display Manager Implementation
 * 
 * Drives the RM68140 3.5" 320x480 TFT via 8-bit parallel interface.
 * Uses TFT_eSPI with RD on GPIO 20 (driven HIGH for write-only operation).
 * 
 * White Screen Fix Notes:
 *   1. Added 1-second pre-init delay for hardware stabilization
 *   2. Using TFT_DC (not TFT_RS) in build flags
 *   3. Explicit backlight control and color inversion check
 *   4. All pin conflicts with camera/audio resolved in config.h
 */

#include "DisplayManager.h"
#include "../config.h"

void DisplayManager::begin() {
    // ── CRITICAL: Wait for display hardware to stabilize ──
    delay(500);

    // ── Perform Hardware Reset ──
    if (TFT_RST_PIN >= 0) {
        pinMode(TFT_RST_PIN, OUTPUT);
        digitalWrite(TFT_RST_PIN, HIGH);
        delay(50);
        digitalWrite(TFT_RST_PIN, LOW);
        delay(150);
        digitalWrite(TFT_RST_PIN, HIGH);
        delay(150);
    }

    _tft.init();
    _tft.setRotation(SCREEN_ROTATION);  // Portrait mode: 320 wide x 480 tall

    // Some RM68140 panels need inversion ON, others OFF.
    _tft.invertDisplay(false);

    _tft.fillScreen(TFT_BLACK);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Set backlight if controlled by GPIO
    if (TFT_BL_PIN >= 0) {
        pinMode(TFT_BL_PIN, OUTPUT);
        analogWrite(TFT_BL_PIN, 200);
    }
}

void DisplayManager::clear(uint16_t color) {
    _tft.fillScreen(color);
}

void DisplayManager::showBootScreen(const char* title, const char* status) {
    _tft.fillScreen(TFT_BLACK);

    // Draw decorative border
    _tft.drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, TFT_CYAN);
    _tft.drawRect(6, 6, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 12, 0x0410);

    // Title — large centered text
    _tft.setTextSize(3);
    _tft.setTextColor(TFT_GREEN);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(title, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30);

    // Subtitle / status
    _tft.setTextSize(1);
    _tft.setTextColor(TFT_CYAN);
    _tft.drawString(status, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20);

    // Version at bottom
    _tft.setTextColor(TFT_DARKGREY);
    _tft.drawString("v1.0.0 | ESP32-S3", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 24);

    // Small leaf emoji placeholder
    _tft.setTextColor(TFT_GREEN);
    _tft.setTextSize(2);
    _tft.drawString("* AgroMate *", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 60);
}

void DisplayManager::showAIResponse(String text) {
    // Show AI response text in a styled box at the bottom of the screen
    int boxHeight = 70;
    int boxY = SCREEN_HEIGHT - boxHeight;

    // Background with gradient-like effect
    _tft.fillRect(0, boxY, SCREEN_WIDTH, boxHeight, 0x0841);
    _tft.drawRect(0, boxY, SCREEN_WIDTH, boxHeight, TFT_CYAN);
    _tft.drawFastHLine(1, boxY + 1, SCREEN_WIDTH - 2, 0x0410);

    // Label
    _tft.setTextSize(1);
    _tft.setTextColor(TFT_CYAN);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("AI Response:", 8, boxY + 6);

    // Response text
    _tft.setTextColor(TFT_WHITE);
    _tft.setTextWrap(true);

    // Truncate if too long for the box
    if (text.length() > 100) {
        text = text.substring(0, 97) + "...";
    }

    _tft.setCursor(8, boxY + 20);
    _tft.print(text);
}

void DisplayManager::cycleInfoScreen() {
    _infoScreenIndex = (_infoScreenIndex + 1) % 4;
}

void DisplayManager::pushSprite() {
    // Sprite disabled to draw directly to TFT screen
}
