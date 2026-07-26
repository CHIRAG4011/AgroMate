#pragma once
#include <Arduino.h>

enum class LEDPattern : uint8_t {
    OFF = 0, SOLID = 1, BLINK = 2, PULSE = 3,
    RAINBOW = 4, CHASE = 5, ALERT = 6, SCANNING = 7
};

class CRGB {
public:
    uint8_t r, g, b;
    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    CRGB(uint32_t val) {
        r = (val >> 16) & 0xFF;
        g = (val >> 8) & 0xFF;
        b = val & 0xFF;
    }
    typedef enum {
        Black = 0,
        Blue = 0x0000FF,
        Green = 0x008000,
        Red = 0xFF0000,
        Cyan = 0x00FFFF,
        Orange = 0xFFA500,
        Yellow = 0xFFFF00
    } HTMLColorCode;
};

class LEDManager {
public:
    void begin(uint8_t pin = 0, uint8_t count = 0, uint8_t brightness = 0) {}
    void update() {}
    void setPattern(LEDPattern pattern, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t speed = 50) {}
    void setPattern(LEDPattern pattern, CRGB color, uint8_t speed = 50) {}
    void setPattern(LEDPattern pattern, CRGB::HTMLColorCode colorCode, uint8_t speed = 50) {}
    void setAll(uint8_t r, uint8_t g, uint8_t b) {}
    void off() {}
};
