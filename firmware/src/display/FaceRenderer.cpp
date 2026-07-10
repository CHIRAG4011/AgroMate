/**
 * AgroMate Face Renderer Implementation — Smooth animated AI face
 * 
 * Renders into a FACE_SPRITE_W x FACE_SPRITE_H sprite (320x160)
 * centered on the 480x320 RM68140 display.
 */

#include "FaceRenderer.h"

// Color palette
#define COLOR_BG        0x0000  // Black
#define COLOR_FACE_BG   0x0841  // Very dark gray
#define COLOR_EYE_WHITE 0xFFFF  // White
#define COLOR_PUPIL     0x0000  // Black
#define COLOR_IRIS_CYAN 0x07FF  // Cyan
#define COLOR_IRIS_GREEN 0x07E0 // Green
#define COLOR_IRIS_RED  0xF800  // Red
#define COLOR_IRIS_YELLOW 0xFFE0
#define COLOR_IRIS_ORANGE 0xFDA0
#define COLOR_MOUTH     0x07FF  // Cyan
#define COLOR_GLOW      0x0410  // Subtle cyan glow

// Eye dimensions — scaled for 320x160 sprite
#define EYE_RADIUS_X    48
#define EYE_RADIUS_Y    40
#define EYE_SPACING     76  // Wider spacing for 320px width
#define EYE_Y_POS       80  // Centered vertically in 160px sprite
#define PUPIL_RADIUS    15
#define IRIS_RADIUS     25

// Mouth dimensions
#define MOUTH_Y_POS     220 // Below eyes on 480x320 screen
#define MOUTH_WIDTH     48
#define MOUTH_HEIGHT    10

void FaceRenderer::begin(DisplayManager* display) {
    _display = display;
    _sprite = display->getTFT();
    
    // Allocate the sprites dynamically
    _leftSprite = new TFT_eSprite(_sprite);
    _rightSprite = new TFT_eSprite(_sprite);
    
    // Create the RAM framebuffers (96x80 pixels, 15.3KB each)
    _leftSprite->setColorDepth(16);
    _leftSprite->createSprite(96, 80);
    _leftSprite->setSwapBytes(true);
    
    _rightSprite->setColorDepth(16);
    _rightSprite->createSprite(96, 80);
    _rightSprite->setSwapBytes(true);
    
    _nextBlinkTime = millis() + random(BLINK_MIN_INTERVAL_MS, BLINK_MAX_INTERVAL_MS);
    _randomMoveTime = millis() + random(2000, 5000);
}

void FaceRenderer::update() {
    unsigned long now = millis();
    float dt = (now - _lastUpdate) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;  // Cap delta time
    _lastUpdate = now;

    _updateBlink();
    _updateEyeMovement();
    _updateBreathing();
    _applyExpression();

    // Smooth mouth animation
    _mouthOpen += (_mouthTargetOpen - _mouthOpen) * 8.0f * dt;

    // Smooth eye position interpolation
    _eyeX += (_targetEyeX - _eyeX) * 5.0f * dt;
    _eyeY += (_targetEyeY - _eyeY) * 5.0f * dt;
}

void FaceRenderer::render() {
    if (!_sprite || !_leftSprite || !_rightSprite) return;

    // Render each eye in its local sprite buffer, then push to display
    _drawEye(_leftSprite, _leftEyeOpenness, _eyeX, _eyeY);
    _leftSprite->pushSprite(240 - EYE_SPACING - 48, 160 - 40);

    _drawEye(_rightSprite, _rightEyeOpenness, _eyeX, _eyeY);
    _rightSprite->pushSprite(240 + EYE_SPACING - 48, 160 - 40);

    // Render animated mouth
    _drawMouth(240, MOUTH_Y_POS);

    _drawExpressionOverlay();
}

void FaceRenderer::setExpression(Expression expr, uint8_t intensity) {
    _targetExpr = expr;
    _intensity = intensity;
    _currentExpr = expr;
    _eyeColor = _getExpressionColor();
}

void FaceRenderer::setMouthOpenness(float openness) {
    _mouthTargetOpen = constrain(openness, 0.0f, 1.0f);
}

void FaceRenderer::setEyeTarget(float x, float y) {
    _targetEyeX = constrain(x, -1.0f, 1.0f);
    _targetEyeY = constrain(y, -1.0f, 1.0f);
}

