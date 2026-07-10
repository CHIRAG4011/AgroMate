/**
 * AgroMate ESP32-S3 Firmware — Production Build
 * 
 * Features:
 *   - Animated boot screen with progress bar
 *   - Expressive AI face with smooth eye animations at 30 FPS
 *   - Dashboard screen with real-time system stats (flicker-free)
 *   - Status bar overlay showing time, FPS, expression name
 *   - BOOT button (interrupt-driven) cycles: Face ↔ Dashboard
 *   - NimBLE Bluetooth peripheral connectivity
 *   - Parallel OV7670 camera capture with live LCD preview
 *   - Time-Multiplexed I2S Audio: recording via INMP441 & playback via MAX98357A
 *   - Audio playback moves the robotic mouth dynamically (lip sync)
 * 
 * Hardware: RM68140 3.5" 320x480 8-bit Parallel TFT on ESP32-S3 DevKitC-1
 * 
 * GPIO Pin Assignment (NO SHARED PINS):
 *   TFT D0-D7: 43,44,39,40,41,42,48,45  |  TFT Ctrl: WR=47,DC=38,CS=21,RST=14
 *   Camera D0-D7: 4,5,6,7,15,16,17,18   |  Cam Ctrl: XCLK=10,VSYNC=12,HREF=11,PCLK=13,SDA=8,SCL=9
 *   Audio: BCLK=2, WS=1, SD_IN=46, SD_OUT=3
 *   LEDs: GPIO 26  |  BOOT Button: GPIO 0
 */

#include <Arduino.h>
#include <esp_pm.h>
#include "esp_log.h"
#include "driver/gpio.h"

// A simple vprintf redirect that bypasses the heavy diagnostics hooks to prevent stack overflows
int simple_vprintf(const char* fmt, va_list args) {
    return 0; // Silence all framework logs to prevent task stack overflows
}
#include "config.h"
#include "display/DisplayManager.h"
#include "display/FaceRenderer.h"
#include "ble/BLEClient.h"
#include "camera/CameraManager.h"
#include "audio/AudioManager.h"
#include "leds/LEDManager.h"

// ── Global instances ──
DisplayManager display;
FaceRenderer face;
AgroMateBLE ble;
CameraManager camera;
AudioManager audio;
LEDManager leds;

// ── Screen modes ──
enum class ScreenMode : uint8_t {
    BOOT = 0,
    FACE = 1,
    DASHBOARD = 2,
    PREVIEW = 3
};

ScreenMode currentScreen = ScreenMode::BOOT;

// ── Timing ──
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
float currentFPS = 0;
unsigned long fpsTimer = 0;
const unsigned long FRAME_INTERVAL = 1000 / DISPLAY_FPS;

// ── Demo mode (active only when BLE is disconnected) ──
bool demoMode = true;
unsigned long lastExprChange = 0;
const unsigned long EXPR_CHANGE_INTERVAL = 8000;  // Change expression every 8s
uint8_t demoExprIndex = 0;

// Expression names for display
const char* expressionNames[] = {
    "Idle", "Happy", "Thinking", "Listening",
    "Talking", "Error", "Warning", "Sleeping",
    "Surprised", "Sad", "Angry", "Curious",
    "Scanning", "Success"
};
const uint8_t NUM_EXPRESSIONS = 14;

// Demo expression sequence
const Expression demoSequence[] = {
    Expression::IDLE,
    Expression::HAPPY,
    Expression::CURIOUS,
    Expression::THINKING,
    Expression::LISTENING,
    Expression::SURPRISED,
    Expression::SUCCESS,
    Expression::SCANNING,
    Expression::SLEEPING,
};
const uint8_t DEMO_SEQ_LEN = sizeof(demoSequence) / sizeof(demoSequence[0]);

// ── Interrupt-driven BOOT button ──
#define BTN_PIN 0
volatile bool buttonPressed = false;
unsigned long lastBtnDebounce = 0;
const unsigned long BTN_DEBOUNCE_MS = 300;
uint8_t btnPressCount = 0;

void IRAM_ATTR buttonISR() {
    buttonPressed = true;
}

// ── Dashboard data ──
unsigned long bootTime = 0;
bool dashboardDrawn = false; // Tracks whether static dashboard elements have been drawn

