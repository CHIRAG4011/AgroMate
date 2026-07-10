#pragma once
/**
 * AgroMate Display Manager — TFT display control
 * 
 * Manages the RM68140 3.5" 320x480 8-bit parallel TFT display.
 * Provides boot screen, AI response overlay, and sprite-based
 * double-buffered rendering for the animated face.
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
public:
    void begin();
    void clear(uint16_t color = TFT_BLACK);
    void showBootScreen(const char* title, const char* status);
    void showAIResponse(String text);
    void cycleInfoScreen();
    
    TFT_eSPI* getTFT() { return &_tft; }
    TFT_eSprite* getSprite() { return nullptr; }
    uint16_t width() { return _tft.width(); }
    uint16_t height() { return _tft.height(); }
    
    void pushSprite();

private:
    TFT_eSPI _tft;
    // TFT_eSprite _sprite = TFT_eSprite(&_tft);
    uint8_t _infoScreenIndex = 0;
};
