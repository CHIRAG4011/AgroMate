/**
 * AgroMate ESP32-CAM Module 2 — UART Camera Node Firmware
 * 
 * This firmware runs on the AI-Thinker ESP32-CAM module (ESP32 + OV2640).
 * It acts as a UART-commanded camera node for the main AgroMate ESP32-S3 board.
 * 
 * Protocol:
 *   Command (from main board):  ASCII text terminated by '\n'
 *     "CAPTURE\n"              — Capture a VGA JPEG frame and send it
 *     "PREVIEW\n"              — Capture a QVGA JPEG frame (for TFT preview)
 *     "STREAM\n"               — Start streaming QVGA JPEG frames continuously
 *     "STOP\n"                 — Stop streaming
 *     "RESOLUTION:QVGA\n"     — Set resolution to 320x240
 *     "RESOLUTION:VGA\n"      — Set resolution to 640x480 (default)
 *     "RESOLUTION:SVGA\n"     — Set resolution to 800x600
 *     "PING\n"                — Respond with "PONG\n"
 *     "FLASH:ON\n"            — Turn on LED flash
 *     "FLASH:OFF\n"           — Turn off LED flash
 * 
 *   Response (to main board):
 *     For CAPTURE/PREVIEW: [0xAA][0x55][4-byte size LE][JPEG data][0x55][0xAA]
 *     For STREAM:          Continuous frames, each with the same frame wrapper
 *     For PING:            "PONG\n"
 *     For others:          "OK\n" or "ERROR:msg\n"
 * 
 * Hardware: AI-Thinker ESP32-CAM
 *   - OV2640 camera with hardware JPEG encoder
 *   - 4MB PSRAM for frame buffers
 *   - GPIO 4 = LED Flash
 *   - GPIO 33 = Red LED (active LOW)
 * 
 * Wiring to main ESP32-S3:
 *   ESP32-CAM U0T (TX) → ESP32-S3 GPIO 4 (RX)
 *   ESP32-CAM U0R (RX) → ESP32-S3 GPIO 5 (TX)
 *   GND ↔ GND (common ground)
 *   5V  ↔ 5V  (shared power rail)
 */

#include <Arduino.h>

#include "esp_camera.h"
#include "img_converters.h"
#include "SafeJpegEncoder.h"
#include "esp_task_wdt.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

void setResolution(framesize_t size);
void sendFrame();
void sendPreviewFrame();

// ── AI-Thinker ESP32-CAM Pin Definitions ──
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ── LED Flash ──
#define FLASH_GPIO         4
#define RED_LED_GPIO      33

// ── Communication ──
#define UART_BAUD    460800  // 460.8 Kbps — reliable on breadboard wiring
#define TX_CHUNK_SIZE 1024   // Send JPEG data in 1KB chunks to avoid FIFO overflow

// ── Frame Sync Markers ──
static const uint8_t SYNC_START[] = {0xAA, 0x55};
static const uint8_t SYNC_END[]   = {0x55, 0xAA};

// ── State ──
framesize_t currentFrameSize = FRAMESIZE_QQVGA;  // Default QQVGA (160x120) rock-solid in internal DRAM
bool streaming = false;  // True when in STREAM mode
bool g_is_rgb_sensor = false; // True if sensor lacks hardware JPEG (GC2145/RHYX-M21-45)
bool g_psram_ok = false; // True only if PSRAM passes integrity check
bool cameraReady = false; // True only after initCamera() succeeds — guards loop() commands

// ── Forward declarations ──
bool initCamera();
bool psramIntegrityCheck();
void handleCommand(const String& cmd);
void sendFrame();
void sendPreviewFrame();
void sendJpegWithSync(const uint8_t* buf, size_t len);
void setResolution(framesize_t size);
void blinkLED(int times, int delayMs);
void sendJpegChunked(const uint8_t* buf, size_t len);
void tuneSensorGC2145(sensor_t* s);

