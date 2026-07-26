/**
 * AgroMate ESP32-S3 Firmware Ã¢â‚¬â€ Production Build
 * 
 * Features:
 *   - Animated boot screen with progress bar
 *   - Expressive AI face with smooth eye animations at 30 FPS
 *   - Dashboard screen with real-time system stats (flicker-free)
 *   - Status bar overlay showing time, FPS, expression name
 *   - BOOT button (interrupt-driven) cycles: Face Ã¢â€ â€ Dashboard
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
#include "esp_mac.h"
#include "driver/gpio.h"
#include <Preferences.h>

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

// Ã¢â€â‚¬Ã¢â€â‚¬ Global instances Ã¢â€â‚¬Ã¢â€â‚¬
DisplayManager display;
FaceRenderer face;
AgroMateBLE ble;
CameraManager camera;
AudioManager audio;
LEDManager leds;

// Ã¢â€â‚¬Ã¢â€â‚¬ Screen modes Ã¢â€â‚¬Ã¢â€â‚¬
enum class ScreenMode : uint8_t {
    BOOT = 0,
    FACE = 1,
    DASHBOARD = 2,
    PREVIEW = 3,
    RESULT = 4
};

ScreenMode currentScreen = ScreenMode::BOOT;

String latestAiResult = "";
bool resultDrawn = false;

// Ã¢â€â‚¬Ã¢â€â‚¬ Timing Ã¢â€â‚¬Ã¢â€â‚¬
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
float currentFPS = 0;
unsigned long fpsTimer = 0;
const unsigned long FRAME_INTERVAL = 1000 / DISPLAY_FPS;

// Ã¢â€â‚¬Ã¢â€â‚¬ Demo mode (active only when BLE is disconnected) Ã¢â€â‚¬Ã¢â€â‚¬
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

// Ã¢â€â‚¬Ã¢â€â‚¬ Boot button polling Ã¢â€â‚¬Ã¢â€â‚¬
#define BTN_PIN 0
bool lastButtonState = HIGH;
unsigned long lastBtnDebounce = 0;
const unsigned long BTN_DEBOUNCE_MS = 250;
uint8_t btnPressCount = 0;

// Ã¢â€â‚¬Ã¢â€â‚¬ Dashboard data Ã¢â€â‚¬Ã¢â€â‚¬
unsigned long bootTime = 0;
bool dashboardDrawn = false; // Tracks whether static dashboard elements have been drawn

// Ã¢â€â‚¬Ã¢â€â‚¬ NVS Persistent Scan Counter Ã¢â€â‚¬Ã¢â€â‚¬
Preferences nvsPrefs;
uint32_t totalScanCount = 0;
uint32_t diseasesFound = 0;

// Ã¢â€â‚¬Ã¢â€â‚¬ Weather data (synced from backend via BLE) Ã¢â€â‚¬Ã¢â€â‚¬
float weatherTemp = 0.0f;
int weatherHumidity = 0;
char weatherForecast[32] = "No Data";
bool weatherSynced = false;

// Ã¢â€â‚¬Ã¢â€â‚¬ Hardware init status (set during boot) Ã¢â€â‚¬Ã¢â€â‚¬
bool hwCameraOk = false;
bool hwMicOk = false;
bool hwSpeakerOk = false;
bool hwLedsOk = false;
bool hwBleOk = false;

// Ã¢â€â‚¬Ã¢â€â‚¬ Color palette (matching FaceRenderer) Ã¢â€â‚¬Ã¢â€â‚¬
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
#define UI_DARK_GREEN 0x03E0
#define UI_DARK_RED   0x7800
#define UI_LIGHT_GRAY 0xC618
#define UI_AMBER      0xFE60

// Ã¢â€â‚¬Ã¢â€â‚¬ Forward declarations Ã¢â€â‚¬Ã¢â€â‚¬
void drawHomeScreen();
void drawBootScreen(float progress, const char* status);
void drawDashboardStatic();
void drawDashboardDynamic();
void drawStatusBar();
void drawDiseaseResultScreen();
void drawHealthyResultScreen();
void handleButton();
void updateDemo();
void handleBLECommand(uint8_t* data, size_t length);
void yuv422_to_rgb565_fast(const uint8_t* yuv, uint16_t* rgb, int width, int height);
void runScanPreview();

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  YUV422 to RGB565 conversion (Fast Integer implementation)
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Live Camera Scanning Preview & Capture
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Run Scan Preview Ã¢â‚¬â€ Simplified Scanning Screen
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
void runScanPreview() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);
    
    // Draw scanning UI
    tft->drawRoundRect(20, 40, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 80, 8, UI_CYAN);
    tft->fillRect(21, 41, SCREEN_WIDTH - 42, 30, UI_DARK_BLUE);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
    tft->drawString("AGROMATE VISION SCAN", SCREEN_WIDTH / 2, 56);
    
    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString("WARMING UP CAMERA...", SCREEN_WIDTH / 2, 120);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Hold the camera steady near the plant", SCREEN_WIDTH / 2, 160);
    
    face.setExpression(Expression::SCANNING);
    leds.setPattern(LEDPattern::PULSE, CRGB::Green, 80);
    
    // Reset diagnostic counters
    camera.warmupSuccessCount = 0;
    camera.warmupFailCount = 0;
    
    // Warm up camera for 2 seconds Ã¢â‚¬â€ flush old frames, let auto-exposure adjust
    unsigned long startTime = millis();
    while (millis() - startTime < 2000) {
        camera_fb_t* fb = camera.captureRaw();
        if (fb) {
            camera.warmupSuccessCount++;
            camera.releaseFrame();
        } else {
            camera.warmupFailCount++;
        }
        delay(50);
        ble.update();
        leds.update();
    }
    
    // Show warm-up results on screen
    tft->fillRect(21, 100, SCREEN_WIDTH - 42, 120, UI_BG);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    char warmBuf[64];
    snprintf(warmBuf, sizeof(warmBuf), "Warmup: %d OK / %d FAIL  Heap: %u",
             camera.warmupSuccessCount, camera.warmupFailCount, ESP.getFreeHeap());
    tft->drawString(warmBuf, SCREEN_WIDTH / 2, 100);
    
    tft->setTextSize(2);
    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString("CAPTURING...", SCREEN_WIDTH / 2, 130);
    
    // Capture final photo
    size_t finalJpgSize = 0;
    uint8_t* finalJpg = camera.capture(&finalJpgSize);
    
    if (finalJpg && finalJpgSize > 0) {
        // SUCCESS Ã¢â‚¬â€ show analyzing screen and send
        tft->fillScreen(UI_BG);
        tft->drawRoundRect(20, 40, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 80, 8, UI_CYAN);
        tft->fillRect(21, 41, SCREEN_WIDTH - 42, 30, UI_DARK_BLUE);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(2);
        tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
        tft->drawString("ANALYZING SCAN...", SCREEN_WIDTH / 2, 56);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("SENDING TO AI SERVER...", SCREEN_WIDTH / 2, 130);
        
        totalScanCount++;
        nvsPrefs.putUInt("scans", totalScanCount);
        
        ble.sendImage(finalJpg, finalJpgSize);
        
        face.setExpression(Expression::THINKING);
        leds.setPattern(LEDPattern::PULSE, CRGB::Cyan, 120);
        
        // Switch back to face mode to prevent infinite scan loop
        currentScreen = ScreenMode::FACE;
    } else {
        // FAILURE Ã¢â‚¬â€ show detailed diagnostic info on screen
        tft->fillScreen(UI_BG);
        tft->drawRoundRect(20, 20, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 40, 8, UI_RED);
        tft->fillRect(21, 21, SCREEN_WIDTH - 42, 30, 0x8000); // Dark red header
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(2);
        tft->setTextColor(UI_WHITE, 0x8000);
        tft->drawString("CAPTURE FAILED", SCREEN_WIDTH / 2, 36);
        
        tft->setTextSize(1);
        tft->setTextColor(UI_WHITE, UI_BG);
        
        // Line 1: Error code
        char errBuf[64];
        const char* errName = "Unknown";
        if (camera.lastError == -1) errName = "NOT_INITIALIZED";
        else if (camera.lastError == 1) errName = "FB_GET_TIMEOUT";
        else if (camera.lastError == 2) errName = "JPG_CONVERT_FAIL";
        snprintf(errBuf, sizeof(errBuf), "Error: %d (%s)", camera.lastError, errName);
        tft->drawString(errBuf, SCREEN_WIDTH / 2, 80);
        
        // Line 2: Camera state
        snprintf(errBuf, sizeof(errBuf), "Camera init: %s  Raw: %u bytes",
                 camera.isAvailable() ? "YES" : "NO", camera.lastRawSize);
        tft->drawString(errBuf, SCREEN_WIDTH / 2, 110);
        
        // Line 3: Warm-up results
        snprintf(errBuf, sizeof(errBuf), "Warmup frames: %d OK / %d FAIL",
                 camera.warmupSuccessCount, camera.warmupFailCount);
        tft->drawString(errBuf, SCREEN_WIDTH / 2, 140);
        
        // Line 4: Free heap
        snprintf(errBuf, sizeof(errBuf), "Free heap: %u bytes", ESP.getFreeHeap());
        tft->drawString(errBuf, SCREEN_WIDTH / 2, 170);
        
        // Line 5: Largest free block
        snprintf(errBuf, sizeof(errBuf), "Largest block: %u bytes", ESP.getMaxAllocHeap());
        tft->drawString(errBuf, SCREEN_WIDTH / 2, 200);
        
        tft->setTextSize(2);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("Press BOOT to dismiss", SCREEN_WIDTH / 2, 260);
        
        face.setExpression(Expression::ERROR);
        leds.setPattern(LEDPattern::SOLID, CRGB::Red);
        
        // Wait for user to read the diagnostics (or 15 seconds)
        unsigned long errorTime = millis();
        while (millis() - errorTime < 15000) {
            if (digitalRead(BTN_PIN) == LOW) break;
            delay(50);
        }
        currentScreen = ScreenMode::FACE;
    }
    
    tft->fillScreen(UI_BG);
}

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  BLE Command Callback Processor
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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
            if (payloadLen > 1) {
                uint8_t exprVal = payload[0];
                Expression expr = static_cast<Expression>(exprVal);
                face.setExpression(expr);
                
                String responseText = "";
                for (size_t i = 1; i < payloadLen; i++) {
                    responseText += (char)payload[i];
                }
                
                latestAiResult = responseText;
                currentScreen = ScreenMode::RESULT;
                display.getTFT()->fillScreen(UI_BG);
                resultDrawn = false; // Allow drawing the new result
                Serial.printf("[BLE] Switching to RESULT screen. Expr: %d, Response: %s\n", exprVal, responseText.c_str());
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
                } else if (mode == 2 && payloadLen >= 6) {
                    // Weather data sync: mode(1) + temp(4 float) + humidity(1) + forecast(rest)
                    memcpy(&weatherTemp, payload + 1, 4);
                    weatherHumidity = payload[5];
                    if (payloadLen > 6) {
                        size_t fLen = min((size_t)(payloadLen - 6), (size_t)31);
                        memcpy(weatherForecast, payload + 6, fLen);
                        weatherForecast[fLen] = '\0';
                    }
                    weatherSynced = true;
                    if (currentScreen == ScreenMode::DASHBOARD) {
                        dashboardDrawn = false; // Force redraw
                    }
                    Serial.printf("[BLE] Weather synced: %.1fC, %d%%, %s\n", weatherTemp, weatherHumidity, weatherForecast);
                } else if (mode == 3) {
                    // Disease detected Ã¢â‚¬â€ increment disease counter
                    diseasesFound++;
                    nvsPrefs.putUInt("diseases", diseasesFound);
                    if (currentScreen == ScreenMode::DASHBOARD) {
                        dashboardDrawn = false; // Force redraw
                    }
                    Serial.printf("[BLE] Disease count updated: %u\n", diseasesFound);
                } else if (mode == 4) {
                    // Stats sync: mode(1) + scans(4 bytes uint32) + diseases(4 bytes uint32)
                    if (payloadLen >= 9) {
                        uint32_t sCount = 0;
                        uint32_t dCount = 0;
                        memcpy(&sCount, payload + 1, 4);
                        memcpy(&dCount, payload + 5, 4);
                        totalScanCount = sCount;
                        diseasesFound = dCount;
                        nvsPrefs.putUInt("scans", totalScanCount);
                        nvsPrefs.putUInt("diseases", diseasesFound);
                        if (currentScreen == ScreenMode::DASHBOARD) {
                            dashboardDrawn = false; // Force redraw
                        }
                        Serial.printf("[BLE] Stats synced: Scans=%u, Diseases=%u\n", totalScanCount, diseasesFound);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Boot Screen Ã¢â‚¬â€ Animated startup with progress bar
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Dashboard Screen Ã¢â‚¬â€ Static elements (drawn once)
//  Professional 4-card layout: Stats | Weather | Hardware | System
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
void drawDashboardStatic() {
    TFT_eSPI* tft = display.getTFT();
    
    tft->fillScreen(UI_BG);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Header Bar Ã¢â€â‚¬Ã¢â€â‚¬
    tft->fillRect(0, 0, SCREEN_WIDTH, 32, UI_DARK_GRAY);
    tft->drawFastHLine(0, 32, SCREEN_WIDTH, UI_CYAN);
    tft->drawFastHLine(0, 33, SCREEN_WIDTH, UI_DARK_CYAN);
    
    tft->setTextDatum(ML_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" AGROMATE DASHBOARD", 8, 16);
    
    // Card layout constants
    int cardX1 = 8;
    int cardX2 = 246;
    int cardW = 226;
    int cardH = 98;
    int row1Y = 42;
    int row2Y = 148;
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 1: SCAN STATISTICS Ã¢â€â‚¬Ã¢â€â‚¬
    tft->drawRoundRect(cardX1, row1Y, cardW, cardH, 5, UI_GREEN);
    tft->fillRect(cardX1 + 1, row1Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX1 + 1, row1Y + 21, cardW - 2, UI_GREEN);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_GREEN, UI_DARK_GRAY);
    tft->drawString(" SCAN STATISTICS", cardX1 + 6, row1Y + 5);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 2: WEATHER Ã¢â€â‚¬Ã¢â€â‚¬
    tft->drawRoundRect(cardX2, row1Y, cardW, cardH, 5, UI_YELLOW);
    tft->fillRect(cardX2 + 1, row1Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX2 + 1, row1Y + 21, cardW - 2, UI_YELLOW);
    tft->setTextColor(UI_YELLOW, UI_DARK_GRAY);
    tft->drawString(" WEATHER", cardX2 + 6, row1Y + 5);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 3: HARDWARE STATUS Ã¢â€â‚¬Ã¢â€â‚¬
    tft->drawRoundRect(cardX1, row2Y, cardW, cardH, 5, UI_ORANGE);
    tft->fillRect(cardX1 + 1, row2Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX1 + 1, row2Y + 21, cardW - 2, UI_ORANGE);
    tft->setTextColor(UI_ORANGE, UI_DARK_GRAY);
    tft->drawString(" HARDWARE STATUS", cardX1 + 6, row2Y + 5);
    
    // Hardware status rows (static Ã¢â‚¬â€ real init flags)
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    int hwY = row2Y + 28;
    int hwLineH = 14;
    
    // Camera status
    tft->setTextColor(hwCameraOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwCameraOk ? "[OK]" : "[XX]", cardX1 + 8, hwY);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Camera OV7670", cardX1 + 42, hwY);
    
    // Microphone status
    tft->setTextColor(hwMicOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwMicOk ? "[OK]" : "[XX]", cardX1 + 8, hwY + hwLineH);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Microphone INMP441", cardX1 + 42, hwY + hwLineH);
    
    // Speaker status
    tft->setTextColor(hwSpeakerOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwSpeakerOk ? "[OK]" : "[XX]", cardX1 + 8, hwY + hwLineH * 2);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Speaker MAX98357A", cardX1 + 42, hwY + hwLineH * 2);
    
    // LED status
    tft->setTextColor(hwLedsOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwLedsOk ? "[OK]" : "[XX]", cardX1 + 8, hwY + hwLineH * 3);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("LEDs WS2812B", cardX1 + 42, hwY + hwLineH * 3);
    
    // BLE status (drawn once, updated dynamically)
    tft->setTextColor(hwBleOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwBleOk ? "[OK]" : "[XX]", cardX1 + 118, hwY);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("BLE Init", cardX1 + 152, hwY);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 4: SYSTEM Ã¢â€â‚¬Ã¢â€â‚¬
    tft->drawRoundRect(cardX2, row2Y, cardW, cardH, 5, UI_CYAN);
    tft->fillRect(cardX2 + 1, row2Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX2 + 1, row2Y + 21, cardW - 2, UI_CYAN);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" SYSTEM", cardX2 + 6, row2Y + 5);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Footer Ã¢â€â‚¬Ã¢â€â‚¬
    int footerY = 258;
    tft->drawRoundRect(8, footerY, SCREEN_WIDTH - 16, 28, 4, UI_CYAN);
    tft->fillRect(9, footerY + 1, SCREEN_WIDTH - 18, 26, UI_DARK_BLUE);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("[ PRESS BOOT TO RETURN TO FACE ]", SCREEN_WIDTH / 2, footerY + 14);
    
    dashboardDrawn = true;
}

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Dashboard Screen Ã¢â‚¬â€ Dynamic elements (updated each tick)
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
void drawDashboardDynamic() {
    TFT_eSPI* tft = display.getTFT();
    
    int cardX1 = 8;
    int cardX2 = 246;
    int cardW = 226;
    int row1Y = 42;
    int row2Y = 148;
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Header: BLE connection status Ã¢â€â‚¬Ã¢â€â‚¬
    tft->setTextDatum(MR_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(ble.isConnected() ? UI_GREEN : UI_RED, UI_DARK_GRAY);
    tft->drawString(ble.isConnected() ? "BLE: SYNCED  " : "BLE: WAITING ", SCREEN_WIDTH - 8, 16);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 1: Scan Stats (dynamic values) Ã¢â€â‚¬Ã¢â€â‚¬
    tft->fillRect(cardX1 + 7, row1Y + 25, cardW - 14, 68, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    char scansBuf[32];
    snprintf(scansBuf, sizeof(scansBuf), "Total Scans:  %u", totalScanCount);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString(scansBuf, cardX1 + 8, row1Y + 28);
    
    char diseasesBuf[32];
    snprintf(diseasesBuf, sizeof(diseasesBuf), "Diseases:     %u", diseasesFound);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString(diseasesBuf, cardX1 + 8, row1Y + 44);
    
    // Scan success rate
    if (totalScanCount > 0) {
        int healthyPct = (int)(((float)(totalScanCount - diseasesFound) / totalScanCount) * 100);
        char rateBuf[32];
        snprintf(rateBuf, sizeof(rateBuf), "Healthy Rate: %d%%", healthyPct);
        tft->setTextColor(healthyPct >= 70 ? UI_GREEN : UI_YELLOW, UI_BG);
        tft->drawString(rateBuf, cardX1 + 8, row1Y + 60);
    } else {
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("No scans yet", cardX1 + 8, row1Y + 60);
    }
    
    // Scan progress bar
    int barX = cardX1 + 8;
    int barY = row1Y + 78;
    int barW = cardW - 16;
    tft->drawRect(barX, barY, barW, 8, UI_MID_GRAY);
    if (totalScanCount > 0 && totalScanCount > diseasesFound) {
        float healthyRatio = (float)(totalScanCount - diseasesFound) / totalScanCount;
        tft->fillRect(barX + 1, barY + 1, (int)((barW - 2) * healthyRatio), 6, UI_GREEN);
    }
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 2: Weather (dynamic values) Ã¢â€â‚¬Ã¢â€â‚¬
    tft->fillRect(cardX2 + 7, row1Y + 25, cardW - 14, 68, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    if (weatherSynced) {
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "Temp:     %.1f C", weatherTemp);
        tft->setTextColor(UI_WHITE, UI_BG);
        tft->drawString(tempBuf, cardX2 + 8, row1Y + 28);
        
        char humBuf[32];
        snprintf(humBuf, sizeof(humBuf), "Humidity: %d%%", weatherHumidity);
        tft->drawString(humBuf, cardX2 + 8, row1Y + 44);
        
        char fcastBuf[40];
        snprintf(fcastBuf, sizeof(fcastBuf), "Forecast: %s", weatherForecast);
        tft->drawString(fcastBuf, cardX2 + 8, row1Y + 60);
    } else {
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("Awaiting sync...", cardX2 + 8, row1Y + 28);
        tft->drawString("Connect via BLE", cardX2 + 8, row1Y + 44);
        tft->drawString("to fetch weather", cardX2 + 8, row1Y + 60);
    }
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Card 4: System Info (dynamic values) Ã¢â€â‚¬Ã¢â€â‚¬
    tft->fillRect(cardX2 + 7, row2Y + 25, cardW - 14, 68, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    
    // Uptime
    unsigned long upSec = (millis() - bootTime) / 1000;
    int hours = upSec / 3600;
    int mins = (upSec % 3600) / 60;
    int secs = upSec % 60;
    
    char uptimeBuf[32];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "Uptime:  %02d:%02d:%02d", hours, mins, secs);
    tft->drawString(uptimeBuf, cardX2 + 8, row2Y + 28);
    
    // Free heap
    char memBuf[32];
    snprintf(memBuf, sizeof(memBuf), "Heap:    %dKB free", ESP.getFreeHeap() / 1024);
    tft->drawString(memBuf, cardX2 + 8, row2Y + 44);
    
    // CPU frequency
    char cpuBuf[32];
    snprintf(cpuBuf, sizeof(cpuBuf), "CPU:     %dMHz", getCpuFrequencyMhz());
    tft->drawString(cpuBuf, cardX2 + 8, row2Y + 60);
    
    // FPS
    char fpsBuf[32];
    snprintf(fpsBuf, sizeof(fpsBuf), "FPS:     %.1f", currentFPS);
    tft->drawString(fpsBuf, cardX2 + 8, row2Y + 76);
}

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Home Screen (AGRO MATE AI BY CHIRAG ST MARY'S SCHOOL)
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Status Bar
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
// =============================================================
//  Helper: draw text list items (semicolon-separated) on TFT
// =============================================================
static void drawListItems(TFT_eSPI* tft, const String& src,
                          int x, int& y, int maxItems,
                          int maxChars, int lineH, const char* prefix) {
    int start = 0, count = 0;
    for (int i = 0; i <= (int)src.length() && count < maxItems; i++) {
        if (i == (int)src.length() || src.charAt(i) == ';') {
            String item = src.substring(start, i);
            item.trim();
            if (item.length() > 0) {
                if ((int)item.length() > maxChars) item = item.substring(0, maxChars - 1) + ".";
                char buf[64];
                snprintf(buf, sizeof(buf), "%s %s", prefix, item.c_str());
                tft->drawString(buf, x, y);
                y += lineH;
                count++;
            }
            start = i + 1;
        }
    }
}

// =============================================================
//  Helper: word-wrap up to maxLines onto TFT
// =============================================================
static void drawWrappedText(TFT_eSPI* tft, const String& text, int x, int& y,
                            int maxChars, int lineH, int maxLines) {
    String rem = text;
    int drawn = 0;
    while (rem.length() > 0 && drawn < maxLines) {
        int cut = (int)rem.length();
        if (cut > maxChars) {
            cut = maxChars;
            for (int i = maxChars - 1; i > 0; i--) {
                if (rem.charAt(i) == ' ') { cut = i; break; }
            }
        }
        String line = rem.substring(0, cut);
        line.trim();
        char buf[64];
        line.substring(0, 62).toCharArray(buf, sizeof(buf));
        tft->drawString(buf, x, y);
        y += lineH;
        rem = rem.substring(cut);
        rem.trim();
        drawn++;
    }
}

// =============================================================
// =============================================================
//  DISEASE RESULT SCREEN  320 x 480  portrait
//  Flows top-to-bottom filling the full 480px height
// =============================================================
void drawDiseaseResultScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);

    // Parse: D|Plant|Sci|Conf|Disease|Severity|HealthScore|Symptoms|Treatments|Summary
    String fields[10];
    int fi = 0, sp = 0;
    for (int i = 0; i <= (int)latestAiResult.length() && fi < 10; i++) {
        if (i == (int)latestAiResult.length() || latestAiResult.charAt(i) == '|') {
            fields[fi++] = latestAiResult.substring(sp, i);
            sp = i + 1;
        }
    }
    String plantName = (fi > 1) ? fields[1] : "Unknown";
    String sciName   = (fi > 2) ? fields[2] : "";
    String confStr   = (fi > 3) ? fields[3] : "0";
    String disease   = (fi > 4) ? fields[4] : "Unknown";
    String severity  = (fi > 5) ? fields[5] : "MODERATE";
    int    hscore    = (fi > 6) ? fields[6].toInt() : 50;
    String symptoms  = (fi > 7) ? fields[7] : "Visible damage;Discoloration";
    String treatment = (fi > 8) ? fields[8] : "Consult an agronomist";
    String summary   = (fi > 9) ? fields[9] : "";

    severity.toUpperCase();
    if (plantName.length() > 24) plantName = plantName.substring(0, 23) + ".";
    if (sciName.length()   > 42) sciName   = sciName.substring(0, 41)   + ".";
    if (disease.length()   > 24) disease   = disease.substring(0, 23)   + ".";

    const int X  = 6;
    const int W  = 308;
    const int CX = 160;
    int y = 0;

    // ── HEADER BAR (34px) ──────────────────────────────────────
    tft->fillRect(0, 0, 320, 34, UI_DARK_RED);
    tft->drawFastHLine(0, 34, 320, UI_RED);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_RED);
    tft->drawString("-- AGROMATE -- DISEASE DETECTED --", CX, 17);
    y = 38;

    // ── PLANT NAME (size:2, bold white, 20px) ──────────────────
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE);
    tft->drawString(plantName, X, y);
    y += 20;

    // ── Sci name (size:1, gray, 11px) ──────────────────────────
    tft->setTextSize(1);
    tft->setTextColor(UI_LIGHT_GRAY);
    if (sciName.length() > 0) tft->drawString(sciName, X, y);
    y += 11;

    // ── Confidence (size:1, cyan, 12px) ────────────────────────
    char cbuf[30]; snprintf(cbuf, sizeof(cbuf), "Match: %s%%  Severity: %s", confStr.c_str(), severity.c_str());
    tft->setTextColor(UI_DARK_CYAN);
    tft->drawString(cbuf, X, y);
    y += 12;

    // ── DIVIDER ─────────────────────────────────────────────────
    tft->drawFastHLine(0, y, 320, UI_MID_GRAY);
    y += 4;

    // ── DISEASE NAME (size:2, amber, 20px) ─────────────────────
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN);
    tft->drawString("DIAGNOSIS:", X, y);
    y += 11;
    tft->setTextSize(2);
    tft->setTextColor(UI_AMBER);
    tft->drawString(disease, X, y);
    y += 21;
    tft->setTextSize(1);

    // ── SEVERITY BADGE + HEALTH SCORE on same row ──────────────
    uint16_t sevC = UI_YELLOW;
    if      (severity == "SEVERE" || severity == "HIGH")  sevC = UI_RED;
    else if (severity == "MILD"   || severity == "LOW")   sevC = UI_GREEN;
    else if (severity == "CRITICAL")                       sevC = 0xF81F;

    tft->fillRoundRect(X, y, 90, 16, 3, sevC);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(0x0000);
    tft->drawString(severity, X + 45, y + 8);

    tft->fillRoundRect(X + 98, y, 76, 16, 3, UI_RED);
    tft->setTextColor(UI_WHITE);
    tft->drawString("DISEASED", X + 136, y + 8);

    // Score % on right side of same row
    char hs[8]; snprintf(hs, sizeof(hs), "%d%%", hscore);
    tft->setTextDatum(MR_DATUM);
    uint16_t bC = (hscore > 60) ? UI_GREEN : (hscore > 30) ? UI_YELLOW : UI_RED;
    tft->setTextColor(bC);
    tft->drawString(hs, 314, y + 8);
    tft->setTextDatum(TL_DATUM);
    y += 22;

    // ── HEALTH BAR (12px) ───────────────────────────────────────
    tft->drawRect(X, y, W, 12, UI_MID_GRAY);
    tft->fillRect(X + 2, y + 2, (W - 4) * hscore / 100, 8, bC);
    y += 16;

    // ── SYMPTOMS ────────────────────────────────────────────────
    tft->drawFastHLine(0, y, 320, UI_MID_GRAY);
    y += 3;
    tft->setTextColor(UI_CYAN);
    tft->drawString("SYMPTOMS:", X, y);
    y += 11;
    tft->setTextColor(UI_AMBER);
    drawListItems(tft, symptoms, X + 4, y, 4, 46, 11, "*");

    // ── TREATMENT ───────────────────────────────────────────────
    tft->drawFastHLine(0, y, 320, UI_MID_GRAY);
    y += 3;
    tft->setTextColor(UI_GREEN);
    tft->drawString("TREATMENT:", X, y);
    y += 11;
    tft->setTextColor(UI_WHITE);
    drawListItems(tft, treatment, X + 4, y, 4, 46, 11, "+");

    // ── AI SUMMARY ──────────────────────────────────────────────
    if (summary.length() > 0) {
        tft->drawFastHLine(0, y, 320, UI_MID_GRAY);
        y += 3;
        tft->setTextColor(UI_DARK_CYAN);
        tft->drawString("AI SUMMARY:", X, y);
        y += 11;
        tft->setTextColor(UI_LIGHT_GRAY);
        drawWrappedText(tft, summary, X, y, 50, 11, 3);
    }

    // ── BOTTOM DISMISS HINT (last 14px) ─────────────────────────
    tft->fillRect(0, 466, 320, 14, 0x1082);  // very dark bg strip
    tft->drawFastHLine(0, 466, 320, UI_MID_GRAY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("[BOOT] Press to dismiss", CX, 473);
    tft->setTextDatum(TL_DATUM);
}

// =============================================================
//  HEALTHY RESULT SCREEN   320 x 480  portrait
// =============================================================
void drawHealthyResultScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);

    // Parse: H|Plant|Sci|Conf|HealthScore|Tips
    String fields[6];
    int fi = 0, sp = 0;
    for (int i = 0; i <= (int)latestAiResult.length() && fi < 6; i++) {
        if (i == (int)latestAiResult.length() || latestAiResult.charAt(i) == '|') {
            fields[fi++] = latestAiResult.substring(sp, i);
            sp = i + 1;
        }
    }
    String plantName = (fi > 1) ? fields[1] : "Unknown";
    String sciName   = (fi > 2) ? fields[2] : "";
    String confStr   = (fi > 3) ? fields[3] : "0";
    int    hscore    = (fi > 4) ? fields[4].toInt() : 95;
    String tips      = (fi > 5) ? fields[5] : "Water regularly;Check sunlight;Ensure drainage;Apply compost";

    if (plantName.length() > 24) plantName = plantName.substring(0, 23) + ".";
    if (sciName.length()   > 42) sciName   = sciName.substring(0, 41)   + ".";

    const int X  = 6;
    const int W  = 308;
    const int CX = 160;
    int y = 0;

    // ── HEADER BAR (34px, green) ────────────────────────────────
    tft->fillRect(0, 0, 320, 34, UI_DARK_GREEN);
    tft->drawFastHLine(0, 34, 320, UI_GREEN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_GREEN);
    tft->drawString("-- AGROMATE -- PLANT SCAN RESULT --", CX, 17);
    y = 38;

    // ── PLANT NAME (size:2, centred, 22px) ─────────────────────
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE);
    tft->drawString(plantName, CX, y);
    y += 22;

    // ── Sci name + Confidence (size:1, centred) ─────────────────
    tft->setTextSize(1);
    tft->setTextColor(UI_LIGHT_GRAY);
    if (sciName.length() > 0) tft->drawString(sciName, CX, y);
    y += 11;
    char cbuf[30]; snprintf(cbuf, sizeof(cbuf), "Confidence: %s%%", confStr.c_str());
    tft->setTextColor(UI_DARK_CYAN);
    tft->drawString(cbuf, CX, y);
    y += 14;

    // ── HEALTHY STATUS BANNER (40px) ────────────────────────────
    tft->fillRoundRect(X, y, W, 40, 6, UI_GREEN);
    tft->setTextSize(2);
    tft->setTextColor(0x0000, UI_GREEN);
    tft->drawString("PLANT IS HEALTHY", CX, y + 20);
    y += 48;

    // ── HEALTH SCORE BAR ────────────────────────────────────────
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(UI_CYAN);
    tft->drawString("HEALTH SCORE", X, y);
    char hs[8]; snprintf(hs, sizeof(hs), "%d%%", hscore);
    tft->setTextDatum(TR_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString(hs, 314, y);
    tft->setTextDatum(TL_DATUM);
    y += 11;
    tft->drawRect(X, y, W, 14, UI_MID_GRAY);
    tft->fillRect(X + 2, y + 2, (W - 4) * hscore / 100, 10, UI_GREEN);
    y += 18;

    // ── DIVIDER ─────────────────────────────────────────────────
    tft->drawFastHLine(0, y, 320, UI_MID_GRAY);
    y += 4;

    // ── CARE RECOMMENDATIONS ─────────────────────────────────────
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("CARE RECOMMENDATIONS", CX, y);
    y += 13;
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(UI_WHITE);
    drawListItems(tft, tips, X + 4, y, 6, 46, 13, ">");

    // ── MOTIVATIONAL MESSAGE ─────────────────────────────────────
    tft->drawFastHLine(0, y + 4, 320, UI_MID_GRAY);
    y += 8;
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_LIGHT_GRAY);
    tft->drawString("Your crop looks great! Keep monitoring.", CX, y);
    y += 13;
    tft->drawString("Regular scans help catch issues early.", CX, y);

    // ── BOTTOM DISMISS HINT ──────────────────────────────────────
    tft->fillRect(0, 466, 320, 14, 0x1082);
    tft->drawFastHLine(0, 466, 320, UI_MID_GRAY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("[BOOT] Press to dismiss", CX, 473);
    tft->setTextDatum(TL_DATUM);
}

void handleButton() {
    static unsigned long lastDebounceTime = 0;
    static bool debouncedState = HIGH;
    static bool lastRawState = HIGH;

    bool rawState = digitalRead(BTN_PIN);
    unsigned long now = millis();

    if (rawState != lastRawState) {
        lastDebounceTime = now;
        lastRawState = rawState;
    }

    if ((now - lastDebounceTime) > BTN_DEBOUNCE_MS) {
        if (rawState != debouncedState) {
            debouncedState = rawState;
            if (debouncedState == LOW) { // Press event (active-low BOOT button)
                btnPressCount++;
                Serial.printf("[BTN] Valid press #%d, switching screen from %d...\n", btnPressCount, (int)currentScreen);

                if (currentScreen == ScreenMode::FACE) {
                    currentScreen = ScreenMode::DASHBOARD;
                    dashboardDrawn = false;
                } else if (currentScreen == ScreenMode::DASHBOARD) {
                    currentScreen = ScreenMode::FACE;
                    display.getTFT()->fillScreen(UI_BG);
                } else if (currentScreen == ScreenMode::RESULT) {
                    currentScreen = ScreenMode::FACE;
                    display.getTFT()->fillScreen(UI_BG);
                    resultDrawn = false;
                }
            }
        }
    }
}

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Demo Mode updates (active only when BLE is not synced)
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Setup
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
void setup() {
    // Set custom base MAC address to bypass Windows BLE caching
    uint8_t customMac[] = {0x28, 0x84, 0x85, 0x4D, 0x11, 0x50};
    esp_base_mac_addr_set(customMac);

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
    Serial.println("   AgroMate Robot Ã¢â‚¬â€ Production Build v3.0");
    Serial.println("=============================================");
    
    // Load persistent scan counter from NVS flash
    nvsPrefs.begin("agromate", false);
    totalScanCount = nvsPrefs.getUInt("scans", 0);
    diseasesFound = nvsPrefs.getUInt("diseases", 0);
    Serial.printf("[NVS] Loaded scan count: %u, diseases: %u\n", totalScanCount, diseasesFound);
    
    // TFT RD line is physically not connected, no need to drive GPIO 20.
 
    // Configure BOOT button for Face Ã¢â€ â€ Dashboard toggle
    pinMode(BTN_PIN, INPUT_PULLUP);
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Actual Sequential System Initialization Ã¢â€â‚¬Ã¢â€â‚¬
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
    hwCameraOk = (cameraErr == 0);
    
    // 2. Release hardware UART0 serial port pins (GPIO 43/44) so they can be driven by the Display parallel bus
    Serial.println("[TFT] Releasing serial port pins (GPIO 43/44) for Display bus...");
    Serial.flush();
    Serial.end();

    // Initialize display screen next
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
    hwBleOk = true;
    delay(150);
    
    drawBootScreen(0.70f, "Hooking I2S Mic & Speaker...");
    audio.begin();
    hwMicOk = true;
    hwSpeakerOk = true;
    delay(150);
    
    drawBootScreen(0.85f, "Syncing WS2812B Status LEDs...");
    leds.begin();
    hwLedsOk = true;
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

// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
//  Main Loop
// Ã¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€ÂÃ¢â€Â
void loop() {
    unsigned long now = millis();
    
    // Consume interrupt-driven button press
    handleButton();
    
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
    
    // Camera frame release monitor after BLE transmission completes
    static bool lastTxActive = false;
    bool currentTxActive = ble.isTxActive();
    if (lastTxActive && !currentTxActive) {
        camera.releaseFrame();
        Serial.println("[CAM] BLE Image transfer complete. Released camera frame buffer.");
    }
    lastTxActive = currentTxActive;
    
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
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Screen rendering engine Ã¢â€â‚¬Ã¢â€â‚¬
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
    } else if (currentScreen == ScreenMode::RESULT) {
        if (!resultDrawn) {
            // Dispatch to the appropriate result screen based on the first character
            if (latestAiResult.startsWith("D|")) {
                drawDiseaseResultScreen();
            } else if (latestAiResult.startsWith("H|")) {
                drawHealthyResultScreen();
            } else {
                // Fallback: legacy plain text result
                display.getTFT()->fillScreen(UI_BG);
                display.getTFT()->fillRect(0, 0, SCREEN_WIDTH, 28, UI_DARK_BLUE);
                display.getTFT()->setTextColor(UI_WHITE);
                display.getTFT()->setTextDatum(MC_DATUM);
                display.getTFT()->setTextSize(1);
                display.getTFT()->drawString("AI DETECTION RESULT", SCREEN_WIDTH / 2, 14);
                display.getTFT()->setTextSize(1);
                display.getTFT()->setTextColor(UI_CYAN);
                display.getTFT()->setTextDatum(TL_DATUM);
                int cursorY = 40;
                int cursorX = 10;
                int idx = 0;
                while(idx < (int)latestAiResult.length()) {
                    int nl = latestAiResult.indexOf('\n', idx);
                    String line = (nl == -1) ? latestAiResult.substring(idx) : latestAiResult.substring(idx, nl);
                    display.getTFT()->drawString(line, cursorX, cursorY);
                    cursorY += 14;
                    idx = (nl == -1) ? (int)latestAiResult.length() : nl + 1;
                }
            }
            resultDrawn = true;
        }
    }
    
    // Voice Bar Overlay
    if (audio.isPlaying()) {
        static unsigned long lastVoiceAnim = 0;
        if (now - lastVoiceAnim > 50) {
            lastVoiceAnim = now;
            int barWidth = SCREEN_WIDTH - 40;
            int barHeight = 20;
            int barX = 20;
            int barY = SCREEN_HEIGHT - 30;
            
            // Background
            display.getTFT()->fillRect(barX, barY, barWidth, barHeight, UI_DARK_GRAY);
            
            // Active level (randomized based on audio playback level)
            float level = audio.getPlaybackLevel();
            int activeBars = (int)(level * 10.0f);
            if (activeBars > 10) activeBars = 10;
            if (activeBars < 1) activeBars = 1;
            
            int segmentWidth = barWidth / 10;
            for (int i = 0; i < activeBars; i++) {
                uint16_t color = (i > 7) ? UI_RED : ((i > 4) ? UI_CYAN : UI_GREEN);
                display.getTFT()->fillRect(barX + (i * segmentWidth) + 1, barY + 2, segmentWidth - 2, barHeight - 4, color);
            }
        }
    }
    
    yield();
}
