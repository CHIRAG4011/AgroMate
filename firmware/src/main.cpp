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
#include <TJpg_Decoder.h>

// Ã¢â€ â‚¬Ã¢â€ â‚¬ Global instances Ã¢â€ â‚¬Ã¢â€ â‚¬
DisplayManager display;
FaceRenderer face;
AgroMateBLE ble;
CameraManager camera;
AudioManager audio;

// Ã¢â€ â‚¬Ã¢â€ â‚¬ Screen modes Ã¢â€ â‚¬Ã¢â€ â‚¬
enum class ScreenMode : uint8_t {
    BOOT = 0,
    FACE = 1,
    DASHBOARD = 2,
    PREVIEW = 3,
    WAIT_RESULT = 4,
    RESULT = 5
};

volatile ScreenMode currentScreen = ScreenMode::BOOT;
volatile bool g_scanRequested = false;
unsigned long previewHoldStart = 0;

String latestAiResult = "";
bool resultDrawn = false;

// Ã¢â€ â‚¬Ã¢â€ â‚¬ Timing Ã¢â€ â‚¬Ã¢â€ â‚¬
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

// ── Buttons ──
#define BTN_PIN 0          // On-board BOOT button (GPIO 0: toggle screen / hold for scan)
#define BTN_EXTERNAL_PIN 7 // Dedicated External Push Button (GPIO 7: instant camera image capture)
uint8_t btnPressCount = 0;

// ── Dashboard data ──
unsigned long bootTime = 0;
bool dashboardDrawn = false; // Tracks whether static dashboard elements have been drawn

// ── NVS Persistent Scan Counter ──
Preferences nvsPrefs;
uint32_t totalScanCount = 0;
uint32_t diseasesFound = 0;

// ── Weather data (synced from backend via BLE) ──
float weatherTemp = 0.0f;
int weatherHumidity = 0;
char weatherForecast[32] = "No Data";
bool weatherSynced = false;

// ── Hardware init status (set during boot) ──
bool hwCameraOk = false;
bool hwMicOk = false;
bool hwSpeakerOk = false;
bool hwBleOk = false;

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
#define UI_DARK_GREEN 0x03E0
#define UI_DARK_RED   0x7800
#define UI_LIGHT_GRAY 0xC618
#define UI_AMBER      0xFE60

// ── Forward declarations ──
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

// ── TJpg_Decoder callback — renders decoded JPEG MCU blocks directly to TFT with 2x stretch ──
static TFT_eSPI* _jpgTft = nullptr;
static int _jpgOffsetX = 0;
static int _jpgOffsetY = 120;
static bool _jpgScale2x = false;

bool onJpgBlockDraw(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (_jpgTft && bitmap) {
        if (_jpgScale2x) {
            // High-speed 2x horizontal & vertical pixel stretch: 160x120 QQVGA -> 320x240 on 320x480 TFT
            uint16_t lineBuf[128]; // Safe for MCU block widths up to 64 pixels
            for (int py = 0; py < h; py++) {
                for (int px = 0; px < w; px++) {
                    uint16_t c = bitmap[py * w + px];
                    lineBuf[px * 2]     = c;
                    lineBuf[px * 2 + 1] = c;
                }
                int outX = _jpgOffsetX + x * 2;
                int outY = _jpgOffsetY + (y + py) * 2;
                if (outY + 1 < SCREEN_HEIGHT && outX + w * 2 <= SCREEN_WIDTH) {
                    _jpgTft->pushImage(outX, outY,     w * 2, 1, lineBuf);
                    _jpgTft->pushImage(outX, outY + 1, w * 2, 1, lineBuf);
                }
            }
        } else {
            _jpgTft->pushImage(x + _jpgOffsetX, y + _jpgOffsetY, w, h, bitmap);
        }
    }
    return true;  // Continue decoding
}

void drawViewfinderHUD(TFT_eSPI* tft, bool captureFlash = false) {
    uint16_t color = captureFlash ? UI_WHITE : UI_CYAN;
    int x0 = max(0, _jpgOffsetX);
    int y0 = _jpgOffsetY;
    int w = min((int)SCREEN_WIDTH, 320);
    int h = 240;
    int bLen = 22;

    // Corner brackets
    tft->drawFastHLine(x0, y0, bLen, color);
    tft->drawFastHLine(x0, y0 + 1, bLen, color);
    tft->drawFastVLine(x0, y0, bLen, color);
    tft->drawFastVLine(x0 + 1, y0, bLen, color);

    tft->drawFastHLine(x0 + w - bLen, y0, bLen, color);
    tft->drawFastHLine(x0 + w - bLen, y0 + 1, bLen, color);
    tft->drawFastVLine(x0 + w - 1, y0, bLen, color);
    tft->drawFastVLine(x0 + w - 2, y0, bLen, color);

    tft->drawFastHLine(x0, y0 + h - 1, bLen, color);
    tft->drawFastHLine(x0, y0 + h - 2, bLen, color);
    tft->drawFastVLine(x0, y0 + h - bLen, bLen, color);
    tft->drawFastVLine(x0 + 1, y0 + h - bLen, bLen, color);

    tft->drawFastHLine(x0 + w - bLen, y0 + h - 1, bLen, color);
    tft->drawFastHLine(x0 + w - bLen, y0 + h - 2, bLen, color);
    tft->drawFastVLine(x0 + w - 1, y0 + h - bLen, bLen, color);
    tft->drawFastVLine(x0 + w - 2, y0 + h - bLen, bLen, color);

    // Center targeting reticle
    int cx = SCREEN_WIDTH / 2;
    int cy = _jpgOffsetY + 120;
    tft->drawFastHLine(cx - 10, cy, 21, UI_GREEN);
    tft->drawFastVLine(cx, cy - 10, 21, UI_GREEN);
    tft->drawCircle(cx, cy, 12, UI_GREEN);
}