#include "esp_task_wdt.h"
#include "soc/rtc_wdt.h"

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
    // 1. Disable brownout detector to prevent voltage-dip resets during camera load
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // 2. Start Serial IMMEDIATELY so camera node is listening to UART commands from the first millisecond
    setCpuFrequencyMhz(240);
    Serial.begin(UART_BAUD);
    delay(50);
    Serial.println("OK:CAMERA_NODE_BOOTING");
    Serial.flush();

    // 3. Configure Red LED (GPIO 33, active LOW)
    pinMode(RED_LED_GPIO, OUTPUT);
    digitalWrite(RED_LED_GPIO, LOW);  // Turn ON during boot

    // Configure Flash LED pin
    pinMode(FLASH_GPIO, OUTPUT);
    digitalWrite(FLASH_GPIO, LOW);

    // Task Watchdog: give 30 seconds timeout so camera init + JPEG encode never triggers panics
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    // Ensure PSRAM is initialized if available
    psramInit();
    g_psram_ok = psramIntegrityCheck();
    Serial.printf("DBG:PSRAM_CHECK=%s FREE=%u\n", g_psram_ok ? "OK" : "BAD", (unsigned)ESP.getFreePsram());
    Serial.flush();
    esp_task_wdt_reset();

    if (initCamera()) {
        cameraReady = true;
        Serial.println("OK:CAMERA_INIT_OK");
        Serial.println("OK:CAMERA_READY");
        digitalWrite(RED_LED_GPIO, LOW); // Solid ON indicates camera node is ready!
    } else {
        cameraReady = false;
        Serial.println("ERROR:CAMERA_INIT_FAILED");
        digitalWrite(RED_LED_GPIO, HIGH);
    }
    Serial.flush();
    esp_task_wdt_reset();
}

// ─────────────────────────────────────────────
//  Main Loop — Listen for UART Commands
// ─────────────────────────────────────────────
void loop() {
    esp_task_wdt_reset();

    if (streaming) {
        // In stream mode: send preview frames continuously
        // Check for STOP command between frames
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            if (cmd == "STOP") {
                streaming = false;
                Serial.println("OK:STREAM_STOPPED");
                Serial.flush();
                return;
            }
        }
        sendPreviewFrame();
        delay(10);  // Small yield between frames
    } else {
        // Normal command mode
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            if (cmd.length() > 0) {
                handleCommand(cmd);
            }
        } else {
            delay(10);
        }
    }
}

// ─────────────────────────────────────────────
//  PSRAM Integrity Check
// ─────────────────────────────────────────────
bool psramIntegrityCheck() {
    if (!psramFound()) return false;
    
    // Allocate 64 KB test block — if this works, PSRAM can hold camera frames
    volatile uint32_t* testBlock = (volatile uint32_t*)ps_malloc(65536);
    if (!testBlock) {
        Serial.println("DBG:PSRAM_ALLOC64K_FAIL");
        return false;
    }
    // Write test patterns across the 64 KB block
    testBlock[0]      = 0xDEADBEEF;
    testBlock[1000]   = 0xCAFEBABE;
    testBlock[4000]   = 0x12345678;
    testBlock[8000]   = 0xABCD1234;
    testBlock[16383]  = 0xFEEDFACE;  // Last uint32 in 64KB
    bool ok = (testBlock[0]      == 0xDEADBEEF &&
               testBlock[1000]   == 0xCAFEBABE &&
               testBlock[4000]   == 0x12345678 &&
               testBlock[8000]   == 0xABCD1234 &&
               testBlock[16383]  == 0xFEEDFACE);
    free((void*)testBlock);
    if (!ok) {
        Serial.println("DBG:PSRAM_DATA_FAIL");
    } else {
        Serial.println("DBG:PSRAM_64K_TEST_OK");
    }
    return ok;
}

// ─────────────────────────────────────────────
//  Sensor Tuning & Register Direct Access for GC2145 (RHYX-M21-45)
// ─────────────────────────────────────────────
// ─────────────────────────────────────────────
//  Sensor Tuning for GC2145 (RHYX-M21-45)
// ─────────────────────────────────────────────
void tuneSensorGC2145(sensor_t* s) {
    if (!s) return;

    // Switch to Page 0
    s->set_reg(s, 0xfe, 0xff, 0x00);

    // 1. Enable Auto White Balance (AWB): Reg 0x82 bit 1 = 1
    s->set_reg(s, 0x82, 0x02, 0x02);

    // 2. Enable Auto Exposure & Auto Gain (AEC/AGC): Reg 0xb6 bit 0 = 1
    s->set_reg(s, 0xb6, 0x01, 0x01);
}