void FaceRenderer::_drawEye(TFT_eSprite* sprite, float openness, float pupilX, float pupilY) {
    int16_t cx = 48; // Local center inside the 96x80 sprite
    int16_t cy = 40;

    // Clear the sprite framebuffer to black
    sprite->fillSprite(COLOR_BG);

    if (openness < 0.05f) {
        // Closed eye — draw a line
        sprite->drawWideLine(cx - EYE_RADIUS_X, cy, cx + EYE_RADIUS_X, cy, 3, _eyeColor);
        return;
    }

    // Eye shape scaling based on openness
    int16_t eyeRY = (int16_t)(EYE_RADIUS_Y * openness);

    // White of the eye (sclera)
    sprite->fillSmoothCircle(cx, cy, EYE_RADIUS_X, COLOR_EYE_WHITE);
    if (eyeRY < EYE_RADIUS_Y) {
        // Partially closed — clip with black above and below
        sprite->fillRect(cx - EYE_RADIUS_X - 2, cy - EYE_RADIUS_Y - 2, EYE_RADIUS_X * 2 + 4, EYE_RADIUS_Y - eyeRY + 2, COLOR_BG);
        sprite->fillRect(cx - EYE_RADIUS_X - 2, cy + eyeRY, EYE_RADIUS_X * 2 + 4, EYE_RADIUS_Y - eyeRY + 2, COLOR_BG);
    }

    // Iris (colored ring)
    int16_t irisX = cx + (int16_t)(pupilX * (EYE_RADIUS_X - IRIS_RADIUS - 4));
    int16_t irisY = cy + (int16_t)(pupilY * (eyeRY - IRIS_RADIUS - 4));
    sprite->fillSmoothCircle(irisX, irisY, IRIS_RADIUS, _eyeColor);

    // Pupil (black center)
    int16_t pSize = (int16_t)(PUPIL_RADIUS * _pupilSize);
    sprite->fillSmoothCircle(irisX, irisY, pSize, COLOR_PUPIL);

    // Highlight (white reflection dot)
    sprite->fillSmoothCircle(irisX - 5, irisY - 5, 4, COLOR_EYE_WHITE);
    sprite->fillSmoothCircle(irisX + 2, irisY + 2, 2, COLOR_EYE_WHITE);

    // Eye outline - blend with pure black background
    sprite->drawSmoothCircle(cx, cy, EYE_RADIUS_X, _eyeColor, COLOR_BG);
}

void FaceRenderer::_drawMouth(int16_t cx, int16_t cy) {
    // Clear old mouth area to prevent ghosting/artifact accumulation
    _sprite->fillRect(cx - MOUTH_WIDTH - 4, cy - MOUTH_HEIGHT * 2 - 4, MOUTH_WIDTH * 2 + 8, MOUTH_HEIGHT * 4 + 8, COLOR_BG);

    uint16_t mouthColor = _eyeColor;
    int16_t halfWidth = (int16_t)(MOUTH_WIDTH * _mouthWidth);

    if (_mouthOpen > 0.05f) {
        // Open mouth — ellipse
        int16_t openHeight = (int16_t)(MOUTH_HEIGHT * 2 * _mouthOpen);
        _sprite->fillSmoothCircle(cx, cy, halfWidth, mouthColor);
        _sprite->fillSmoothCircle(cx, cy, halfWidth - 2, COLOR_BG);

        // Inner mouth
        _sprite->fillEllipse(cx, cy, halfWidth - 3, max((int)(openHeight - 2), 1), 0x4208);
    } else {
        // Closed mouth — curve line
        int16_t curveY = (int16_t)(_mouthCurve * 10);

        // Draw mouth as a bezier-like curve
        for (int i = -halfWidth; i <= halfWidth; i++) {
            float t = (float)i / halfWidth;
            int16_t y = cy + (int16_t)(curveY * (1.0f - t * t));
            _sprite->drawPixel(cx + i, y, mouthColor);
            _sprite->drawPixel(cx + i, y + 1, mouthColor);
        }
    }
}

void FaceRenderer::_drawExpressionOverlay() {
    int16_t centerX = SCREEN_WIDTH / 2;
    int16_t centerY = SCREEN_HEIGHT / 2;
    int16_t startX = (SCREEN_WIDTH - 320) / 2;

    switch (_currentExpr) {
        case Expression::THINKING:
            // Animated dots centered below eyes
            {
                unsigned long t = millis() / 500;
                for (int i = 0; i < 3; i++) {
                    uint16_t color = ((t + i) % 3 == 0) ? _eyeColor : COLOR_FACE_BG;
                    _sprite->fillSmoothCircle(centerX - 18 + i * 18, centerY + 55, 4, color);
                }
            }
            break;

        case Expression::SCANNING:
            // Scanning line animation constrained to the eye viewport
            {
                int scanY = (SCREEN_HEIGHT - 160) / 2 + ((millis() / 10) % 160);
                _sprite->drawFastHLine(startX, scanY, 320, COLOR_IRIS_CYAN);
                _sprite->drawFastHLine(startX, scanY + 1, 320, 0x0410);
            }
            break;

        case Expression::ERROR:
            // Red warning triangle below eyes
            _sprite->drawTriangle(centerX, centerY + 50, centerX - 10, centerY + 70, centerX + 10, centerY + 70, COLOR_IRIS_RED);
            _sprite->setTextColor(COLOR_IRIS_RED);
            _sprite->setTextDatum(MC_DATUM);
            _sprite->drawString("!", centerX, centerY + 60);
            break;

        default:
            break;
    }
}