// ── Color palette (matching FaceRenderer) ──
#define UI_BG        0x0000  // Black
#define UI_DARK_GRAY 0x0841
#define UI_MID_GRAY  0x4208
#define UI_CYAN      0x07FF
#define UI_GREEN     0x07E0
#define UI_RED       0xF800
#define UI_YELLOW    0xFFE0
#define UI_ORANGE    0xFDA0
#define UI_DARK_CYAN 0x0410
#define UI_WHITE     0xFFFF
#define UI_DARK_BLUE 0x000F

// ── Forward declarations ──
void drawHomeScreen();
void drawBootScreen(float progress, const char* status);
void drawDashboardStatic();
void drawDashboardDynamic();
void drawStatusBar();
void handleButton();
void updateDemo();
void handleBLECommand(uint8_t* data, size_t length);
void yuv422_to_rgb565_fast(const uint8_t* yuv, uint16_t* rgb, int width, int height);
void runScanPreview();

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  YUV422 to RGB565 conversion (Fast Integer implementation)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void yuv422_to_rgb565_fast(const uint8_t* yuv, uint16_t* rgb, int width, int height) {
    int size = width * height;
    for (int i = 0; i < size; i += 2) {
        int y0 = yuv[0];
        int u  = yuv[1] - 128;
        int y1 = yuv[2];
        int v  = yuv[3] - 128;
        yuv += 4;

        // Fast integer approximations for RGB offsets
        int r_offset = (91881 * v) >> 16;
        int g_offset = (-22554 * u - 46802 * v) >> 16;
        int b_offset = (116130 * u) >> 16;

        int r0 = constrain(y0 + r_offset, 0, 255);
        int g0 = constrain(y0 + g_offset, 0, 255);
        int b0 = constrain(y0 + b_offset, 0, 255);

        int r1 = constrain(y1 + r_offset, 0, 255);
        int g1 = constrain(y1 + g_offset, 0, 255);
        int b1 = constrain(y1 + b_offset, 0, 255);

        // Pack into 16-bit RGB565 values
        rgb[i]     = ((r0 & 0xF8) << 8) | ((g0 & 0xFC) << 3) | (b0 >> 3);
        rgb[i + 1] = ((r1 & 0xF8) << 8) | ((g1 & 0xFC) << 3) | (b1 >> 3);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Live Camera Scanning Preview & Capture
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void runScanPreview() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);
    
    // Draw scanning border box (centered around 426x320 scaled preview)
    int borderX = 23;
    int borderY = 0;
    tft->drawRect(borderX, borderY, 434, 320, UI_CYAN);
    tft->drawRect(borderX + 1, borderY + 1, 432, 318, UI_CYAN);
    
    // Stack-allocated line buffer for one row of scaled RGB565 pixels (426 pixels = 852 bytes)
    uint16_t rgbLine[426];
    
    unsigned long startTime = millis();
    int scanLineY = 0;
    bool scanLineDown = true;
    
    face.setExpression(Expression::SCANNING);
    leds.setPattern(LEDPattern::PULSE, CRGB::Green, 80);
    
    // Run live preview for 10 seconds (10000 ms) as requested by user
    while (millis() - startTime < 10000) {
        camera_fb_t* fb = camera.captureRaw();
        
        if (fb) {
            leds.setPattern(LEDPattern::SOLID, CRGB::Green);
            
            int w = fb->width;
            
            // Perform integer-based 1.33x scale conversion line-by-line:
            //   - Width: 320px -> 426px (srcX = x * 3 / 4)
            //   - Height: 240px -> 320px (srcY = y * 3 / 4)
            for (int y = 0; y < 320; y++) {
                int srcY = (y * 3) / 4;
                
                if (fb->format == PIXFORMAT_GRAYSCALE) {
                    const uint8_t* grayPtr = fb->buf + (srcY * w);
                    for (int x = 0; x < 426; x++) {
                        int srcX = (x * 3) / 4;
                        uint8_t g = grayPtr[srcX];
                        rgbLine[x] = ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
                    }
                    tft->pushImage(27, y, 426, 1, rgbLine);
                } else if (fb->format == PIXFORMAT_YUV422) {
                    const uint8_t* yuvPtr = fb->buf + (srcY * w * 2);
                    uint16_t tempLine[320];
                    yuv422_to_rgb565_fast(yuvPtr, tempLine, w, 1);
                    for (int x = 0; x < 426; x++) {
                        int srcX = (x * 3) / 4;
                        rgbLine[x] = tempLine[srcX];
                    }
                    tft->pushImage(27, y, 426, 1, rgbLine);
                }
            }
            
            // Draw a green laser line scanning overlay spanning the scaled preview
            tft->drawFastHLine(27, scanLineY, 426, UI_GREEN);
            if (scanLineY > 0) {
                tft->drawFastHLine(27, scanLineY - 1, 426, UI_GREEN);
            }
            
            // Update scan line movement
            if (scanLineDown) {
                scanLineY += 4;
                if (scanLineY >= 318) scanLineDown = false;
            } else {
                scanLineY -= 4;
                if (scanLineY <= 2) scanLineDown = true;
            }
        } else {
            leds.setPattern(LEDPattern::SOLID, CRGB::Red);
        }
        
        camera.releaseFrame();
        delay(15); // Frame limiting for responsive loop
        ble.update();
        leds.update();

        // Send heartbeat in background to prevent laptop BLE timeout
        static unsigned long lastHeartbeatInPreview = 0;
        unsigned long now = millis();
        if (now - lastHeartbeatInPreview >= HEARTBEAT_INTERVAL_MS) {
            lastHeartbeatInPreview = now;
            if (ble.isConnected()) {
                ble.sendHeartbeat();
                ble.sendSensorData(85, 4100, false);
            }
        }
    }
    
    // Capture final high-quality photo to send
    size_t finalJpgSize = 0;
    uint8_t* finalJpg = camera.capture(&finalJpgSize);
    if (finalJpg && finalJpgSize > 0) {
        Serial.printf("[Preview] Capture successful (%u bytes). Sending over BLE...\n", finalJpgSize);
        ble.sendDebugMessage("Capture successful. Sending image chunks...");
        
        // Show analyzer indicator on screen inside the preview box
        tft->fillRect(27, 0, 426, 320, UI_BG);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(2);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("ANALYZING PLANT...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        
        ble.sendImage(finalJpg, finalJpgSize);
        face.setExpression(Expression::THINKING);
        leds.setPattern(LEDPattern::BLINK, CRGB::Cyan, 120);
    } else {
        Serial.println("[Preview] Camera capture failed!");
        String errMsg = "Camera capture failed! Free heap: " + String(ESP.getFreeHeap()) + " init=" + String(camera.isAvailable());
        ble.sendDebugMessage(errMsg.c_str());
        face.setExpression(Expression::ERROR);
        leds.setPattern(LEDPattern::SOLID, CRGB::Red);
    }
    
    camera.releaseFrame();
    delay(1000);
    
    // Restore Face Screen mode
    currentScreen = ScreenMode::FACE;
    drawHomeScreen();
    leds.setPattern(LEDPattern::SOLID, CRGB::Blue);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  BLE Command Callback Processor
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void handleBLECommand(uint8_t* data, size_t length) {
    if (length == 0) return;
    uint8_t cmdType = data[0];
    uint8_t* payload = data + 1;
    size_t payloadLen = length - 1;

    Serial.printf("[BLE] Command Type: 0x%02X, Payload size: %u bytes\n", cmdType, payloadLen);

    switch (static_cast<MessageType>(cmdType)) {
        case MessageType::FACE_COMMAND: {
            if (payloadLen >= 1) {
                Expression expr = static_cast<Expression>(payload[0]);
                face.setExpression(expr);
                Serial.printf("[BLE] Expression changed: %d\n", static_cast<int>(expr));
            }
            break;
        }
        case MessageType::AI_RESPONSE: {
            if (payloadLen > 0) {
                String responseText = "";
                for (size_t i = 0; i < payloadLen; i++) {
                    responseText += (char)payload[i];
                }
                display.showAIResponse(responseText);
                Serial.printf("[BLE] Displaying response: %s\n", responseText.c_str());
            }
            break;
        }
        case MessageType::SPEECH_AUDIO: {
            if (payloadLen > 0) {
                Serial.printf("[BLE] Playing speech audio (%u bytes)...\n", payloadLen);
                face.setExpression(Expression::TALKING);
                audio.playBuffer(payload, payloadLen);
            }
            break;
        }
        case MessageType::CAPTURE_REQUEST: {
            Serial.println("[BLE] Camera Capture requested.");
            currentScreen = ScreenMode::PREVIEW;
            break;
        }
        case MessageType::RECORD_REQUEST: {
            uint32_t recordDurationMs = 5000;
            if (payloadLen >= 4) {
                memcpy(&recordDurationMs, payload, 4);
            }
            Serial.printf("[BLE] Starting recording for %u ms...\n", recordDurationMs);
            face.setExpression(Expression::LISTENING);
            leds.setPattern(LEDPattern::PULSE, CRGB::Orange, 100);
            audio.startRecording(recordDurationMs);
            break;
        }
        case MessageType::STATUS_UPDATE: {
            if (payloadLen >= 1) {
                uint8_t mode = payload[0];
                if (mode == 0) {
                    currentScreen = ScreenMode::FACE;
                    display.getTFT()->fillScreen(UI_BG);
                    Serial.println("[BLE] Switched to Face Screen via voice/BLE");
                } else if (mode == 1) {
                    currentScreen = ScreenMode::DASHBOARD;
                    dashboardDrawn = false;
                    Serial.println("[BLE] Switched to Dashboard Screen via voice/BLE");
                }
            }
            break;
        }
        default:
            break;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Boot Screen — Animated startup with progress bar
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawBootScreen(float progress, const char* status) {
    TFT_eSPI* tft = display.getTFT();
    
    // Only clear screen on the first frame
    static bool bootScreenInit = false;
    if (!bootScreenInit) {
        tft->fillScreen(UI_BG);
        
        // Decorative outer frame
        tft->drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, UI_CYAN);
        
        int cx = SCREEN_WIDTH / 2;
        int cy = 100;
        
        // Draw leaf icon
        tft->fillCircle(cx - 12, cy, 20, UI_GREEN);
        tft->fillCircle(cx + 12, cy, 20, UI_GREEN);
        tft->fillCircle(cx, cy - 8, 15, UI_GREEN);
        tft->fillRect(cx - 2, cy + 10, 4, 25, UI_GREEN);
        
        // Title
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(3);
        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("AGRO", cx - 55, 165);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("MATE", cx + 55, 165);
        
        // Subtitle
        tft->setTextSize(1);
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("Agricultural AI Companion", cx, 195);
        
        // Version at bottom
        tft->setTextSize(1);
        tft->setTextColor(UI_DARK_CYAN, UI_BG);
        tft->drawString("v2.0.0 | ESP32-S3 | Production Build", cx, SCREEN_HEIGHT - 20);
        
        // Progress bar outline (drawn once)
        int barX = 60;
        int barY = 220;
        int barW = SCREEN_WIDTH - 120;
        int barH = 12;
        tft->drawRect(barX, barY, barW, barH, UI_CYAN);
        
        bootScreenInit = true;
    }
    
    int cx = SCREEN_WIDTH / 2;
    int barX = 60;
    int barY = 220;
    int barW = SCREEN_WIDTH - 120;
    int barH = 12;
    
    // Update progress bar fill
    int fillW = (int)((barW - 4) * progress);
    if (fillW > 0) {
        tft->fillRect(barX + 2, barY + 2, fillW, barH - 4, UI_GREEN);
    }
    
    // Update progress percentage
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100));
    // Clear old percentage text area
    tft->fillRect(cx - 30, barY + barH + 8, 60, 12, UI_BG);
    tft->drawString(pctBuf, cx, barY + barH + 15);
    
    // Update status text
    tft->setTextColor(UI_CYAN, UI_BG);
    // Clear old status text area
    tft->fillRect(60, barY + barH + 23, SCREEN_WIDTH - 120, 12, UI_BG);
    tft->drawString(status, cx, barY + barH + 30);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Dashboard Screen — Static elements (drawn once)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawDashboardStatic() {
    TFT_eSPI* tft = display.getTFT();
    
    tft->fillScreen(UI_BG);
    
    // Header Bar
    tft->fillRect(0, 0, SCREEN_WIDTH, 36, UI_DARK_GRAY);
    tft->drawFastHLine(0, 36, SCREEN_WIDTH, UI_CYAN);
    tft->drawFastHLine(0, 37, SCREEN_WIDTH, UI_DARK_CYAN);
    
    tft->setTextDatum(ML_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" AGROMATE SMART FARM ENGINE", 12, 18);
    
    int cardX1 = 10;
    int cardX2 = 246;
    int cardW = 224;
    int cardH = 104;
    int row1Y = 48;
    int row2Y = 160;
    
    // Soil Conditions Card
    tft->drawRoundRect(cardX1, row1Y, cardW, cardH, 6, UI_CYAN);
    tft->fillRect(cardX1 + 1, row1Y + 1, cardW - 2, 22, UI_DARK_GRAY);
    tft->drawFastHLine(cardX1 + 1, row1Y + 23, cardW - 2, UI_CYAN);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" SOIL CONDITIONS", cardX1 + 8, row1Y + 6);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Moisture:  45% (Optimal)", cardX1 + 8, row1Y + 32);
    
    int barX = cardX1 + 8;
    int barY = row1Y + 48;
    int barW = cardW - 16;
    int barH = 8;
    tft->drawRect(barX, barY, barW, barH, UI_MID_GRAY);
    tft->fillRect(barX + 1, barY + 1, (int)((barW - 2) * 0.45f), barH - 2, UI_GREEN);
    
    tft->drawString("Soil Temp: 21.8 C", cardX1 + 8, row1Y + 62);
    tft->drawString("Soil pH:   6.5 (Neutral)", cardX1 + 8, row1Y + 78);
    
    // Environment & Nutrients Card
    tft->drawRoundRect(cardX1, row2Y, cardW, cardH, 6, UI_GREEN);
    tft->fillRect(cardX1 + 1, row2Y + 1, cardW - 2, 22, UI_DARK_GRAY);
    tft->drawFastHLine(cardX1 + 1, row2Y + 23, cardW - 2, UI_GREEN);
    tft->setTextColor(UI_GREEN, UI_DARK_GRAY);
    tft->drawString(" NUTRIENTS & NPK", cardX1 + 8, row2Y + 6);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("N: 42ppm | P: 18ppm | K: 35ppm", cardX1 + 8, row2Y + 32);
    tft->drawString("Light:   450 Lux (Optimal)", cardX1 + 8, row2Y + 52);
    tft->drawString("Conduct: 1.2 dS/m (Good)", cardX1 + 8, row2Y + 72);
    
    // Farm Weather Card
    tft->drawRoundRect(cardX2, row1Y, cardW, cardH, 6, UI_YELLOW);
    tft->fillRect(cardX2 + 1, row1Y + 1, cardW - 2, 22, UI_DARK_GRAY);
    tft->drawFastHLine(cardX2 + 1, row1Y + 23, cardW - 2, UI_YELLOW);
    tft->setTextColor(UI_YELLOW, UI_DARK_GRAY);
    tft->drawString(" FARM WEATHER", cardX2 + 8, row1Y + 6);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Air Temp:  28.5 C", cardX2 + 8, row1Y + 32);
    tft->drawString("Humidity:  62% (Optimal)", cardX2 + 8, row1Y + 52);
    tft->drawString("Forecast:  Sunny / Clear", cardX2 + 8, row1Y + 72);
    
    // Robot System Status Card (static frame only)
    tft->drawRoundRect(cardX2, row2Y, cardW, cardH, 6, UI_ORANGE);
    tft->fillRect(cardX2 + 1, row2Y + 1, cardW - 2, 22, UI_DARK_GRAY);
    tft->drawFastHLine(cardX2 + 1, row2Y + 23, cardW - 2, UI_ORANGE);
    tft->setTextColor(UI_ORANGE, UI_DARK_GRAY);
    tft->drawString(" ROBOT SYSTEM STATUS", cardX2 + 8, row2Y + 6);
    
    // Footer button hint
    int footerY = 276;
    tft->drawRoundRect(10, footerY, SCREEN_WIDTH - 20, 34, 4, UI_CYAN);
    tft->fillRect(11, footerY + 1, SCREEN_WIDTH - 22, 32, UI_DARK_BLUE);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("[ PRESS PHYSICAL BOOT BUTTON TO SHOW EYE ANIMATION ]", SCREEN_WIDTH / 2, footerY + 17);
    
    dashboardDrawn = true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Dashboard Screen — Dynamic elements (updated each tick)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawDashboardDynamic() {
    TFT_eSPI* tft = display.getTFT();
    
    int cardX2 = 246;
    int row2Y = 160;
    
    // BLE status in header
    tft->setTextDatum(MR_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(ble.isConnected() ? UI_GREEN : UI_RED, UI_DARK_GRAY);
    tft->drawString(ble.isConnected() ? "BLE: SYNCED | SYS: OK " : "BLE: WAITING | SYS: OK ", SCREEN_WIDTH - 12, 18);
    
    // Robot System Status — dynamic values
    unsigned long upSec = (millis() - bootTime) / 1000;
    int hours = upSec / 3600;
    int mins = (upSec % 3600) / 60;
    int secs = upSec % 60;
    
    // Clear dynamic text area in System Status card
    tft->fillRect(cardX2 + 7, row2Y + 28, 210, 56, UI_BG);
    
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Battery:   USB Powered", cardX2 + 8, row2Y + 32);
    
    char uptimeBuf[32];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "Uptime:    %02d:%02d:%02d", hours, mins, secs);
    tft->drawString(uptimeBuf, cardX2 + 8, row2Y + 52);
    
    char memoryBuf[32];
    snprintf(memoryBuf, sizeof(memoryBuf), "Free Heap: %dKB", ESP.getFreeHeap() / 1024);
    tft->drawString(memoryBuf, cardX2 + 8, row2Y + 72);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Home Screen (AGRO MATE AI BY CHIRAG ST MARY'S SCHOOL)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawHomeScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);
    
    // Draw modern high-tech border
    tft->drawRect(6, 6, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 12, UI_CYAN);
    tft->drawRect(8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, UI_DARK_CYAN); // subtle dark outline
    
    // Draw decorative brackets/corners
    tft->fillRect(4, 4, 30, 6, UI_CYAN);
    tft->fillRect(4, 4, 6, 30, UI_CYAN);
    
    tft->fillRect(SCREEN_WIDTH - 34, 4, 30, 6, UI_CYAN);
    tft->fillRect(SCREEN_WIDTH - 10, 4, 6, 30, UI_CYAN);
    
    tft->fillRect(4, SCREEN_HEIGHT - 10, 30, 6, UI_CYAN);
    tft->fillRect(4, SCREEN_HEIGHT - 34, 6, 30, UI_CYAN);
    
    tft->fillRect(SCREEN_WIDTH - 34, SCREEN_HEIGHT - 10, 30, 6, UI_CYAN);
    tft->fillRect(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 34, 6, 30, UI_CYAN);

    // Title: AGRO MATE AI
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN, UI_BG);
    tft->setTextSize(4);
    tft->drawString("AGRO MATE AI", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 40);

    // Subtitle: BY CHIRAG
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->setTextSize(2);
    tft->drawString("BY CHIRAG", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15);

    // Subtitle 2: ST MARY'S SCHOOL
    tft->setTextColor(UI_YELLOW, UI_BG);
    tft->setTextSize(2);
    tft->drawString("ST MARY'S SCHOOL", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 45);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Status Bar
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawStatusBar() {
    TFT_eSPI* tft = display.getTFT();
    
    tft->fillRect(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, 26, UI_DARK_GRAY);
    tft->drawFastHLine(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, UI_DARK_CYAN);
    tft->setTextDatum(ML_DATUM);
    tft->setTextSize(1);
    
    uint8_t exprIdx = static_cast<uint8_t>(face.getExpression());
    const char* exprName = (exprIdx < NUM_EXPRESSIONS) ? expressionNames[exprIdx] : "?";
    char modeBuf[32];
    snprintf(modeBuf, sizeof(modeBuf), "  AI MODE: %s", exprName);
    tft->setTextColor(UI_GREEN, UI_DARK_GRAY);
    tft->drawString(modeBuf, 4, SCREEN_HEIGHT - 13);
    
    tft->setTextDatum(MC_DATUM);
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", currentFPS);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(fpsBuf, SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT - 13);
    
    int btnW = 110;
    int btnH = 18;
    int btnX = SCREEN_WIDTH - btnW - 6;
    int btnY = SCREEN_HEIGHT - btnH - 4;
    tft->drawRoundRect(btnX, btnY, btnW, btnH, 3, UI_CYAN);
    tft->fillRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, UI_DARK_BLUE);
    
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("DASHBOARD [BOOT]", btnX + (btnW / 2), btnY + (btnH / 2) + 1);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Button Handler (consumes interrupt flag)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void handleButton() {
    if (!buttonPressed) return;
    
    unsigned long now = millis();
    if ((now - lastBtnDebounce) < BTN_DEBOUNCE_MS) {
        buttonPressed = false;
        return;
    }
    
    buttonPressed = false;
    lastBtnDebounce = now;
    btnPressCount++;
    
    Serial.printf("[BTN] Press #%d (ISR), screen=%d\n", btnPressCount, (int)currentScreen);
    
    if (currentScreen == ScreenMode::FACE) {
        currentScreen = ScreenMode::DASHBOARD;
        dashboardDrawn = false;
    } else if (currentScreen == ScreenMode::DASHBOARD) {
        currentScreen = ScreenMode::FACE;
        display.getTFT()->fillScreen(UI_BG);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Demo Mode updates (active only when BLE is not synced)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void updateDemo() {
    if (!demoMode) return;
    
    unsigned long now = millis();
    if (now - lastExprChange >= EXPR_CHANGE_INTERVAL) {
        lastExprChange = now;
        
        demoExprIndex = (demoExprIndex + 1) % DEMO_SEQ_LEN;
        Expression newExpr = demoSequence[demoExprIndex];
        face.setExpression(newExpr);
        
        Serial.printf("[DEMO] Expression -> %s\n", expressionNames[static_cast<uint8_t>(newExpr)]);
        
        if (newExpr == Expression::LISTENING) {
            face.setMouthOpenness(0.0f);
        }
    }
    
    Expression currentExpr = face.getExpression();
    if (currentExpr == Expression::TALKING || currentExpr == Expression::HAPPY) {
        float talkAnim = sin(millis() / 150.0f) * 0.5f + 0.5f;
        if (currentExpr == Expression::TALKING) {
            face.setMouthOpenness(talkAnim * 0.7f);
        }
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Setup
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void setup() {
    esp_log_set_vprintf(simple_vprintf);
    setCpuFrequencyMhz(240); // Run at 240MHz for stable clock configuration of LCD_CAM and LEDC
    
    // Immediately detach the hardware JTAG controller from GPIO 39, 40, 41, 42
    // to prevent JTAG signals from interfering with display data and triggering CPU panics.
    gpio_reset_pin((gpio_num_t)39);
    gpio_reset_pin((gpio_num_t)40);
    gpio_reset_pin((gpio_num_t)41);
    gpio_reset_pin((gpio_num_t)42);
    
    pinMode(39, OUTPUT);
    pinMode(40, OUTPUT);
    pinMode(41, OUTPUT);
    pinMode(42, OUTPUT);
    
    // Drive TFT_RST (GPIO 14) HIGH to keep the screen active without triggering resets
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
 
    // Lock CPU frequency to maximum (240MHz) to prevent clock switching crashes
    // #if CONFIG_PM_ENABLE
    // esp_pm_lock_handle_t cpu_lock;
    // if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_lock", &cpu_lock) == ESP_OK) {
    //     esp_pm_lock_acquire(cpu_lock);
    // }
    // #endif
 
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n=============================================");
    Serial.println("   AgroMate Robot — Production Build v2.0");
    Serial.println("=============================================");
    
    // TFT RD line is physically not connected, no need to drive GPIO 20.
 
    // Configure BOOT button (disabled - voice controlled screen transitions)
    // pinMode(BTN_PIN, INPUT_PULLUP);
    // attachInterrupt(digitalPinToInterrupt(BTN_PIN), buttonISR, FALLING);
    
    // ── Actual Sequential System Initialization ──
    // 1. Probe and initialize camera first to avoid LCD_CAM conflict with parallel TFT
    int cameraErr = -1;
    for (int retry = 0; retry < 3; retry++) {
        cameraErr = camera.begin();
        if (cameraErr == 0) {
            break;
        }
        Serial.printf("[BOOT] Camera init failed (try %d/3). Retrying...\n", retry + 1);
        delay(200); // Wait 200ms before retrying
    }
    
    // 2. Initialize display screen next
    display.begin();
    
    drawBootScreen(0.40f, "Loading display settings...");
    delay(150);
    
    // 3. Now that the display is active, show the boot screen results
    if (cameraErr != 0) {
        char errBuf[64];
        if (cameraErr == 1) {
            snprintf(errBuf, sizeof(errBuf), "Error: I2C Pins Stuck LOW (Code %d)", cameraErr);
        } else {
            snprintf(errBuf, sizeof(errBuf), "Error: Cam Init Failed 0x%X (Code %d)", cameraErr, cameraErr);
        }
        drawBootScreen(0.10f, errBuf);
        delay(5000); // Wait 5s so user can read the specific error code
    } else {
        drawBootScreen(0.10f, "OV7670 Camera Initialized");
        delay(150);
    }

    drawBootScreen(0.45f, "Starting BLE Client...");
    ble.begin(BLE_DEVICE_NAME);
    ble.onCommand(handleBLECommand);
    delay(150);
    
    drawBootScreen(0.70f, "Hooking I2S Mic & Speaker...");
    audio.begin();
    delay(150);
    
    drawBootScreen(0.85f, "Syncing WS2812B Status LEDs...");
    leds.begin();
    delay(150);
    
    drawBootScreen(1.0f, "AgroMate system ready!");
    delay(400);
    
    leds.setPattern(LEDPattern::SOLID, CRGB::Blue);
    
    // Draw the static home text screen as a splash screen
    drawHomeScreen();
    delay(3000); // Show splash screen for 3 seconds
    display.getTFT()->fillScreen(UI_BG);
    currentScreen = ScreenMode::FACE;
    
    bootTime = millis();
    lastFrameTime = millis();
    fpsTimer = millis();
    lastExprChange = millis();
    
    setCpuFrequencyMhz(240); // Restore full 240MHz CPU clock for main loop operations
    Serial.println("[BOOT] Complete! Entering main loop.");
    Serial.printf("[BOOT] Free heap: %d bytes\n", ESP.getFreeHeap());
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Main Loop
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void loop() {
    unsigned long now = millis();
    
    // Consume interrupt-driven button press (disabled - voice controlled screen transitions)
    // handleButton();
    
    // System core updates
    ble.update();
    audio.update();
    leds.update();
    
    // Heartbeats and battery updates to BLE center
    static unsigned long lastHeartbeat = 0;
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        if (ble.isConnected()) {
            ble.sendHeartbeat();
            ble.sendSensorData(85, 4100, false);
            demoMode = false; // Disable demo cycling when connected
        } else {
            demoMode = true;
        }
    }
    
    // Audio capture completion monitor
    static bool lastAudioRecording = false;
    bool currentAudioRecording = audio.isRecording();
    if (lastAudioRecording && !currentAudioRecording) {
        // Recording just completed, send raw audio bytes
        size_t audioSize = 0;
        uint8_t* audioData = audio.getRecordedData(&audioSize);
        if (audioData && audioSize > 0) {
            Serial.printf("[AUDIO] Transmitting recorded audio (%u bytes) over BLE...\n", audioSize);
            ble.sendAudio(audioData, audioSize);
            face.setExpression(Expression::THINKING);
            leds.setPattern(LEDPattern::BLINK, CRGB::Cyan, 150);
        } else {
            face.setExpression(Expression::ERROR);
            leds.setPattern(LEDPattern::SOLID, CRGB::Red);
        }
    }
    lastAudioRecording = currentAudioRecording;
    
    // Audio lipsync update
    if (audio.isPlaying()) {
        face.setExpression(Expression::TALKING);
        face.setMouthOpenness(audio.getPlaybackLevel() * 1.5f);
        leds.setPattern(LEDPattern::PULSE, CRGB::Green, 120);
    } else if (face.getExpression() == Expression::TALKING) {
        face.setExpression(Expression::IDLE);
        face.setMouthOpenness(0.0f);
        leds.setPattern(LEDPattern::SOLID, CRGB::Blue);
    }
    
    // FPS computation
    frameCount++;
    if (now - fpsTimer >= 1000) {
        currentFPS = frameCount * 1000.0f / (now - fpsTimer);
        frameCount = 0;
        fpsTimer = now;
    }
    
    // ── Screen rendering engine ──
    if (currentScreen == ScreenMode::FACE) {
        face.update();
        face.render();
    } else if (currentScreen == ScreenMode::DASHBOARD) {
        // Draw static elements once, then only update dynamic values
        if (!dashboardDrawn) {
            drawDashboardStatic();
        }
        static unsigned long lastDash = 0;
        if (now - lastDash >= 1000) {
            lastDash = now;
            drawDashboardDynamic();
        }
    } else if (currentScreen == ScreenMode::PREVIEW) {
        // Run full preview routine and restore screen mode after
        runScanPreview();
    }
    
    yield();
}