// ─────────────────────────────────────────────
//  Camera Initialization (Native VGA 640x480)
// ─────────────────────────────────────────────
bool initCamera() {
    esp_task_wdt_reset();

    // Hardware reset cycle sensor via PWDN pin (GPIO 32)
    if (PWDN_GPIO_NUM != -1) {
        pinMode(PWDN_GPIO_NUM, OUTPUT);
        digitalWrite(PWDN_GPIO_NUM, HIGH); // Power down
        delay(50);
        digitalWrite(PWDN_GPIO_NUM, LOW);  // Power up
        delay(100);  // Settle time for GC2145
    }
    esp_task_wdt_reset();

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = -1;

    config.pixel_format = PIXFORMAT_YUV422; // Native format for GC2145
    config.frame_size   = currentFrameSize; // Default FRAMESIZE_QQVGA (160x120)
    config.jpeg_quality = 12;
    if (currentFrameSize == FRAMESIZE_QQVGA) {
        config.fb_count     = 2;  // Double buffering for QQVGA: ~76KB in DRAM
        config.grab_mode    = CAMERA_GRAB_LATEST; // Continuous grab: always keeps newest frame!
        config.fb_location = CAMERA_FB_IN_DRAM;
    } else {
        config.fb_count     = 1;
        config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
        config.fb_location = g_psram_ok ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    }

    // Try 10MHz XCLK (verified rock-solid for GC2145 on ESP32)
    config.xclk_freq_hz = 10000000;
    Serial.printf("DBG:TRYING CAMERA_INIT@10MHz (FB in %s, free heap=%u)\n",
                  config.fb_location == CAMERA_FB_IN_PSRAM ? "PSRAM" : "DRAM", (unsigned)ESP.getFreeHeap());
    Serial.flush();
    esp_task_wdt_reset();

    esp_err_t err = esp_camera_init(&config);
    esp_task_wdt_reset();

    if (err != ESP_OK && g_psram_ok && config.fb_location == CAMERA_FB_IN_DRAM) {
        Serial.printf("DBG:DRAM init fail err=0x%x, retrying PSRAM\n", err);
        Serial.flush();
        esp_camera_deinit();
        delay(50);
        esp_task_wdt_reset();
        config.fb_location = CAMERA_FB_IN_PSRAM;
        err = esp_camera_init(&config);
        esp_task_wdt_reset();
    }
    if (err != ESP_OK) {
        Serial.printf("DBG:10MHz_INIT_FAIL err=0x%x, trying 8MHz\n", err);
        Serial.flush();
        esp_camera_deinit();
        delay(50);
        esp_task_wdt_reset();
        config.xclk_freq_hz = 8000000;
        config.fb_location = g_psram_ok ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
        err = esp_camera_init(&config);
        esp_task_wdt_reset();
        if (err != ESP_OK) {
            esp_camera_deinit();
            Serial.printf("DBG:8MHz_INIT_FAIL err=0x%x\n", err);
            Serial.flush();
            return false;
        }
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s && s->id.PID == 0x2145) {
        g_is_rgb_sensor = true;
        tuneSensorGC2145(s);
    }
    esp_task_wdt_reset();

    // Allow sensor PLL, AGC, and VSYNC pipeline to stabilize
    delay(200);
    esp_task_wdt_reset();

    // Drain initial warmup frames to synchronize DMA pipeline (2 frames is sufficient)
    for (int i = 0; i < 2; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
        delay(50);
        esp_task_wdt_reset();
    }

    // Verify camera returns valid frame with retries
    camera_fb_t* testFb = nullptr;
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_task_wdt_reset();
        testFb = esp_camera_fb_get();
        if (testFb && testFb->len > 0) break;
        if (testFb) esp_camera_fb_return(testFb);
        testFb = nullptr;
        delay(100);
    }

    if (testFb && testFb->len > 0) {
        Serial.printf("DBG:CAMERA_READY w=%d h=%d len=%u PID=0x%04X\n",
                      testFb->width, testFb->height, (unsigned)testFb->len, s ? s->id.PID : 0);
        Serial.flush();
        esp_camera_fb_return(testFb);
        return true;
    }
    if (testFb) esp_camera_fb_return(testFb);

    Serial.println("DBG:CAMERA_TEST_FRAME_FAIL");
    Serial.flush();
    return false;
}


