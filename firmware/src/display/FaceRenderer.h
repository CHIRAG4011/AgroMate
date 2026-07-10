#pragma once
/**
 * AgroMate Face Renderer — Animated AI face with expressive eyes and mouth
 * 
 * Renders a 320x160 animated face sprite with:
 *   - Smooth eye movement with iris/pupil tracking
 *   - Natural blink animation
 *   - Expression-driven mouth shapes
 *   - Breathing animation (pupil oscillation)
 *   - Expression overlays (thinking dots, scan line, etc.)
 */

#include <Arduino.h>
#include "DisplayManager.h"
#include "../config.h"

// Expression types matching the BLE protocol
enum class Expression : uint8_t {
    IDLE = 0, HAPPY = 1, THINKING = 2, LISTENING = 3,
    TALKING = 4, ERROR = 5, WARNING = 6, SLEEPING = 7,
    SURPRISED = 8, SAD = 9, ANGRY = 10, CURIOUS = 11,
    SCANNING = 12, SUCCESS = 13
};

class FaceRenderer {
public:
    FaceRenderer() = default;
    void begin(DisplayManager* display);
    void update();   // Update animation state
    void render();   // Render frame to sprite buffer
    
    void setExpression(Expression expr, uint8_t intensity = 100);
    void setMouthOpenness(float openness);  // 0.0 - 1.0
    void setEyeTarget(float x, float y);    // Gaze target (-1 to 1)
    Expression getExpression() { return _currentExpr; }

private:
    DisplayManager* _display = nullptr;
    TFT_eSPI* _sprite = nullptr;
    
    // Expression state
    Expression _currentExpr = Expression::IDLE;
    Expression _targetExpr = Expression::IDLE;
    uint8_t _intensity = 100;
    
    // Eye animation state
    float _eyeX = 0.0, _eyeY = 0.0;           // Current eye position
    float _targetEyeX = 0.0, _targetEyeY = 0.0; // Target eye position
    float _leftEyeOpenness = 1.0;               // 0=closed, 1=open
    float _rightEyeOpenness = 1.0;
    float _pupilSize = 1.0;
    uint16_t _eyeColor = 0x07FF;                // Cyan default
    
    // Blink state
    unsigned long _nextBlinkTime = 0;
    bool _isBlinking = false;
    unsigned long _blinkStartTime = 0;
    
    // Mouth state
    float _mouthOpen = 0.0;
    float _mouthTargetOpen = 0.0;
    float _mouthWidth = 1.0;
    float _mouthCurve = 0.0;  // -1=frown, 0=neutral, 1=smile
    
    // Animation timing
    unsigned long _lastUpdate = 0;
    unsigned long _randomMoveTime = 0;
    float _breathPhase = 0.0;
    
    TFT_eSprite* _leftSprite = nullptr;
    TFT_eSprite* _rightSprite = nullptr;
    
    // Drawing methods
    void _drawEye(TFT_eSprite* sprite, float openness, float pupilX, float pupilY);
    void _drawMouth(int16_t cx, int16_t cy);
    void _drawExpressionOverlay();
    void _updateBlink();
    void _updateEyeMovement();
    void _updateBreathing();
    void _applyExpression();
    uint16_t _getExpressionColor();
};