// ── Animation Updates ──

void FaceRenderer::_updateBlink() {
    unsigned long now = millis();

    if (_isBlinking) {
        float blinkProgress = (now - _blinkStartTime) / 500.0f;  // 500ms slow blink

        if (blinkProgress < 0.5f) {
            _leftEyeOpenness = 1.0f - blinkProgress * 2;
            _rightEyeOpenness = _leftEyeOpenness;
        } else if (blinkProgress < 1.0f) {
            _leftEyeOpenness = (blinkProgress - 0.5f) * 2;
            _rightEyeOpenness = _leftEyeOpenness;
        } else {
            _isBlinking = false;
            _leftEyeOpenness = 1.0f;
            _rightEyeOpenness = 1.0f;
            _nextBlinkTime = now + random(BLINK_MIN_INTERVAL_MS, BLINK_MAX_INTERVAL_MS);
        }
    } else if (now >= _nextBlinkTime && _currentExpr != Expression::SLEEPING) {
        _isBlinking = true;
        _blinkStartTime = now;
    }
}

void FaceRenderer::_updateEyeMovement() {
    unsigned long now = millis();

    // Random eye movement when idle
    if (_currentExpr == Expression::IDLE || _currentExpr == Expression::HAPPY) {
        if (now >= _randomMoveTime) {
            _targetEyeX = random(-60, 60) / 100.0f;
            _targetEyeY = random(-30, 30) / 100.0f;
            _randomMoveTime = now + random(2000, 5000);
        }
    }
}

void FaceRenderer::_updateBreathing() {
    // Subtle breathing animation (pupil size oscillation)
    _breathPhase += 0.03f;
    if (_breathPhase > 6.28f) _breathPhase -= 6.28f;

    float breath = sin(_breathPhase) * 0.1f;
    _pupilSize = 1.0f + breath;
}

void FaceRenderer::_applyExpression() {
    switch (_currentExpr) {
        case Expression::IDLE:
            _mouthCurve = 0.0f;
            _mouthWidth = 0.8f;
            break;

        case Expression::HAPPY:
            _mouthCurve = 1.0f;  // Smile
            _mouthWidth = 1.2f;
            _leftEyeOpenness = min(_leftEyeOpenness, 0.7f);  // Happy squint
            _rightEyeOpenness = _leftEyeOpenness;
            break;

        case Expression::THINKING:
            _targetEyeX = 0.3f;
            _targetEyeY = -0.5f;
            _mouthCurve = -0.2f;
            _leftEyeOpenness = min(_leftEyeOpenness, 0.6f);
            break;

        case Expression::LISTENING:
            _mouthCurve = 0.0f;
            _pupilSize = 1.3f;  // Wide pupils
            break;

        case Expression::TALKING:
            // Mouth animated externally via setMouthOpenness
            _mouthWidth = 1.0f;
            break;

        case Expression::ERROR:
            _mouthCurve = -0.8f;  // Frown
            break;

        case Expression::WARNING:
            _mouthCurve = -0.4f;
            _leftEyeOpenness = min(_leftEyeOpenness, 0.5f);
            break;

        case Expression::SLEEPING:
            _leftEyeOpenness = 0.0f;
            _rightEyeOpenness = 0.0f;
            _mouthCurve = 0.0f;
            break;

        case Expression::SURPRISED:
            _pupilSize = 0.6f;  // Small pupils
            _mouthOpen = 0.6f;
            break;

        case Expression::SCANNING:
            _mouthCurve = 0.0f;
            _pupilSize = 0.8f;
            break;

        case Expression::SUCCESS:
            _mouthCurve = 1.0f;
            _mouthWidth = 1.3f;
            break;

        default:
            break;
    }
}

uint16_t FaceRenderer::_getExpressionColor() {
    switch (_currentExpr) {
        case Expression::IDLE:     return COLOR_IRIS_CYAN;
        case Expression::HAPPY:    return COLOR_IRIS_GREEN;
        case Expression::SUCCESS:  return COLOR_IRIS_GREEN;
        case Expression::THINKING: return COLOR_IRIS_YELLOW;
        case Expression::LISTENING:return 0x001F;  // Blue
        case Expression::TALKING:  return COLOR_IRIS_CYAN;
        case Expression::ERROR:    return COLOR_IRIS_RED;
        case Expression::WARNING:  return COLOR_IRIS_ORANGE;
        case Expression::SLEEPING: return 0x0010;  // Dark blue
        case Expression::SURPRISED:return COLOR_IRIS_YELLOW;
        case Expression::SCANNING: return COLOR_IRIS_CYAN;
        default:                   return COLOR_IRIS_CYAN;
    }
}