// ─────────────────────────────────────────────
//  Command Handler
// ─────────────────────────────────────────────
void handleCommand(const String& cmd) {
    if (cmd.indexOf("PING") >= 0) {
        Serial.println("PONG");
        Serial.flush();
        return;
    }
    if (cmd.indexOf("FLASH:ON") >= 0) {
        digitalWrite(FLASH_GPIO, LOW);
        Serial.println("OK");
        Serial.flush();
        return;
    }
    if (cmd.indexOf("FLASH:OFF") >= 0) {
        digitalWrite(FLASH_GPIO, LOW);
        Serial.println("OK");
        Serial.flush();
        return;
    }
    if (cmd.indexOf("STATUS") >= 0) {
        sensor_t* s = esp_camera_sensor_get();
        if (!s) {
            initCamera();
            s = esp_camera_sensor_get();
        }
        uint16_t pid = s ? s->id.PID : 0;
        int w = 160, h = 120;
        if (currentFrameSize == FRAMESIZE_QQVGA) { w = 160; h = 120; }
        else if (currentFrameSize == FRAMESIZE_QVGA) { w = 320; h = 240; }
        else if (currentFrameSize == FRAMESIZE_VGA) { w = 640; h = 480; }
        else if (currentFrameSize == FRAMESIZE_SVGA) { w = 800; h = 600; }
        Serial.printf("OK:PID:0x%04X:PSRAM:%s:PS_OK:%s:HEAP:%u:RES:%dx%d\n",
            pid,
            psramFound() ? "YES" : "NO",
            g_psram_ok ? "YES" : "BAD",
            (unsigned)ESP.getFreeHeap(),
            w, h);
        Serial.flush();
        return;
    }
    if (cmd.indexOf("REINIT") >= 0) {
        Serial.println("DBG:REINIT_START");
        Serial.flush();
        esp_camera_deinit();
        delay(200);
        esp_task_wdt_reset();
        g_psram_ok = psramIntegrityCheck();
        esp_task_wdt_reset();
        if (initCamera()) {
            cameraReady = true;
            Serial.println("OK:REINIT_OK");
            digitalWrite(RED_LED_GPIO, LOW);
        } else {
            cameraReady = false;
            Serial.println("ERROR:REINIT_FAILED");
            digitalWrite(RED_LED_GPIO, HIGH);
        }
        Serial.flush();
        return;
    }
    if (cmd.indexOf("RESET") >= 0) {
        Serial.println("OK:RESETTING");
        Serial.flush();
        delay(100);
        esp_restart();
    }

    // Guard: reject capture commands if camera never initialized successfully
    if (!cameraReady) {
        if (cmd.indexOf("CAPTURE") >= 0 || cmd.indexOf("PREVIEW") >= 0 || cmd.indexOf("STREAM") >= 0) {
            Serial.println("ERROR:CAM_NOT_INIT");
            Serial.flush();
            return;
        }
    }

    if (cmd.indexOf("CAPTURE") >= 0) {
        sendFrame();
    }
    else if (cmd.indexOf("PREVIEW") >= 0) {
        sendPreviewFrame();
    }
    else if (cmd.indexOf("STREAM") >= 0) {
        streaming = true;
        Serial.println("OK:STREAM_STARTED");
        Serial.flush();
    }
    else if (cmd.indexOf("STOP") >= 0) {
        streaming = false;
        Serial.println("OK:STREAM_STOPPED");
        Serial.flush();
    }
    else if (cmd.indexOf("RESOLUTION:") >= 0) {
        if (cmd.indexOf("QQVGA") >= 0) {
            setResolution(FRAMESIZE_QQVGA);
        } else if (cmd.indexOf("QVGA") >= 0) {
            setResolution(FRAMESIZE_QVGA);
        } else if (cmd.indexOf("SVGA") >= 0) {
            setResolution(FRAMESIZE_SVGA);
        } else {
            setResolution(FRAMESIZE_VGA);
        }
    }
    else {
        Serial.println("ERROR:UNKNOWN_CMD");
        Serial.flush();
    }
}

// ─────────────────────────────────────────────
//  Send JPEG data in direct blocking FIFO chunks
// ─────────────────────────────────────────────
void sendJpegChunked(const uint8_t* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        size_t chunk = min((size_t)256, len - sent);
        Serial.write(buf + sent, chunk);
        sent += chunk;
        esp_task_wdt_reset();
    }
    // Ensure every single byte has physically left the UART FIFO
    Serial.flush();
}

void sendJpegWithSync(const uint8_t* buf, size_t len) {
    uint32_t frameSize = (uint32_t)len;
    Serial.write(SYNC_START, 2);
    Serial.write((const uint8_t*)&frameSize, 4);
    sendJpegChunked(buf, len);
    Serial.write(SYNC_END, 2);
    Serial.flush();
}

struct JpegBufferCtx {
    uint8_t* buf;
    size_t cap;
    size_t size;
    bool overflow;
};