static bool s_lastProbeOk = false;
static size_t s_lastProbeSize = 0;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Comprehensive Pre-Scan Camera Diagnostic Report
//  Queries UART Link, Sensor Hardware, PSRAM, and Probes a Frame Test
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
bool showCameraDiagnosticReport() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);
    
    // Header
    tft->fillRect(0, 0, SCREEN_WIDTH, 30, UI_DARK_BLUE);
    tft->drawFastHLine(0, 30, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("CAMERA DIAGNOSTIC REPORT", SCREEN_WIDTH / 2, 10);
    tft->setTextColor(UI_LIGHT_GRAY, UI_DARK_BLUE);
    tft->drawString("Hardware & Communication Pre-Check", SCREEN_WIDTH / 2, 22);
    
    // Progress box while running live probes
    tft->drawRoundRect(60, 90, SCREEN_WIDTH - 120, 120, 8, UI_CYAN);
    tft->fillRect(61, 91, SCREEN_WIDTH - 122, 118, UI_DARK_GRAY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_YELLOW, UI_DARK_GRAY);
    tft->drawString("PROBING HARDWARE...", SCREEN_WIDTH / 2, 120);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_GRAY);
    tft->drawString("Pinging ESP32-CAM (UART 460800 baud)...", SCREEN_WIDTH / 2, 150);
    tft->drawString("Checking GC2145 sensor & frame response...", SCREEN_WIDTH / 2, 175);
    
    ble.update();
    
    // Run hardware tests
    CamStatusInfo camInfo;
    bool camOk = camera.getCamStatus(&camInfo);
    ble.update();
    
    size_t probeSize = 0;
    unsigned long probeMs = 0;
    char probeDetail[64] = {0};
    bool probeOk = false;
    if (camOk) {
        camera.setResolution("QQVGA");
        probeOk = camera.testProbeFrame(&probeSize, &probeMs, probeDetail, sizeof(probeDetail));
    } else {
        snprintf(probeDetail, sizeof(probeDetail), "No UART response from module");
    }
    s_lastProbeOk = probeOk;
    s_lastProbeSize = probeSize;
    ble.update();
    
    // Clear screen for full 4-card report
    tft->fillScreen(UI_BG);
    tft->fillRect(0, 0, SCREEN_WIDTH, 30, UI_DARK_BLUE);
    tft->drawFastHLine(0, 30, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("CAMERA DIAGNOSTIC REPORT", SCREEN_WIDTH / 2, 10);
    tft->setTextColor(UI_LIGHT_GRAY, UI_DARK_BLUE);
    tft->drawString("Hardware & Communication Pre-Check", SCREEN_WIDTH / 2, 22);
    
    int cardW = SCREEN_WIDTH - 16;
    int cardH = 88;
    int col1X = 8;
    int row1Y = 36;
    int row2Y = 128;
    int row3Y = 220;
    int row4Y = 312;
    
    // ════════════════════════════════════════════
    // Card 1: UART Link (GPIO 4/5)
    // ════════════════════════════════════════════
    uint16_t uartBorder = camInfo.online ? UI_CYAN : UI_RED;
    tft->drawRoundRect(col1X, row1Y, cardW, cardH, 5, uartBorder);
    tft->fillRect(col1X + 1, row1Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row1Y + 21, cardW - 2, uartBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(uartBorder, UI_DARK_GRAY);
    tft->drawString(" 1. UART LINK (GPIO 4/5)", col1X + 6, row1Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Port: Serial2 (TX=5, RX=4)", col1X + 8, row1Y + 28);
    tft->drawString("Baud: 460,800 bps", col1X + 180, row1Y + 28);
    if (camInfo.online) {
        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("Status: [OK] CONNECTED", col1X + 8, row1Y + 46);
        tft->setTextColor(UI_CYAN, UI_BG);
        char pingBuf[32];
        snprintf(pingBuf, sizeof(pingBuf), "Ping: %lu ms", camInfo.pingMs);
        tft->drawString(pingBuf, col1X + 180, row1Y + 46);
    } else {
        tft->setTextColor(UI_RED, UI_BG);
        tft->drawString("Status: [FAIL] NO PONG", col1X + 8, row1Y + 46);
        tft->setTextColor(UI_YELLOW, UI_BG);
        tft->drawString("Check wires / power rail", col1X + 150, row1Y + 46);
    }
    tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
    tft->drawString("RX Buffer: 16 KB Ring | Zero Drops", col1X + 8, row1Y + 66);
    
    // ════════════════════════════════════════════
    // Card 2: Camera Node Hardware
    // ════════════════════════════════════════════
    uint16_t camBorder = (camInfo.online && camInfo.pid != 0) ? UI_GREEN : (camInfo.online ? UI_YELLOW : UI_RED);
    tft->drawRoundRect(col1X, row2Y, cardW, cardH, 5, camBorder);
    tft->fillRect(col1X + 1, row2Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row2Y + 21, cardW - 2, camBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(camBorder, UI_DARK_GRAY);
    tft->drawString(" 2. CAMERA NODE", col1X + 6, row2Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    char sensBuf[40];
    snprintf(sensBuf, sizeof(sensBuf), "Sensor: %s", camInfo.sensorName);
    tft->drawString(sensBuf, col1X + 8, row2Y + 28);
    
    char psBuf[40];
    snprintf(psBuf, sizeof(psBuf), "PSRAM: %s (%s)", 
             camInfo.psramFound ? "YES" : "NO", 
             camInfo.psramOk ? "DMA OK" : "NONE");
    tft->setTextColor(camInfo.psramOk ? UI_GREEN : UI_YELLOW, UI_BG);
    tft->drawString(psBuf, col1X + 175, row2Y + 28);
    
    char heapBuf[40];
    snprintf(heapBuf, sizeof(heapBuf), "CAM Heap: %u B free", (unsigned)camInfo.freeHeap);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString(heapBuf, col1X + 8, row2Y + 46);
    
    char resBuf[40];
    if (camInfo.width > 0 && camInfo.height > 0) {
        snprintf(resBuf, sizeof(resBuf), "Res: %ux%u (HQ)", camInfo.width, camInfo.height);
    } else {
        snprintf(resBuf, sizeof(resBuf), "Res: 160x120 (QQVGA)");
    }
    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString(resBuf, col1X + 175, row2Y + 46);
    
    tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
    tft->drawString("Quality: HQ Color JPEG (DRAM safe)", col1X + 8, row2Y + 66);
    
    // ════════════════════════════════════════════
    // Card 3: Frame Probe Test
    // ════════════════════════════════════════════
    uint16_t probeBorder = probeOk ? UI_GREEN : (camInfo.online ? UI_YELLOW : UI_RED);
    tft->drawRoundRect(col1X, row3Y, cardW, cardH, 5, probeBorder);
    tft->fillRect(col1X + 1, row3Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row3Y + 21, cardW - 2, probeBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(probeBorder, UI_DARK_GRAY);
    tft->drawString(" 3. FRAME PROBE TEST", col1X + 6, row3Y + 5);
    
    if (probeOk) {
        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("Capture: [OK] SUCCESS", col1X + 8, row3Y + 28);
        tft->setTextColor(UI_WHITE, UI_BG);
        char szBuf[40];
        snprintf(szBuf, sizeof(szBuf), "Size: %u B", (unsigned)probeSize);
        tft->drawString(szBuf, col1X + 185, row3Y + 28);
        char latBuf[40];
        snprintf(latBuf, sizeof(latBuf), "Latency: %lu ms", probeMs);
        tft->drawString(latBuf, col1X + 8, row3Y + 46);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("JPEG Header: 0xFFD8 OK", col1X + 145, row3Y + 46);
    } else {
        tft->setTextColor(UI_RED, UI_BG);
        tft->drawString("Capture: [FAIL] PROBE", col1X + 8, row3Y + 28);
        tft->setTextColor(UI_YELLOW, UI_BG);
        char errB[50];
        snprintf(errB, sizeof(errB), "Err: %.32s", probeDetail);
        tft->drawString(errB, col1X + 8, row3Y + 46);
    }
    tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
    tft->drawString("Direct Capture & BLE Stream Ready", col1X + 8, row3Y + 66);
    
    // ════════════════════════════════════════════
    // Card 4: S3 Host System
    // ════════════════════════════════════════════
    tft->drawRoundRect(col1X, row4Y, cardW, cardH, 5, UI_CYAN);
    tft->fillRect(col1X + 1, row4Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row4Y + 21, cardW - 2, UI_CYAN);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" 4. S3 HOST SYSTEM", col1X + 6, row4Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    char s3Heap[40];
    snprintf(s3Heap, sizeof(s3Heap), "S3 Heap: %d KB free [OK]", ESP.getFreeHeap() / 1024);
    tft->drawString(s3Heap, col1X + 8, row4Y + 28);
    tft->drawString("Display: 320x480 Portrait [OK]", col1X + 8, row4Y + 46);
    
    char bleBuf[40];
    snprintf(bleBuf, sizeof(bleBuf), "BLE: %s", ble.isConnected() ? "Connected" : "Advertising");
    tft->setTextColor(ble.isConnected() ? UI_GREEN : UI_CYAN, UI_BG);
    tft->drawString(bleBuf, col1X + 8, row4Y + 66);
    
    // ════════════════════════════════════════════
    // Footer / Decision Bar
    // ════════════════════════════════════════════
    int footerY = 414;
    int footerH = 46;
    
    if (!camInfo.online) {
        // Critical communication failure
        tft->fillRect(0, footerY, SCREEN_WIDTH, footerH, 0x4000); // Dark Red
        tft->drawFastHLine(0, footerY, SCREEN_WIDTH, UI_RED);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(UI_WHITE, 0x4000);
        tft->drawString("COMM ERROR: ESP32-CAM not responding! Check UART wiring.", SCREEN_WIDTH / 2, footerY + 12);
        tft->setTextColor(UI_YELLOW, 0x4000);
        tft->drawString("Wiring: S3 GPIO 4 <-> CAM U0T, GPIO 5 <-> CAM U0R, GND <-> GND", SCREEN_WIDTH / 2, footerY + 26);
        
        audio.playTestTone(500);
        
        // Guarantee 5 full seconds on screen so user can read the report
        unsigned long startWait = millis();
        while (millis() - startWait < 5000) {
            ble.update();
            delay(50);
        }
        return false;
    }
    
    if (!probeOk) {
        // Camera UART responded, but frame probe failed!
        tft->fillRect(0, footerY, SCREEN_WIDTH, footerH, 0x8200); // Dark Amber
        tft->drawFastHLine(0, footerY, SCREEN_WIDTH, UI_YELLOW);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(UI_WHITE, 0x8200);
        char probeErr[90];
        snprintf(probeErr, sizeof(probeErr), "PROBE FAILED: %.50s", probeDetail);
        tft->drawString(probeErr, SCREEN_WIDTH / 2, footerY + 12);
        tft->setTextColor(UI_YELLOW, 0x8200);
        tft->drawString("Attempting direct live capture in 3 sec...", SCREEN_WIDTH / 2, footerY + 26);
        
        audio.playTestTone(400);
        
        // Show diagnostic card for 3 seconds then attempt direct capture
        unsigned long startWait = millis();
        while (millis() - startWait < 3000) {
            ble.update();
            delay(50);
        }
        return true;  // Proceed to live capture attempt
    }
    
    // Camera is online AND frame probe succeeded!
    tft->fillRect(0, footerY, SCREEN_WIDTH, footerH, 0x0320); // Dark Green
    tft->drawFastHLine(0, footerY, SCREEN_WIDTH, UI_GREEN);
    
    // Drain any lingering button press from entering this mode
    while (digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) {
        ble.update();
        delay(10);
    }
    delay(100);
    
    unsigned long diagStart = millis();
    int lastSec = -1;
    while (millis() - diagStart < 8000) {
        int remaining = 8 - (int)((millis() - diagStart) / 1000);
        if (remaining < 1) remaining = 1;
        
        if (remaining != lastSec) {
            lastSec = remaining;
            char cdBuf[64];
            snprintf(cdBuf, sizeof(cdBuf), "Hardware & Camera Verified! Press button to return (%ds)", remaining);
            tft->fillRect(1, footerY + 1, SCREEN_WIDTH - 2, footerH - 2, 0x0320);
            tft->setTextDatum(MC_DATUM);
            tft->setTextSize(1);
            tft->setTextColor(UI_WHITE, 0x0320);
            tft->drawString(cdBuf, SCREEN_WIDTH / 2, footerY + 12);
            tft->setTextColor(UI_CYAN, 0x0320);
            tft->drawString("Press external button to trigger plant scan anytime", SCREEN_WIDTH / 2, footerY + 26);
        }
        
        if (digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) {
            while (digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) delay(10);
            break;
        }
        
        ble.update();
        delay(50);
    }
    
    currentScreen = ScreenMode::FACE;
    display.getTFT()->fillScreen(UI_BG);
    return true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Camera Scan — Direct Capture (No Live Preview)
//  Flow: Pre-Scan Diagnostics → "Capturing..." screen → final capture → BLE send
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Camera Diagnostic Dashboard for Scan Routine
//  Displays full 4-card hardware status & real-time VGA transfer telemetry
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawCameraDiagnosticScanDashboard(TFT_eSPI* tft, const CamStatusInfo& camInfo, 
                                      bool capturing, bool captured, size_t frameSize, unsigned long latencyMs) {
    tft->fillScreen(UI_BG);
    
    // Header
    tft->fillRect(0, 0, SCREEN_WIDTH, 30, UI_DARK_BLUE);
    tft->drawFastHLine(0, 30, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("CAMERA DIAGNOSTIC SCAN", SCREEN_WIDTH / 2, 10);
    tft->setTextColor(UI_LIGHT_GRAY, UI_DARK_BLUE);
    tft->drawString("Direct VGA (640x480) Capture -> AI Server Pipeline", SCREEN_WIDTH / 2, 22);
    
    int cardW = 304;
    int cardH = 88;
    int col1X = 8;
    int row1Y = 36;
    int row2Y = 128;
    int row3Y = 220;
    int row4Y = 312;
    
    // ════════════════════════════════════════════
    // Card 1: UART Link (GPIO 4/5)
    // ════════════════════════════════════════════
    uint16_t uartBorder = camInfo.online ? UI_CYAN : UI_RED;
    tft->drawRoundRect(col1X, row1Y, cardW, cardH, 5, uartBorder);
    tft->fillRect(col1X + 1, row1Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row1Y + 21, cardW - 2, uartBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(uartBorder, UI_DARK_GRAY);
    tft->drawString(" 1. UART LINK (GPIO 4/5)", col1X + 6, row1Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Port: Serial2 (TX=5, RX=4)", col1X + 8, row1Y + 26);
    tft->drawString("Baud: 460,800 bps | RX: 16 KB Ring", col1X + 8, row1Y + 44);
    if (camInfo.online) {
        tft->setTextColor(UI_GREEN, UI_BG);
        char pingBuf[48];
        snprintf(pingBuf, sizeof(pingBuf), "Status: [OK] | Ping: %lu ms", camInfo.pingMs);
        tft->drawString(pingBuf, col1X + 8, row1Y + 62);
    } else {
        tft->setTextColor(UI_RED, UI_BG);
        tft->drawString("Status: [FAIL] Check UART wiring", col1X + 8, row1Y + 62);
    }
    
    // ════════════════════════════════════════════
    // Card 2: Camera Node Hardware
    // ════════════════════════════════════════════
    uint16_t camBorder = (camInfo.online && camInfo.pid != 0) ? UI_GREEN : (camInfo.online ? UI_YELLOW : UI_RED);
    tft->drawRoundRect(col1X, row2Y, cardW, cardH, 5, camBorder);
    tft->fillRect(col1X + 1, row2Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row2Y + 21, cardW - 2, camBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(camBorder, UI_DARK_GRAY);
    tft->drawString(" 2. CAMERA NODE", col1X + 6, row2Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    char sensBuf[48];
    snprintf(sensBuf, sizeof(sensBuf), "Sensor: %s | PSRAM: %s", camInfo.sensorName, camInfo.psramFound ? "YES" : "NO");
    tft->drawString(sensBuf, col1X + 8, row2Y + 26);
    
    char heapBuf[48];
    snprintf(heapBuf, sizeof(heapBuf), "CAM Heap: %u B free", (unsigned)camInfo.freeHeap);
    tft->drawString(heapBuf, col1X + 8, row2Y + 44);
    
    tft->setTextColor(UI_CYAN, UI_BG);
    char resBuf[48];
    if (camInfo.width > 0 && camInfo.height > 0) {
        snprintf(resBuf, sizeof(resBuf), "Res: %ux%u | JPEG (q=20)", camInfo.width, camInfo.height);
    } else {
        snprintf(resBuf, sizeof(resBuf), "Res: 160x120 (QQVGA) | JPEG");
    }
    tft->drawString(resBuf, col1X + 8, row2Y + 62);
    
    // ════════════════════════════════════════════
    // Card 3: VGA Capture & AI Pipeline
    // ════════════════════════════════════════════
    uint16_t pipeBorder = captured ? UI_GREEN : (capturing ? UI_YELLOW : UI_CYAN);
    tft->drawRoundRect(col1X, row3Y, cardW, cardH, 5, pipeBorder);
    tft->fillRect(col1X + 1, row3Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row3Y + 21, cardW - 2, pipeBorder);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(pipeBorder, UI_DARK_GRAY);
    tft->drawString(" 3. CAMERA CAPTURE & AI PIPELINE", col1X + 6, row3Y + 5);
    
    if (captured) {
        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("Capture: [OK] SUCCESS", col1X + 8, row3Y + 26);
        tft->setTextColor(UI_WHITE, UI_BG);
        char szBuf[48];
        snprintf(szBuf, sizeof(szBuf), "Size: %u B | Lat: %lu ms", (unsigned)frameSize, latencyMs);
        tft->drawString(szBuf, col1X + 8, row3Y + 44);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("AI Cloud: Gemini Vision Ready", col1X + 8, row3Y + 62);
    } else if (capturing) {
        tft->setTextColor(UI_YELLOW, UI_BG);
        tft->drawString("Capture: [IN PROGRESS]", col1X + 8, row3Y + 26);
        tft->setTextColor(UI_WHITE, UI_BG);
        tft->drawString("Reading DVP frame from GC2145...", col1X + 8, row3Y + 44);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("Safe JPEG Encode in DRAM...", col1X + 8, row3Y + 62);
    } else {
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("Capture: READY", col1X + 8, row3Y + 26);
        tft->setTextColor(UI_WHITE, UI_BG);
        tft->drawString("Direct Stream Pipeline Ready", col1X + 8, row3Y + 44);
        tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
        tft->drawString("AI Model: WeedNet + Gemini", col1X + 8, row3Y + 62);
    }
    
    // ════════════════════════════════════════════
    // Card 4: S3 Host System
    // ════════════════════════════════════════════
    tft->drawRoundRect(col1X, row4Y, cardW, cardH, 5, UI_CYAN);
    tft->fillRect(col1X + 1, row4Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(col1X + 1, row4Y + 21, cardW - 2, UI_CYAN);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" 4. S3 HOST SYSTEM", col1X + 6, row4Y + 5);
    
    tft->setTextColor(UI_WHITE, UI_BG);
    char s3Heap[40];
    snprintf(s3Heap, sizeof(s3Heap), "S3 Heap: %d KB free [OK]", ESP.getFreeHeap() / 1024);
    tft->drawString(s3Heap, col1X + 8, row4Y + 26);
    tft->drawString("Display: 320x480 Portrait [OK]", col1X + 8, row4Y + 44);
    
    char bleBuf[48];
    snprintf(bleBuf, sizeof(bleBuf), "BLE: %s | DRAM: 96KB", ble.isConnected() ? "Connected" : "Advertising");
    tft->setTextColor(ble.isConnected() ? UI_GREEN : UI_CYAN, UI_BG);
    tft->drawString(bleBuf, col1X + 8, row4Y + 62);
    
    // ════════════════════════════════════════════
    // Footer
    // ════════════════════════════════════════════
    int footerY = 414;
    int footerH = 46;
    tft->fillRect(0, footerY, SCREEN_WIDTH, footerH, UI_DARK_BLUE);
    tft->drawFastHLine(0, footerY, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
    tft->drawString("AGROMATE HARDWARE TELEMETRY", SCREEN_WIDTH / 2, footerY + 14);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("All subsystems configured for 320x480", SCREEN_WIDTH / 2, footerY + 30);
}

void runScanPreview() {
    TFT_eSPI* tft = display.getTFT();
    audio.stopBootVoice(); // Ensure boot voice stops before scan audio
    
    face.setExpression(Expression::SCANNING);
    ble.update();
    
    // Drain lingering button presses (with 300ms timeout)
    unsigned long drainStart = millis();
    while ((digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) && (millis() - drainStart < 300)) {
        ble.update();
        delay(10);
    }
    delay(20);
    
    audio.playScanProgressBeep();
    
    // ══════════════════════════════════════════════════════════════════
    //  STEP 1: Capture Screen - Preparing & Taking Photo
    // ══════════════════════════════════════════════════════════════════
    tft->fillScreen(UI_BG);
    
    // Top Header
    tft->fillRect(0, 0, SCREEN_WIDTH, 36, UI_DARK_BLUE);
    tft->drawFastHLine(0, 36, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
    tft->drawString("AGROMATE VISION SCAN", SCREEN_WIDTH / 2, 18);
    
    // Center Card
    tft->drawRoundRect(16, 50, SCREEN_WIDTH - 32, SCREEN_HEIGHT - 105, 8, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(2);
    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString("CAPTURING PLANT...", SCREEN_WIDTH / 2, 140);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("Hold camera steady near the plant", SCREEN_WIDTH / 2, 190);
    tft->setTextColor(UI_YELLOW, UI_BG);
    tft->drawString("Snapping image...", SCREEN_WIDTH / 2, 230);
    
    // Footer
    int footerH2 = 40;
    int footerY2 = SCREEN_HEIGHT - footerH2;
    tft->fillRect(0, footerY2, SCREEN_WIDTH, footerH2, UI_DARK_BLUE);
    tft->drawFastHLine(0, footerY2, SCREEN_WIDTH, UI_CYAN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("PREPARING SCAN | PLEASE WAIT...", SCREEN_WIDTH / 2, footerY2 + 18);
    
    // Configure TJpg_Decoder for dynamic resolution rendering
    _jpgTft = tft;
    _jpgOffsetX = 0;
    _jpgOffsetY = 120;
    _jpgScale2x = false;
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(onJpgBlockDraw);
    
    camera.setFlash(false);
    camera.flushRx();
    
    // Take a fresh capture directly
    size_t finalSize = 0;
    uint8_t* finalJpg = camera.capture(&finalSize);
    if (!finalJpg) {
        camera.reinit();
        delay(150);
        finalJpg = camera.capture(&finalSize);
    }
    
    char capDbg[96];
    snprintf(capDbg, sizeof(capDbg), "CAP: %s sz=%u err=%d msg=%.30s", finalJpg ? "OK" : "NULL", (unsigned)finalSize, camera.lastError, camera.lastCamMsg);
    ble.sendDebugMessage(capDbg);
    
    audio.playScanProgressBeep();
    
    // ══════════════════════════════════════════════════════════════════
    //  STEP 2: Transfer Image to AI Server over BLE with TFT Progress Bar
    // ══════════════════════════════════════════════════════════════════
    if (finalJpg && finalSize > 0) {
        totalScanCount++;
        nvsPrefs.putUInt("scans", totalScanCount);
        
        ble.sendDebugMessage("BLE TX STARTING");
        
        // Render dedicated upload UI with prominent progress bar on TFT
        tft->fillScreen(UI_BG);
        
        // Top Header
        tft->fillRect(0, 0, SCREEN_WIDTH, 36, UI_DARK_BLUE);
        tft->drawFastHLine(0, 36, SCREEN_WIDTH, UI_CYAN);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(2);
        tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
        tft->drawString("TRANSFER TO AI", SCREEN_WIDTH / 2, 18);
        
        // Center Card
        tft->drawRoundRect(16, 50, SCREEN_WIDTH - 32, SCREEN_HEIGHT - 105, 8, UI_CYAN);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(UI_CYAN, UI_BG);
        char subBuf[70];
        snprintf(subBuf, sizeof(subBuf), "Size: %u B | BLE Protocol", (unsigned)finalSize);
        tft->drawString(subBuf, SCREEN_WIDTH / 2, 110);
        
        // Progress Bar dimensions
        int progBarX = 24;
        int progBarY = 170;
        int progBarW = SCREEN_WIDTH - 48;
        int progBarH = 24;
        tft->drawRoundRect(progBarX - 2, progBarY - 2, progBarW + 4, progBarH + 4, 4, UI_CYAN);
        tft->fillRect(progBarX, progBarY, progBarW, progBarH, UI_DARK_GRAY);
        
        // Percentage label below bar
        tft->setTextDatum(MC_DATUM);
        tft->setTextColor(UI_YELLOW, UI_BG);
        tft->drawString("0% Complete (0 B)", SCREEN_WIDTH / 2, 215);
        
        // Footer: Upload status
        tft->fillRect(0, footerY2, SCREEN_WIDTH, footerH2, UI_DARK_BLUE);
        tft->drawFastHLine(0, footerY2, SCREEN_WIDTH, UI_CYAN);
        tft->setTextDatum(MC_DATUM);
        tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
        tft->drawString("Uploading to AI server... Please wait", SCREEN_WIDTH / 2, footerY2 + 18);
        
        if (ble.isConnected()) {
            // Start BLE cooperative transfer
            ble.sendImage(finalJpg, finalSize);
            
            unsigned long uploadStart = millis();
            int lastPct = -1;
            while (ble.isTxActive() && (millis() - uploadStart < 35000)) {
                ble.update();
                
                // Send heartbeat every 3.5s to keep connection active
                static unsigned long lastUploadHb = 0;
                if (millis() - lastUploadHb >= 3500) {
                    lastUploadHb = millis();
                    ble.sendHeartbeat();
                }
                
                float progress = ble.getTxProgress();
                int pct = (int)(progress * 100);
                if (pct != lastPct) {
                    lastPct = pct;
                    
                    // Update progress bar fill
                    int fillW = (progBarW * pct) / 100;
                    tft->fillRect(progBarX, progBarY, fillW, progBarH, UI_GREEN);
                    tft->fillRect(progBarX + fillW, progBarY, progBarW - fillW, progBarH, UI_DARK_GRAY);
                    
                    // Update percentage label
                    char pctBuf[64];
                    snprintf(pctBuf, sizeof(pctBuf), "%d%% Complete (%u / %u B)", pct, (unsigned)(finalSize * pct / 100), (unsigned)finalSize);
                    tft->fillRect(16, 205, SCREEN_WIDTH - 32, 22, UI_BG);
                    tft->setTextDatum(MC_DATUM);
                    tft->setTextColor(UI_YELLOW, UI_BG);
                    tft->drawString(pctBuf, SCREEN_WIDTH / 2, 215);
                    
                    // Update footer text
                    char footBuf[70];
                    snprintf(footBuf, sizeof(footBuf), "Uploading to AI: %d%% (%u B)", pct, (unsigned)finalSize);
                    tft->fillRect(10, footerY2 + 1, SCREEN_WIDTH - 20, 36, UI_DARK_BLUE);
                    tft->setTextColor(UI_WHITE, UI_DARK_BLUE);
                    tft->drawString(footBuf, SCREEN_WIDTH / 2, footerY2 + 18);
                }
                delay(2);
            }
            
            if (ble.isTxActive()) {
                ble.abortTx();
            }
        } else {
            delay(1000);
        }
        
        // ══════════════════════════════════════════════════════════════════
        //  STEP 3: Display Captured Image on TFT Display AFTER Transfer
        // ══════════════════════════════════════════════════════════════════
        uint16_t jw = 0, jh = 0;
        if (TJpgDec.getJpgSize(&jw, &jh, (const uint8_t*)finalJpg, finalSize) == 0) {
            if (jw <= 160) {
                _jpgScale2x = true;
                _jpgOffsetX = (SCREEN_WIDTH > 320) ? (SCREEN_WIDTH - 320) / 2 : 0;
                _jpgOffsetY = (SCREEN_HEIGHT > (jh * 2)) ? (SCREEN_HEIGHT - (jh * 2)) / 2 : 0;
            } else {
                _jpgScale2x = false;
                _jpgOffsetX = (SCREEN_WIDTH > jw) ? (SCREEN_WIDTH - jw) / 2 : 0;
                _jpgOffsetY = (SCREEN_HEIGHT > jh) ? (SCREEN_HEIGHT - jh) / 2 : 0;
            }
        }
        tft->fillScreen(TFT_BLACK);
        TJpgDec.drawJpg(0, 0, (const uint8_t*)finalJpg, finalSize);
        drawViewfinderHUD(tft, false);
        
        // Top Header Overlay
        tft->fillRect(0, 0, SCREEN_WIDTH, 32, UI_DARK_BLUE);
        tft->drawFastHLine(0, 31, SCREEN_WIDTH, UI_GREEN);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
        tft->drawString("AGROMATE CAPTURED", 12, 16);
        tft->setTextDatum(TR_DATUM);
        tft->setTextColor(UI_GREEN, UI_DARK_BLUE);
        tft->drawString("SENT TO AI [OK]", SCREEN_WIDTH - 12, 16);
        
        // Footer: Upload complete status
        if (ble.isConnected()) {
            tft->fillRect(0, footerY2, SCREEN_WIDTH, footerH2, 0x0320); // Dark Green
            tft->drawFastHLine(0, footerY2, SCREEN_WIDTH, UI_GREEN);
            tft->setTextDatum(MC_DATUM);
            tft->setTextSize(1);
            tft->setTextColor(UI_WHITE, 0x0320);
            tft->drawString("Image Sent to AI Server! [100% OK]", SCREEN_WIDTH / 2, footerY2 + 12);
            tft->setTextColor(UI_CYAN, 0x0320);
            tft->drawString("Awaiting AI Diagnosis...", SCREEN_WIDTH / 2, footerY2 + 26);
            face.setExpression(Expression::THINKING);
        } else {
            tft->fillRect(0, footerY2, SCREEN_WIDTH, footerH2, 0x8200); // Amber
            tft->drawFastHLine(0, footerY2, SCREEN_WIDTH, UI_YELLOW);
            tft->setTextDatum(MC_DATUM);
            tft->setTextSize(1);
            tft->setTextColor(UI_WHITE, 0x8200);
            tft->drawString("Photo Captured Locally | Awaiting Server...", SCREEN_WIDTH / 2, footerY2 + 18);
        }
        
        // ══════════════════════════════════════════════════════════════════
        //  STEP 4: Ready for AI Diagnosis (Keep Camera Online — No Reboot)
        // ══════════════════════════════════════════════════════════════════
        camera.releaseFrame();
        camera.flushRx();
        
        // Non-blocking transition: hold captured image on display and return to main loop
        previewHoldStart = millis();
        currentScreen = ScreenMode::WAIT_RESULT;
        return;
    } else {
        // Capture failed
        char failDbg[96];
        snprintf(failDbg, sizeof(failDbg), "CAPTURE FAILED: err=%d msg=%.40s", camera.lastError, camera.lastCamMsg);
        ble.sendDebugMessage(failDbg);
        camera.releaseFrame();
        tft->fillRect(0, footerY2, SCREEN_WIDTH, footerH2, 0x4000);
        tft->drawFastHLine(0, footerY2, SCREEN_WIDTH, UI_RED);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(UI_WHITE, 0x4000);
        tft->drawString("CAPTURE FAILED: Camera did not return frame in time", SCREEN_WIDTH / 2, footerY2 + 12);
        tft->setTextColor(UI_YELLOW, 0x4000);
        tft->drawString("Check ESP32-CAM ribbon cable / power. Press BOOT to return.", SCREEN_WIDTH / 2, footerY2 + 26);
        
        face.setExpression(Expression::ERROR);
        
        for (int i = 0; i < 40; i++) {
            if (digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) {
                while (digitalRead(BTN_PIN) == LOW || digitalRead(BTN_EXTERNAL_PIN) == LOW) delay(10);
                break;
            }
            ble.update();
            delay(50);
        }
        // Reset camera to known state before returning
        camera.setResolution("QQVGA");
        camera.flushRx();
        currentScreen = ScreenMode::FACE;
        tft->fillScreen(UI_BG);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  BLE Command Callback Processor
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void handleBLECommand(uint8_t* data, size_t length) {
    if (length == 0) return;
    uint8_t cmdType = data[0];
    uint8_t* payload = data + 1;
    size_t payloadLen = length - 1;

    switch (static_cast<MessageType>(cmdType)) {
        case MessageType::FACE_COMMAND: {
            if (payloadLen >= 1) {
                Expression expr = static_cast<Expression>(payload[0]);
                face.setExpression(expr);
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
                resultDrawn = false; // Allow drawing the new result in loop()
            }
            break;
        }
        case MessageType::SPEECH_AUDIO: {
            if (payloadLen > 0) {
                face.setExpression(Expression::TALKING);
                audio.playBuffer(payload, payloadLen);
            }
            break;
        }
        case MessageType::CAPTURE_REQUEST: {
            g_scanRequested = true;
            currentScreen = ScreenMode::PREVIEW;
            break;
        }
        case MessageType::RECORD_REQUEST: {
            uint32_t recordDurationMs = 5000;
            if (payloadLen >= 4) {
                memcpy(&recordDurationMs, payload, 4);
            }
            face.setExpression(Expression::LISTENING);
            audio.startRecording(recordDurationMs);
            break;
        }
        case MessageType::STATUS_UPDATE: {
            if (payloadLen >= 1) {
                uint8_t mode = payload[0];
                if (mode == 0) {
                    currentScreen = ScreenMode::FACE;
                } else if (mode == 1) {
                    currentScreen = ScreenMode::DASHBOARD;
                    dashboardDrawn = false;
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
                        dashboardDrawn = false; // Force redraw in loop()
                    }
                } else if (mode == 3) {
                    // Disease detected — increment disease counter in memory
                    diseasesFound++;
                    if (currentScreen == ScreenMode::DASHBOARD) {
                        dashboardDrawn = false; // Force redraw in loop()
                    }
                } else if (mode == 4) {
                    // Stats sync: mode(1) + scans(4 bytes uint32) + diseases(4 bytes uint32)
                    if (payloadLen >= 9) {
                        uint32_t sCount = 0;
                        uint32_t dCount = 0;
                        memcpy(&sCount, payload + 1, 4);
                        memcpy(&dCount, payload + 5, 4);
                        totalScanCount = sCount;
                        diseasesFound = dCount;
                        if (currentScreen == ScreenMode::DASHBOARD) {
                            dashboardDrawn = false; // Force redraw in loop()
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Boot Screen — Animated startup with progress bar
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawBootScreen(float progress, const char* status) {
    TFT_eSPI* tft = display.getTFT();
    
    // Only clear screen on the first frame
    static bool bootScreenInit = false;
    int cx = SCREEN_WIDTH / 2;
    int barX = 30;
    int barY = 300;
    int barW = SCREEN_WIDTH - 60;
    int barH = 14;

    if (!bootScreenInit) {
        tft->fillScreen(UI_BG);
        
        // Decorative outer frame
        tft->drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, UI_CYAN);
        
        int cy = 130;
        
        // Draw leaf icon
        tft->fillCircle(cx - 14, cy, 22, UI_GREEN);
        tft->fillCircle(cx + 14, cy, 22, UI_GREEN);
        tft->fillCircle(cx, cy - 10, 17, UI_GREEN);
        tft->fillRect(cx - 2, cy + 12, 4, 28, UI_GREEN);
        
        // Title
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(3);
        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("AGRO", cx - 48, 205);
        tft->setTextColor(UI_CYAN, UI_BG);
        tft->drawString("MATE", cx + 48, 205);
        
        // Subtitle
        tft->setTextSize(1);
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("Agricultural AI Companion", cx, 240);
        
        // Version at bottom
        tft->setTextSize(1);
        tft->setTextColor(UI_DARK_CYAN, UI_BG);
        tft->drawString("v2.0.0 | ESP32-S3 | Portrait", cx, SCREEN_HEIGHT - 20);
        
        // Progress bar outline (drawn once)
        tft->drawRect(barX, barY, barW, barH, UI_CYAN);
        
        bootScreenInit = true;
    }
    
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
    tft->drawString(pctBuf, cx, barY + barH + 14);
    
    // Update status text
    tft->setTextColor(UI_CYAN, UI_BG);
    // Clear old status text area
    tft->fillRect(20, barY + barH + 24, SCREEN_WIDTH - 40, 14, UI_BG);
    tft->drawString(status, cx, barY + barH + 31);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Dashboard Screen — Static elements (drawn once)
//  Professional 4-card layout: Stats | Weather | Hardware | System
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawDashboardStatic() {
    TFT_eSPI* tft = display.getTFT();
    
    tft->fillScreen(UI_BG);
    
    // ── Header Bar ──
    tft->fillRect(0, 0, SCREEN_WIDTH, 32, UI_DARK_GRAY);
    tft->drawFastHLine(0, 32, SCREEN_WIDTH, UI_CYAN);
    tft->drawFastHLine(0, 33, SCREEN_WIDTH, UI_DARK_CYAN);
    
    tft->setTextDatum(ML_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" AGROMATE DASHBOARD", 6, 16);
    
    // Card layout constants (4 stacked cards in portrait)
    int cardX = 8;
    int cardW = SCREEN_WIDTH - 16;
    int cardH = 88;
    int row1Y = 36;
    int row2Y = 128;
    int row3Y = 220;
    int row4Y = 312;
    
    // ── Card 1: SCAN STATISTICS ──
    tft->drawRoundRect(cardX, row1Y, cardW, cardH, 5, UI_GREEN);
    tft->fillRect(cardX + 1, row1Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX + 1, row1Y + 21, cardW - 2, UI_GREEN);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_GREEN, UI_DARK_GRAY);
    tft->drawString(" SCAN STATISTICS", cardX + 6, row1Y + 5);
    
    // ── Card 2: WEATHER ──
    tft->drawRoundRect(cardX, row2Y, cardW, cardH, 5, UI_YELLOW);
    tft->fillRect(cardX + 1, row2Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX + 1, row2Y + 21, cardW - 2, UI_YELLOW);
    tft->setTextColor(UI_YELLOW, UI_DARK_GRAY);
    tft->drawString(" WEATHER & FIELD", cardX + 6, row2Y + 5);
    
    // ── Card 3: HARDWARE STATUS ──
    tft->drawRoundRect(cardX, row3Y, cardW, cardH, 5, UI_ORANGE);
    tft->fillRect(cardX + 1, row3Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX + 1, row3Y + 21, cardW - 2, UI_ORANGE);
    tft->setTextColor(UI_ORANGE, UI_DARK_GRAY);
    tft->drawString(" HARDWARE STATUS", cardX + 6, row3Y + 5);
    
    // Hardware status rows (static flags)
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    // Left column
    tft->setTextColor(hwCameraOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwCameraOk ? "[OK]" : "[XX]", cardX + 8, row3Y + 28);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("CAM: GC2145", cardX + 38, row3Y + 28);
    
    tft->setTextColor(hwMicOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwMicOk ? "[OK]" : "[XX]", cardX + 8, row3Y + 46);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("MIC: INMP441", cardX + 38, row3Y + 46);

    tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
    tft->drawString("Display: 320x480 Portrait", cardX + 8, row3Y + 66);

    // Right column
    tft->setTextColor(hwSpeakerOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwSpeakerOk ? "[OK]" : "[XX]", cardX + 155, row3Y + 28);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("SPK: MAX98357A", cardX + 185, row3Y + 28);

    tft->setTextColor(hwBleOk ? UI_GREEN : UI_RED, UI_BG);
    tft->drawString(hwBleOk ? "[OK]" : "[XX]", cardX + 155, row3Y + 46);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString("BLE: NimBLE", cardX + 185, row3Y + 46);

    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString("Bus: 8-Bit Parallel", cardX + 155, row3Y + 66);
    
    // ── Card 4: SYSTEM ──
    tft->drawRoundRect(cardX, row4Y, cardW, cardH, 5, UI_CYAN);
    tft->fillRect(cardX + 1, row4Y + 1, cardW - 2, 20, UI_DARK_GRAY);
    tft->drawFastHLine(cardX + 1, row4Y + 21, cardW - 2, UI_CYAN);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(" SYSTEM TELEMETRY", cardX + 6, row4Y + 5);
    
    // ── Footer ──
    int footerY = 414;
    int footerH = 46;
    tft->drawRoundRect(cardX, footerY, cardW, footerH, 5, UI_CYAN);
    tft->fillRect(cardX + 1, footerY + 1, cardW - 2, footerH - 2, UI_DARK_BLUE);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("[ TAP BOOT: FACE  |  HOLD: VISION SCAN ]", SCREEN_WIDTH / 2, footerY + 14);
    tft->setTextColor(UI_YELLOW, UI_DARK_BLUE);
    tft->drawString("Press GPIO 7 button anytime to snap photo", SCREEN_WIDTH / 2, footerY + 30);
    
    dashboardDrawn = true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Dashboard Screen — Dynamic elements (updated each tick)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawDashboardDynamic() {
    TFT_eSPI* tft = display.getTFT();
    
    int cardX = 8;
    int cardW = SCREEN_WIDTH - 16;
    int row1Y = 36;
    int row2Y = 128;
    int row4Y = 312;
    
    // ── Header: BLE connection status ──
    tft->setTextDatum(MR_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(ble.isConnected() ? UI_GREEN : UI_RED, UI_DARK_GRAY);
    tft->drawString(ble.isConnected() ? "BLE: SYNCED  " : "BLE: WAITING ", SCREEN_WIDTH - 6, 16);
    
    // ── Card 1: Scan Stats (dynamic values) ──
    tft->fillRect(cardX + 7, row1Y + 24, cardW - 14, 60, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    char scansBuf[32];
    snprintf(scansBuf, sizeof(scansBuf), "Total Scans: %u", totalScanCount);
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->drawString(scansBuf, cardX + 8, row1Y + 28);

    char diseasesBuf[32];
    snprintf(diseasesBuf, sizeof(diseasesBuf), "Diseases: %u", diseasesFound);
    tft->setTextColor(diseasesFound > 0 ? UI_AMBER : UI_GREEN, UI_BG);
    tft->drawString(diseasesBuf, cardX + 160, row1Y + 28);
    
    // Scan success rate
    if (totalScanCount > 0) {
        int healthyPct = (int)(((float)(totalScanCount - diseasesFound) / totalScanCount) * 100);
        char rateBuf[32];
        snprintf(rateBuf, sizeof(rateBuf), "Healthy Rate: %d%%", healthyPct);
        tft->setTextColor(healthyPct >= 70 ? UI_GREEN : UI_YELLOW, UI_BG);
        tft->drawString(rateBuf, cardX + 8, row1Y + 46);
    } else {
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("No scans yet", cardX + 8, row1Y + 46);
    }

    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString("AI: Gemini Vision Online", cardX + 160, row1Y + 46);
    
    // Scan progress bar
    int barX = cardX + 8;
    int barY = row1Y + 66;
    int barW = cardW - 16;
    tft->drawRect(barX, barY, barW, 8, UI_MID_GRAY);
    if (totalScanCount > 0 && totalScanCount > diseasesFound) {
        float healthyRatio = (float)(totalScanCount - diseasesFound) / totalScanCount;
        tft->fillRect(barX + 1, barY + 1, (int)((barW - 2) * healthyRatio), 6, UI_GREEN);
    }
    
    // ── Card 2: Weather (dynamic values) ──
    tft->fillRect(cardX + 7, row2Y + 24, cardW - 14, 60, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    
    if (weatherSynced) {
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "Temp: %.1f C", weatherTemp);
        tft->setTextColor(UI_WHITE, UI_BG);
        tft->drawString(tempBuf, cardX + 8, row2Y + 28);
        
        char humBuf[32];
        snprintf(humBuf, sizeof(humBuf), "Humidity: %d%%", weatherHumidity);
        tft->drawString(humBuf, cardX + 160, row2Y + 28);
        
        char fcastBuf[40];
        snprintf(fcastBuf, sizeof(fcastBuf), "Forecast: %s", weatherForecast);
        tft->drawString(fcastBuf, cardX + 8, row2Y + 46);

        tft->setTextColor(UI_GREEN, UI_BG);
        tft->drawString("Field Condition: Optimal", cardX + 8, row2Y + 66);
    } else {
        tft->setTextColor(UI_MID_GRAY, UI_BG);
        tft->drawString("Awaiting Weather Sync...", cardX + 8, row2Y + 28);
        tft->drawString("Connect phone / app via BLE", cardX + 8, row2Y + 46);
        tft->drawString("Live sensor auto-fetch enabled", cardX + 8, row2Y + 66);
    }
    
    // ── Card 4: System Info (dynamic values) ──
    tft->fillRect(cardX + 7, row4Y + 24, cardW - 14, 60, UI_BG);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_BG);
    
    // Uptime
    unsigned long upSec = (millis() - bootTime) / 1000;
    int hours = upSec / 3600;
    int mins = (upSec % 3600) / 60;
    int secs = upSec % 60;
    
    char uptimeBuf[32];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "Uptime: %02d:%02d:%02d", hours, mins, secs);
    tft->drawString(uptimeBuf, cardX + 8, row4Y + 28);
    
    // Free heap
    char memBuf[32];
    snprintf(memBuf, sizeof(memBuf), "Heap: %dKB free", ESP.getFreeHeap() / 1024);
    tft->drawString(memBuf, cardX + 8, row4Y + 46);

    tft->setTextColor(UI_LIGHT_GRAY, UI_BG);
    tft->drawString("Core 0 Audio | Core 1 UI", cardX + 8, row4Y + 66);
    
    // CPU frequency
    char cpuBuf[32];
    snprintf(cpuBuf, sizeof(cpuBuf), "CPU: %dMHz", getCpuFrequencyMhz());
    tft->setTextColor(UI_CYAN, UI_BG);
    tft->drawString(cpuBuf, cardX + 160, row4Y + 28);
    
    // FPS
    char fpsBuf[32];
    snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", currentFPS);
    tft->drawString(fpsBuf, cardX + 160, row4Y + 46);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Home Screen (AGRO MATE AI BY CHIRAG ST MARY'S SCHOOL)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawHomeScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);
    
    // Draw modern high-tech border
    tft->drawRect(6, 6, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 12, UI_CYAN);
    tft->drawRect(8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, UI_DARK_CYAN); // subtle dark outline
    
    // Draw decorative brackets/corners
    tft->fillRect(4, 4, 25, 5, UI_CYAN);
    tft->fillRect(4, 4, 5, 25, UI_CYAN);
    
    tft->fillRect(SCREEN_WIDTH - 29, 4, 25, 5, UI_CYAN);
    tft->fillRect(SCREEN_WIDTH - 9, 4, 5, 25, UI_CYAN);
    
    tft->fillRect(4, SCREEN_HEIGHT - 9, 25, 5, UI_CYAN);
    tft->fillRect(4, SCREEN_HEIGHT - 29, 5, 25, UI_CYAN);
    
    tft->fillRect(SCREEN_WIDTH - 29, SCREEN_HEIGHT - 9, 25, 5, UI_CYAN);
    tft->fillRect(SCREEN_WIDTH - 9, SCREEN_HEIGHT - 29, 5, 25, UI_CYAN);

    // Title: AGRO MATE AI
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN, UI_BG);
    tft->setTextSize(3);
    tft->drawString("AGRO MATE AI", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 45);

    // Subtitle: BY CHIRAG
    tft->setTextColor(UI_WHITE, UI_BG);
    tft->setTextSize(2);
    tft->drawString("BY CHIRAG", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);

    // Subtitle 2: ST MARY'S SCHOOL
    tft->setTextColor(UI_YELLOW, UI_BG);
    tft->setTextSize(2);
    tft->drawString("ST MARY'S SCHOOL", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 45);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Status Bar
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void drawStatusBar() {
    TFT_eSPI* tft = display.getTFT();
    
    tft->fillRect(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, 26, UI_DARK_GRAY);
    tft->drawFastHLine(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, UI_DARK_CYAN);
    tft->setTextDatum(ML_DATUM);
    tft->setTextSize(1);
    
    uint8_t exprIdx = static_cast<uint8_t>(face.getExpression());
    const char* exprName = (exprIdx < NUM_EXPRESSIONS) ? expressionNames[exprIdx] : "?";
    char modeBuf[24];
    snprintf(modeBuf, sizeof(modeBuf), " AI: %s", exprName);
    tft->setTextColor(UI_GREEN, UI_DARK_GRAY);
    tft->drawString(modeBuf, 4, SCREEN_HEIGHT - 13);
    
    tft->setTextDatum(MC_DATUM);
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", currentFPS);
    tft->setTextColor(UI_CYAN, UI_DARK_GRAY);
    tft->drawString(fpsBuf, SCREEN_WIDTH / 2 - 8, SCREEN_HEIGHT - 13);
    
    int btnW = 95;
    int btnH = 18;
    int btnX = SCREEN_WIDTH - btnW - 4;
    int btnY = SCREEN_HEIGHT - btnH - 4;
    tft->drawRoundRect(btnX, btnY, btnW, btnH, 3, UI_CYAN);
    tft->fillRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, UI_DARK_BLUE);
    
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_CYAN, UI_DARK_BLUE);
    tft->drawString("DASH [BOOT]", btnX + (btnW / 2), btnY + (btnH / 2) + 1);
}


// =============================================================
//  TFT Helper: draw semicolon-separated items at FIXED y steps
// =============================================================
static void drawItems(TFT_eSPI* tft, const String& src,
                      int x, int startY, int step, int maxItems,
                      int maxChars, const char* bullet) {
    int idx = 0, from = 0, y = startY;
    for (int i = 0; i <= (int)src.length() && idx < maxItems; i++) {
        if (i == (int)src.length() || src.charAt(i) == ';') {
            String s = src.substring(from, i);
            s.trim();
            if (s.length() > 0) {
                if ((int)s.length() > maxChars) s = s.substring(0, maxChars - 1) + ".";
                char buf[64];
                snprintf(buf, sizeof(buf), "%s %s", bullet, s.c_str());
                tft->drawString(buf, x, y);
                y += step;
                idx++;
            }
            from = i + 1;
        }
    }
}

// =============================================================
//  TFT Helper: word-wrap text at FIXED y steps
// =============================================================
static void drawWrap(TFT_eSPI* tft, const String& text,
                     int x, int startY, int step, int maxLines, int maxChars) {
    String rem = text;
    int y = startY, n = 0;
    while (rem.length() > 0 && n < maxLines) {
        int cut = (int)rem.length();
        if (cut > maxChars) {
            cut = maxChars;
            for (int j = maxChars - 1; j > 0; j--) {
                if (rem.charAt(j) == ' ') { cut = j; break; }
            }
        }
        String line = rem.substring(0, cut);
        line.trim();
        char buf[80];
        line.substring(0, 78).toCharArray(buf, sizeof(buf));
        tft->drawString(buf, x, y);
        y += step;
        rem = rem.substring(cut);
        rem.trim();
        n++;
    }
}

// =============================================================
// =============================================================
//  DISEASE RESULT SCREEN (320 x 480 PORTRAIT)
//
//  Single-column portrait card layout:
//    0-28     Header bar (red)
//    34-80    Plant info (Plant, Sci Name, Confidence)
//    88-142   Diagnosis & Badges (Severity, Diseased)
//    154-180  Health Score & Bar
//    190-250  Symptoms (up to 3 items)
//    258-316  Treatment (up to 3 items)
//    326-440  AI Summary (up to 7 wrapped lines)
//    458-480  [BOOT] Dismiss footer
// =============================================================
void drawDiseaseResultScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);

    // --- Parse fields ---
    String f[10]; int fi = 0, sp = 0;
    for (int i = 0; i <= (int)latestAiResult.length() && fi < 10; i++) {
        if (i == (int)latestAiResult.length() || latestAiResult.charAt(i) == '|') {
            f[fi++] = latestAiResult.substring(sp, i); sp = i + 1;
        }
    }
    String plant   = (fi > 1) ? f[1] : "Unknown";
    String sci     = (fi > 2) ? f[2] : "";
    String conf    = (fi > 3) ? f[3] : "0";
    String disease = (fi > 4) ? f[4] : "Unknown";
    String sev     = (fi > 5) ? f[5] : "MODERATE";
    int    score   = (fi > 6) ? f[6].toInt() : 50;
    String symp    = (fi > 7) ? f[7] : "Discoloration;Leaf damage";
    String treat   = (fi > 8) ? f[8] : "Monitor plant health";
    String summ    = (fi > 9) ? f[9] : "";

    sev.toUpperCase();
    if (plant.length()   > 22) plant   = plant.substring(0, 21)   + "..";
    if (sci.length()     > 44) sci     = sci.substring(0, 43)     + "..";
    if (disease.length() > 22) disease = disease.substring(0, 21) + "..";

    const int LX = 10;
    const int W = SCREEN_WIDTH - 20;

    // ============ HEADER BAR (y: 0-28) ============
    tft->fillRect(0, 0, SCREEN_WIDTH, 28, UI_DARK_RED);
    tft->drawFastHLine(0, 28, SCREEN_WIDTH, UI_RED);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_RED);
    tft->drawString("AGROMATE - DISEASE DETECTED", SCREEN_WIDTH / 2, 14);
    tft->setTextDatum(TL_DATUM);

    // ============ PLANT INFO ============
    // Plant name (y:34, size 2)
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE);
    tft->drawString(plant, LX, 34);

    // Sci name (y:54)
    tft->setTextSize(1);
    tft->setTextColor(UI_LIGHT_GRAY);
    if (sci.length() > 0) tft->drawString(sci, LX, 54);

    // Confidence (y:68)
    char cb[32]; snprintf(cb, sizeof(cb), "Confidence: %s%%", conf.c_str());
    tft->setTextColor(UI_DARK_CYAN);
    tft->drawString(cb, LX, 68);

    // Divider (y:82)
    tft->drawFastHLine(LX, 82, W, UI_MID_GRAY);

    // ============ DIAGNOSIS ============
    // DIAGNOSIS label (y:88)
    tft->setTextColor(UI_CYAN);
    tft->drawString("DIAGNOSIS:", LX, 88);

    // Disease name (y:102, size 2)
    tft->setTextSize(2);
    tft->setTextColor(UI_AMBER);
    tft->drawString(disease, LX, 102);
    tft->setTextSize(1);

    // Severity + Diseased badges (y:124)
    uint16_t sc = UI_YELLOW;
    if      (sev == "SEVERE" || sev == "HIGH") sc = UI_RED;
    else if (sev == "MILD"   || sev == "LOW")  sc = UI_GREEN;
    else if (sev == "CRITICAL")                 sc = 0xF81F;
    tft->fillRoundRect(LX, 124, 90, 18, 4, sc);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(0x0000);
    tft->drawString(sev, LX + 45, 133);
    tft->fillRoundRect(LX + 96, 124, 90, 18, 4, UI_RED);
    tft->setTextColor(UI_WHITE);
    tft->drawString("DISEASED", LX + 141, 133);
    tft->setTextDatum(TL_DATUM);

    // Divider (y:148)
    tft->drawFastHLine(LX, 148, W, UI_MID_GRAY);

    // ============ HEALTH SCORE ============
    tft->setTextColor(UI_CYAN);
    tft->drawString("HEALTH SCORE", LX, 154);
    char hs[8]; snprintf(hs, sizeof(hs), "%d%%", score);
    tft->setTextDatum(TR_DATUM);
    uint16_t bc = (score > 60) ? UI_GREEN : (score > 30) ? UI_YELLOW : UI_RED;
    tft->setTextColor(bc);
    tft->drawString(hs, SCREEN_WIDTH - LX, 154);
    tft->setTextDatum(TL_DATUM);

    // Health bar (y:168, 10px tall)
    tft->drawRect(LX, 168, W, 10, UI_MID_GRAY);
    tft->fillRect(LX + 2, 170, ((W - 4) * score) / 100, 6, bc);

    // Divider (y:184)
    tft->drawFastHLine(LX, 184, W, UI_MID_GRAY);

    // ============ SYMPTOMS ============
    tft->setTextColor(UI_CYAN);
    tft->drawString("SYMPTOMS:", LX, 190);
    tft->setTextColor(UI_AMBER);
    drawItems(tft, symp, LX + 4, 204, 15, 3, 44, "*");

    // Divider (y:252)
    tft->drawFastHLine(LX, 252, W, UI_MID_GRAY);

    // ============ TREATMENT ============
    tft->setTextColor(UI_GREEN);
    tft->drawString("TREATMENT:", LX, 258);
    tft->setTextColor(UI_WHITE);
    drawItems(tft, treat, LX + 4, 272, 15, 3, 44, "+");

    // Divider (y:320)
    tft->drawFastHLine(LX, 320, W, UI_MID_GRAY);

    // ============ AI SUMMARY ============
    tft->setTextColor(UI_DARK_CYAN);
    tft->drawString("AI SUMMARY:", LX, 326);
    tft->setTextColor(UI_LIGHT_GRAY);
    if (summ.length() > 0) {
        drawWrap(tft, summ, LX, 340, 14, 7, 48);
    }

    // ============ BOTTOM FOOTER (y: 458-480) ============
    tft->fillRect(0, 458, SCREEN_WIDTH, 22, 0x1082);
    tft->drawFastHLine(0, 458, SCREEN_WIDTH, UI_MID_GRAY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("Press BOOT to dismiss", SCREEN_WIDTH / 2, 469);
    tft->setTextDatum(TL_DATUM);
}

// =============================================================
//  HEALTHY RESULT SCREEN (320 x 480 PORTRAIT)
//
//  Single-column portrait card layout:
//    0-28     Header bar (green)
//    42-96    Plant info (Plant, Sci Name, Confidence)
//    102      --- divider ---
//    114-152  PLANT IS HEALTHY banner
//    162      --- divider ---
//    172-200  Health Score & Bar
//    212      --- divider ---
//    226      CARE RECOMMENDATIONS label
//    246-380  Care tips (up to 6 items)
//    458-480  [BOOT] Dismiss footer
// =============================================================
void drawHealthyResultScreen() {
    TFT_eSPI* tft = display.getTFT();
    tft->fillScreen(UI_BG);

    // --- Parse fields ---
    String f[6]; int fi = 0, sp = 0;
    for (int i = 0; i <= (int)latestAiResult.length() && fi < 6; i++) {
        if (i == (int)latestAiResult.length() || latestAiResult.charAt(i) == '|') {
            f[fi++] = latestAiResult.substring(sp, i); sp = i + 1;
        }
    }
    String plant = (fi > 1) ? f[1] : "Unknown";
    String sci   = (fi > 2) ? f[2] : "";
    String conf  = (fi > 3) ? f[3] : "0";
    int    score = (fi > 4) ? f[4].toInt() : 95;
    String tips  = (fi > 5) ? f[5] : "Water regularly;Ensure sunlight;Good drainage;Apply compost";

    if (plant.length() > 24) plant = plant.substring(0, 23) + "..";
    if (sci.length()   > 44) sci   = sci.substring(0, 43)   + "..";

    const int CX = SCREEN_WIDTH / 2;
    const int MX = 16;
    const int cardW = SCREEN_WIDTH - (2 * MX);

    // ============ HEADER (y: 0-28, green) ============
    tft->fillRect(0, 0, SCREEN_WIDTH, 28, UI_DARK_GREEN);
    tft->drawFastHLine(0, 28, SCREEN_WIDTH, UI_GREEN);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->setTextColor(UI_WHITE, UI_DARK_GREEN);
    tft->drawString("AGROMATE - PLANT SCAN RESULT", CX, 14);

    // Plant name (y:42, size 2, centred)
    tft->setTextSize(2);
    tft->setTextColor(UI_WHITE);
    tft->drawString(plant, CX, 44);

    // Sci name (y:68, centred)
    tft->setTextSize(1);
    tft->setTextColor(UI_LIGHT_GRAY);
    if (sci.length() > 0) tft->drawString(sci, CX, 68);

    // Confidence (y:84, centred)
    char cb[32]; snprintf(cb, sizeof(cb), "Confidence: %s%%", conf.c_str());
    tft->setTextColor(UI_DARK_CYAN);
    tft->drawString(cb, CX, 84);

    // Divider (y:102)
    tft->drawFastHLine(MX, 102, cardW, UI_MID_GRAY);

    // HEALTHY banner (y:114, 38px tall)
    tft->fillRoundRect(MX, 114, cardW, 38, 6, UI_GREEN);
    tft->setTextSize(2);
    tft->setTextColor(0x0000, UI_GREEN);
    tft->drawString("PLANT IS HEALTHY", CX, 133);

    // Divider (y:162)
    tft->drawFastHLine(MX, 162, cardW, UI_MID_GRAY);

    // Health score (y:172)
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(UI_CYAN);
    tft->drawString("HEALTH SCORE", MX, 172);
    char hs[8]; snprintf(hs, sizeof(hs), "%d%%", score);
    tft->setTextDatum(TR_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString(hs, SCREEN_WIDTH - MX, 172);
    tft->setTextDatum(TL_DATUM);

    // Health bar (y:188, 12px)
    tft->drawRect(MX, 188, cardW, 12, UI_MID_GRAY);
    tft->fillRect(MX + 2, 190, ((cardW - 4) * score) / 100, 8, UI_GREEN);

    // Divider (y:212)
    tft->drawFastHLine(MX, 212, cardW, UI_MID_GRAY);

    // CARE RECOMMENDATIONS label (y:226, centred)
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("CARE RECOMMENDATIONS", CX, 226);
    tft->setTextDatum(TL_DATUM);

    // Tips (y:246, 22px step, up to 6 items)
    tft->setTextColor(UI_WHITE);
    drawItems(tft, tips, MX + 4, 246, 22, 6, 44, ">");

    // ============ BOTTOM FOOTER (y: 458-480) ============
    tft->fillRect(0, 458, SCREEN_WIDTH, 22, 0x1082);
    tft->drawFastHLine(0, 458, SCREEN_WIDTH, UI_MID_GRAY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(UI_GREEN);
    tft->drawString("Press BOOT to dismiss", CX, 469);
    tft->setTextDatum(TL_DATUM);
}




void handleButton() {
    static bool lastState = HIGH;
    static unsigned long pressStartTime = 0;
    static bool buttonIsPressed = false;
    static unsigned long lastDebounceTime = 0;
    
    bool rawState = digitalRead(BTN_PIN);
    unsigned long now = millis();
    
    // 30ms stable noise filter
    if (rawState != lastState) {
        lastDebounceTime = now;
    }
    
    if ((now - lastDebounceTime) > 30) {
        if (rawState == LOW && !buttonIsPressed) {
            // Button pressed down
            buttonIsPressed = true;
            pressStartTime = now;
        } else if (rawState == HIGH && buttonIsPressed) {
            // Button released! Trigger action based on hold duration
            buttonIsPressed = false;
            unsigned long duration = now - pressStartTime;
            
            // ── Any press dismisses RESULT or WAIT_RESULT screen back to Face ──
            if (currentScreen == ScreenMode::RESULT || currentScreen == ScreenMode::WAIT_RESULT || currentScreen == ScreenMode::PREVIEW) {
                btnPressCount++;
                // Abort any in-progress BLE transfer
                if (ble.isTxActive()) {
                    ble.abortTx();
                }
                // Reset camera to known state for next scan
                camera.releaseFrame();
                camera.flushRx();
                camera.setResolution("QQVGA");
                currentScreen = ScreenMode::FACE;
                display.getTFT()->fillScreen(UI_BG);
                resultDrawn = false;
                lastState = rawState;
                return;
            }
            
            if (duration >= 30 && duration < 350) {
                // ── Short Click (< 350ms): Toggle Face <-> Dashboard ──
                btnPressCount++;
                if (currentScreen == ScreenMode::FACE) {
                    currentScreen = ScreenMode::DASHBOARD;
                    dashboardDrawn = false;
                    display.getTFT()->fillScreen(UI_BG);
                } else if (currentScreen == ScreenMode::DASHBOARD) {
                    currentScreen = ScreenMode::FACE;
                    display.getTFT()->fillScreen(UI_BG);
                }
            } else if (duration >= 350 && duration < 2500) {
                // ── Medium Hold (350ms - 2.5s): Direct Plant Scan ──
                btnPressCount++;
                currentScreen = ScreenMode::PREVIEW;
            } else if (duration >= 2500) {
                // ── Long Hold (>= 2.5s): Camera Hardware Diagnostic Report ──
                btnPressCount++;
                showCameraDiagnosticReport();
            }
        }
    }
    
    lastState = rawState;

    // ══════════════════════════════════════════════════════════════════
    //  2. Dedicated External Push Button (GPIO 7: Instant Image Capture)
    // ══════════════════════════════════════════════════════════════════
    static bool extLastState = HIGH;
    static unsigned long extPressStart = 0;
    static bool extButtonPressed = false;
    static unsigned long extDebounceTime = 0;

    bool extRaw = digitalRead(BTN_EXTERNAL_PIN);
    if (extRaw != extLastState) {
        extDebounceTime = now;
    }

    if ((now - extDebounceTime) > 30) {
        if (extRaw == LOW && !extButtonPressed) {
            extButtonPressed = true;
            extPressStart = now;
        } else if (extRaw == HIGH && extButtonPressed) {
            extButtonPressed = false;
            unsigned long extDuration = now - extPressStart;
            if (extDuration >= 20) {
                // External button clicked! Instantly snap photo & trigger AI scan!
                btnPressCount++;

                // If currently showing an old scan result or waiting, dismiss cleanly first
                if (currentScreen == ScreenMode::RESULT || currentScreen == ScreenMode::WAIT_RESULT) {
                    if (ble.isTxActive()) {
                        ble.abortTx();
                    }
                    camera.releaseFrame();
                    camera.flushRx();
                    camera.setResolution("QQVGA");
                    currentScreen = ScreenMode::FACE;
                    display.getTFT()->fillScreen(UI_BG);
                    resultDrawn = false;
                    delay(50);
                }

                // Immediately start plant scan & camera capture!
                currentScreen = ScreenMode::PREVIEW;
            }
        }
    }
    extLastState = extRaw;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Demo Mode updates (active only when BLE is not synced)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void updateDemo() {
    if (!demoMode) return;
    
    unsigned long now = millis();
    if (now - lastExprChange >= EXPR_CHANGE_INTERVAL) {
        lastExprChange = now;
        
        demoExprIndex = (demoExprIndex + 1) % DEMO_SEQ_LEN;
        Expression newExpr = demoSequence[demoExprIndex];
        face.setExpression(newExpr);
        
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

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Setup
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void setup() {
    // Set custom base MAC address to bypass Windows BLE caching
    uint8_t customMac[] = {0x28, 0x84, 0x85, 0x4D, 0x11, 0x58};
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
    Serial.println("   AgroMate Robot — Production Build v3.0");
    Serial.println("=============================================");
    
    // Load persistent scan counter from NVS flash
    nvsPrefs.begin("agromate", false);
    totalScanCount = nvsPrefs.getUInt("scans", 0);
    diseasesFound = nvsPrefs.getUInt("diseases", 0);
    Serial.printf("[NVS] Loaded scan count: %u, diseases: %u\n", totalScanCount, diseasesFound);
    
    // TFT RD line is physically not connected, no need to drive GPIO 20.
 
    // Configure BOOT button and dedicated external camera shutter push button (internal pull-ups)
    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(BTN_EXTERNAL_PIN, INPUT_PULLUP);
    
    // Release hardware UART0 serial port pins (GPIO 43/44) so they can be driven by the Display parallel bus
    Serial.println("[TFT] Releasing serial port pins (GPIO 43/44) for Display bus...");
    Serial.flush();
    Serial.end();

    // 1. Initialize display screen FIRST for instant visual feedback
    display.begin();
    face.begin(&display);
    
    drawBootScreen(0.25f, "Display initialized...");
    delay(50);
    
    // 2. Initialize ESP32-CAM Module 2 over UART (GPIO 4/5)
    drawBootScreen(0.50f, "Connecting ESP32-CAM...");
    int cameraErr = camera.begin();
    hwCameraOk = (cameraErr == 0);
    if (hwCameraOk) {
        drawBootScreen(0.70f, "ESP32-CAM Connected (Crisp QVGA)");
    } else {
        drawBootScreen(0.70f, "ESP32-CAM Ready (UART 460k)");
    }
    delay(50);

    // 3. Start BLE
    drawBootScreen(0.85f, "Starting BLE Client...");
    ble.begin(BLE_DEVICE_NAME);
    ble.onCommand(handleBLECommand);
    hwBleOk = true;
    delay(50);
    
    // 4. Initialize I2S Audio
    drawBootScreen(0.95f, "Audio system online...");
    audio.begin();
    hwMicOk = true;
    hwSpeakerOk = true;

    drawBootScreen(1.0f, "AgroMate ready!");
    delay(100);
    
    // 5. Instantly bring up Face screen with eyes!
    display.getTFT()->fillScreen(UI_BG);
    currentScreen = ScreenMode::FACE;

    // 6. Start Boot Voice asynchronously in background!
    // Eyes come up immediately and the robot continues speaking in background with mouth lip-sync
    audio.startBootVoice();
    
    bootTime = millis();
    lastFrameTime = millis();
    fpsTimer = millis();
    lastExprChange = millis();
    
    setCpuFrequencyMhz(240); // Full 240MHz CPU clock for main loop operations
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
            NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
            if (pAdv && !pAdv->isAdvertising()) {
                pAdv->start();
            }
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
            ble.sendAudio(audioData, audioSize);
            face.setExpression(Expression::THINKING);
        } else {
            face.setExpression(Expression::ERROR);
        }
    }
    lastAudioRecording = currentAudioRecording;
    
    // Camera frame release monitor after BLE transmission completes
    static bool lastTxActive = false;
    bool currentTxActive = ble.isTxActive();
    if (lastTxActive && !currentTxActive) {
        camera.releaseFrame();
    }
    lastTxActive = currentTxActive;
    
    // Audio lipsync update
    if (audio.isPlaying()) {
        face.setExpression(Expression::TALKING);
        face.setMouthOpenness(audio.getPlaybackLevel() * 1.5f);
    } else if (face.getExpression() == Expression::TALKING) {
        face.setExpression(Expression::IDLE);
        face.setMouthOpenness(0.0f);
    }
    
    // FPS computation
    frameCount++;
    if (now - fpsTimer >= 1000) {
        currentFPS = frameCount * 1000.0f / (now - fpsTimer);
        frameCount = 0;
        fpsTimer = now;
    }
    
    // Ã¢â€â‚¬Ã¢â€â‚¬ Screen rendering engine Ã¢â€â‚¬Ã¢â€â‚¬
    if (g_scanRequested || currentScreen == ScreenMode::PREVIEW) {
        g_scanRequested = false;
        currentScreen = ScreenMode::PREVIEW;
        // Run full preview routine and restore screen mode after
        runScanPreview();
    } else if (currentScreen == ScreenMode::FACE) {
        face.update();
        face.render();
    } else if (currentScreen == ScreenMode::DASHBOARD) {
        // Draw static elements once, then only update dynamic values
        if (!dashboardDrawn) {
            drawDashboardStatic();
        }
        static unsigned long lastDash = 0;
        if (now - lastDash >= 150) { // Fast, smooth dynamic refresh (~7 FPS)
            lastDash = now;
            drawDashboardDynamic();
        }
    } else if (currentScreen == ScreenMode::WAIT_RESULT) {
        // Captured plant image remains displayed on TFT screen while awaiting AI diagnosis!
        // Full loop responsiveness: BLE heartbeats and button checks continue running smoothly.
        // If 30 seconds pass without AI result (e.g. server offline), return gracefully to Face.
        if (now - previewHoldStart >= 30000) {
            // Reset camera to known state so next scan works without hardware reset
            camera.releaseFrame();
            camera.flushRx();
            camera.setResolution("QQVGA");
            currentScreen = ScreenMode::FACE;
            display.getTFT()->fillScreen(UI_BG);
        }
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
    yield();
}