static size_t bufferJpegChunk(void *arg, size_t index, const void *data, size_t len) {
    JpegBufferCtx *ctx = static_cast<JpegBufferCtx *>(arg);
    if (!ctx || !ctx->buf) return 0;
    if (ctx->size + len > ctx->cap) {
        ctx->overflow = true;
        return 0;
    }
    memcpy(ctx->buf + ctx->size, data, len);
    ctx->size += len;
    return len;
}

// ─────────────────────────────────────────────
//  Capture & Send Frame (QQVGA 160x120 / VGA / QVGA)
// ─────────────────────────────────────────────
void sendFrame() {
    // Pulse RED LED off during capture to indicate active capture
    digitalWrite(RED_LED_GPIO, HIGH);
    esp_task_wdt_reset();

    // Discard any stale DMA frame sitting in the queue while camera was idle
    camera_fb_t* stale = esp_camera_fb_get();
    if (stale) {
        esp_camera_fb_return(stale);
    }

    camera_fb_t* fb = nullptr;
    for (int retry = 0; retry < 3; retry++) {
        fb = esp_camera_fb_get();
        if (fb && fb->buf && fb->len > 0) break;
        if (fb) esp_camera_fb_return(fb);
        fb = nullptr;
        delay(20);
    }

    if (!fb || !fb->buf || fb->len == 0) {
        if (fb) esp_camera_fb_return(fb);
        digitalWrite(RED_LED_GPIO, LOW); // Restore ready LED
        Serial.println("ERROR:FB_NULL");
        Serial.flush();
        return;
    }

    // Determine actual image dimensions from buffer metrics
    int width = fb->width;
    int height = fb->height;
    if (fb->len == 38400) {
        width = 160;
        height = 120;
    } else if (fb->len == 153600) {
        width = 320;
        height = 240;
    } else if (fb->len == 614400) {
        width = 640;
        height = 480;
    } else if (fb->len == 960000) {
        width = 800;
        height = 600;
    }
    fb->width = width;
    fb->height = height;

    // If sensor already outputs hardware JPEG (e.g. OV2640), send directly
    if (fb->format == PIXFORMAT_JPEG) {
        delay(5);
        sendJpegWithSync(fb->buf, fb->len);
        esp_camera_fb_return(fb);
        digitalWrite(RED_LED_GPIO, LOW); // Restore ready LED
        return;
    }

    // High-performance, crash-proof software JPEG encoder for GC2145 (YUV422) - Quality 85 for rich color depth
    uint8_t* outJpg = nullptr;
    size_t outLen = 0;
    bool ok = encodeYUV422ToJpeg(fb->buf, width, height, 85, &outJpg, &outLen);

    // Return the raw frame buffer immediately to release DMA buffer
    esp_camera_fb_return(fb);

    if (!ok || !outJpg || outLen == 0) {
        digitalWrite(RED_LED_GPIO, LOW); // Restore ready LED
        Serial.println("ERROR:CONV_FAIL");
        Serial.flush();
        if (outJpg) free(outJpg);
        return;
    }

    // Settle delay to ensure line is completely idle before binary sync
    delay(5);
    sendJpegWithSync(outJpg, outLen);
    free(outJpg);

    digitalWrite(RED_LED_GPIO, LOW); // Restore ready LED (solid ON)
}

// ─────────────────────────────────────────────
//  Send Preview Frame (reuses sendFrame)
// ─────────────────────────────────────────────
void sendPreviewFrame() {
    sendFrame();
}

// ─────────────────────────────────────────────
//  Set Camera Resolution
// ─────────────────────────────────────────────
void setResolution(framesize_t size) {
    if (size == currentFrameSize && esp_camera_sensor_get() != nullptr) {
        Serial.println("OK");
        Serial.flush();
        return;
    }

    currentFrameSize = size;
    esp_camera_deinit();
    delay(100);
    esp_task_wdt_reset();
    if (initCamera()) {
        cameraReady = true;
        Serial.println("OK");
    } else {
        cameraReady = false;
        Serial.println("ERROR:INIT_FAIL");
    }
    Serial.flush();
}

// ─────────────────────────────────────────────
//  LED Helper (Blink active-low red LED)
// ─────────────────────────────────────────────
void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(RED_LED_GPIO, LOW);
        delay(delayMs);
        digitalWrite(RED_LED_GPIO, HIGH);
        delay(delayMs);
    }
}
