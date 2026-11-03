/**
 * AgroMate Face Renderer Implementation — Beautiful Cartoon Robot Eyes
 * 
 * Renders large, expressive cartoon eyes inspired by cute robot designs:
 *   - Large round eye sprites (120x120 each, ~28KB)
 *   - Blue gradient iris with radial color bands
 *   - Smooth pupil with white specular highlights
 *   - Eyelid shadow for 3D depth
 *   - Fast 200ms blink animation
 *   - Smooth eye tracking and breathing animation
 */

#include "FaceRenderer.h"

// ── Color Palette ──
#define COLOR_BG         0x0000  // Pure black
#define COLOR_EYE_WHITE  0xFFFF  // White sclera
#define COLOR_PUPIL      0x0000  // Black pupil

// Blue iris gradient (outer to inner)
#define COLOR_IRIS_OUTER  0x000F  // Deep navy blue
#define COLOR_IRIS_MID    0x03BF  // Medium blue
#define COLOR_IRIS_INNER  0x065F  // Bright sky blue
#define COLOR_IRIS_CORE   0x07DF  // Light cyan-blue

// Other iris colors for expressions
#define COLOR_IRIS_CYAN   0x07FF
#define COLOR_IRIS_GREEN  0x07E0
#define COLOR_IRIS_RED    0xF800
#define COLOR_IRIS_YELLOW 0xFFE0
#define COLOR_IRIS_ORANGE 0xFDA0

// Mouth and effects
#define COLOR_MOUTH      0x07FF  // Cyan
#define COLOR_GLOW       0x0410  // Subtle cyan glow
#define COLOR_FACE_BG    0x0841  // Very dark gray
#define COLOR_SHADOW     0x4208  // Mid gray for eyelid shadow

// ── Eye Dimensions (100x96 sprite canvas: ~19KB each = 38KB total, fast & rock solid) ──
#define EYE_SPRITE_W     100
#define EYE_SPRITE_H     96
#define EYE_CX           50     // Center X in sprite
#define EYE_CY           48     // Center Y in sprite
#define EYE_RADIUS       44     // Main eye circle radius
#define IRIS_RADIUS       26    // Iris radius
#define PUPIL_RADIUS      13    // Pupil radius
#define EYE_SPACING       66    // Distance from center to eye center in portrait (160 - 66 = 94, 160 + 66 = 226)

// Mouth dimensions
#define MOUTH_Y_POS      330    // Below eyes on 320x480 portrait screen
#define MOUTH_WIDTH       48
#define MOUTH_HEIGHT      10

void FaceRenderer::begin(DisplayManager* display) {
    _display = display;
    _sprite = display->getTFT();
    
    // Allocate the eye sprites dynamically with safe fallbacks
    if (!_leftSprite) _leftSprite = new TFT_eSprite(_sprite);
    if (!_rightSprite) _rightSprite = new TFT_eSprite(_sprite);
    
    _leftSprite->setColorDepth(16);
    void* ptrL = _leftSprite->createSprite(EYE_SPRITE_W, EYE_SPRITE_H);
    _leftSprite->setSwapBytes(true);
    
    _rightSprite->setColorDepth(16);
    void* ptrR = _rightSprite->createSprite(EYE_SPRITE_W, EYE_SPRITE_H);
    _rightSprite->setSwapBytes(true);

    if (!ptrL || !ptrR) {
        if (_leftSprite) _leftSprite->deleteSprite();
        if (_rightSprite) _rightSprite->deleteSprite();
        _leftSprite->createSprite(80, 70);
        _rightSprite->createSprite(80, 70);
    }
    
    _nextBlinkTime = millis() + random(BLINK_MIN_INTERVAL_MS, BLINK_MAX_INTERVAL_MS);
    _randomMoveTime = millis() + random(2000, 5000);
}

void FaceRenderer::update() {
    unsigned long now = millis();
    float dt = (now - _lastUpdate) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    _lastUpdate = now;

    _updateBlink();
    _updateEyeMovement();
    _updateBreathing();
    _applyExpression();

    // Smooth mouth animation
    _mouthOpen += (_mouthTargetOpen - _mouthOpen) * 8.0f * dt;

    // Smooth eye position interpolation (faster tracking)
    _eyeX += (_targetEyeX - _eyeX) * 7.0f * dt;
    _eyeY += (_targetEyeY - _eyeY) * 7.0f * dt;
}

void FaceRenderer::render() {
    if (!_sprite || !_leftSprite || !_rightSprite) return;
    if (!_leftSprite->getPointer() || !_rightSprite->getPointer()) return;

    int screenCX = SCREEN_WIDTH / 2;
    int screenCY = 210;
    int leftEyeX = screenCX - EYE_SPACING - EYE_SPRITE_W / 2;
    int rightEyeX = screenCX + EYE_SPACING - EYE_SPRITE_W / 2;
    int eyeY = screenCY - EYE_SPRITE_H / 2;

    // 1. Draw glowing blue forehead visor (from reference robot photo)
    _drawVisorGlow();

    // 2. Draw animated expressive cartoon eyebrows above the eyes
    _drawEyebrows(leftEyeX, rightEyeX, eyeY);

    // 3. Render left and right eyes to sprites and push to screen
    _drawEye(_leftSprite, _leftEyeOpenness, _eyeX, _eyeY);
    _leftSprite->pushSprite(leftEyeX, eyeY);

    _drawEye(_rightSprite, _rightEyeOpenness, _eyeX, _eyeY);
    _rightSprite->pushSprite(rightEyeX, eyeY);

    // 4. Draw expressive mouth & overlays
    _drawMouth(screenCX, MOUTH_Y_POS);
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
    if (!sprite || !sprite->getPointer()) return;
    sprite->fillSprite(COLOR_BG);

    if (openness < 0.05f) {
        sprite->drawWideLine(EYE_CX - EYE_RADIUS + 5, EYE_CY,
                             EYE_CX + EYE_RADIUS - 5, EYE_CY, 3, _eyeColor);
        return;
    }

    // 1. Outer Glow
    sprite->drawSmoothCircle(EYE_CX, EYE_CY, EYE_RADIUS + 2, _eyeColor, COLOR_BG);
    sprite->drawSmoothCircle(EYE_CX, EYE_CY, EYE_RADIUS + 1, _eyeColor, COLOR_BG);

    // 2. White Sclera
    sprite->fillSmoothCircle(EYE_CX, EYE_CY, EYE_RADIUS, COLOR_EYE_WHITE);

    // 3. Iris/pupil position
    float maxIrisTravel = EYE_RADIUS - IRIS_RADIUS - 4;
    int16_t irisX = EYE_CX + (int16_t)(pupilX * maxIrisTravel);
    int16_t irisY = EYE_CY + (int16_t)(pupilY * maxIrisTravel * 0.7f);

    // 4. Iris gradient colors based on expression
    uint16_t irisOuter, irisMid, irisInner, irisCore;

    if (_eyeColor == COLOR_IRIS_CYAN || _eyeColor == 0x001F || _eyeColor == 0x03BF) {
        irisOuter = COLOR_IRIS_OUTER;
        irisMid   = COLOR_IRIS_MID;
        irisInner = COLOR_IRIS_INNER;
        irisCore  = COLOR_IRIS_CORE;
    } else if (_eyeColor == COLOR_IRIS_GREEN) {
        irisOuter = 0x01E0;
        irisMid   = 0x03E0;
        irisInner = 0x07E0;
        irisCore  = 0x87F0;
    } else if (_eyeColor == COLOR_IRIS_RED) {
        irisOuter = 0x5000;
        irisMid   = 0x9800;
        irisInner = 0xF800;
        irisCore  = 0xFB20;
    } else if (_eyeColor == COLOR_IRIS_YELLOW) {
        irisOuter = 0x8400;
        irisMid   = 0xC600;
        irisInner = 0xFFE0;
        irisCore  = 0xFFF0;
    } else if (_eyeColor == COLOR_IRIS_ORANGE) {
        irisOuter = 0x8200;
        irisMid   = 0xC400;
        irisInner = 0xFDA0;
        irisCore  = 0xFEC0;
    } else {
        irisOuter = _eyeColor;
        irisMid   = _eyeColor;
        irisInner = _eyeColor;
        irisCore  = COLOR_EYE_WHITE;
    }

    // Draw gradient rings
    sprite->fillSmoothCircle(irisX, irisY, IRIS_RADIUS,      irisOuter);
    sprite->fillSmoothCircle(irisX, irisY, IRIS_RADIUS - 5,  irisMid);
    sprite->fillSmoothCircle(irisX, irisY, IRIS_RADIUS - 10, irisInner);
    sprite->fillSmoothCircle(irisX, irisY, IRIS_RADIUS - 15, irisCore);

    // 5. Pupil
    int16_t pSize = (int16_t)(PUPIL_RADIUS * _pupilSize);
    if (pSize < 4) pSize = 4;
    sprite->fillSmoothCircle(irisX, irisY, pSize, COLOR_PUPIL);

    // 6. Specular Highlights — Pixar/Disney alive character sparkle
    sprite->fillSmoothCircle(irisX - 8, irisY - 9, 6, COLOR_EYE_WHITE);  // Primary shine
    sprite->fillSmoothCircle(irisX + 7, irisY + 6, 3, COLOR_EYE_WHITE);  // Secondary catchlight
    sprite->fillCircle(irisX + 4, irisY - 11, 2, COLOR_EYE_WHITE);        // Micro sparkle

    // 7. Iris outline
    sprite->drawSmoothCircle(irisX, irisY, IRIS_RADIUS, irisOuter, COLOR_EYE_WHITE);

    // 8. Top Eyelid Shadow
    for (int i = 0; i < 12; i++) {
        int y = EYE_CY - EYE_RADIUS + i;
        float dy = (float)(y - EYE_CY) / EYE_RADIUS;
        if (dy < -1.0f) dy = -1.0f;
        float xExtent = sqrt(1.0f - dy * dy) * EYE_RADIUS;
        int x1 = EYE_CX - (int)xExtent;
        int x2 = EYE_CX + (int)xExtent;
        if (x2 > x1) {
            uint16_t shadowColor = (i < 6) ? 0x2104 : 0x4208;
            sprite->drawFastHLine(x1 + 1, y, x2 - x1 - 2, shadowColor);
        }
    }

    // 9. Eyelid Clipping for blink/squint
    if (openness < 1.0f) {
        int16_t eyeRY = (int16_t)(EYE_RADIUS * openness);
        int clipTop = EYE_CY - EYE_RADIUS;
        int clipHeight = EYE_RADIUS - eyeRY;
        if (clipHeight > 0) {
            sprite->fillRect(0, clipTop, EYE_SPRITE_W, clipHeight, COLOR_BG);
            int edgeY = clipTop + clipHeight;
            float dy2 = (float)(edgeY - EYE_CY) / EYE_RADIUS;
            if (dy2 >= -1.0f && dy2 <= 1.0f) {
                float xExt2 = sqrt(1.0f - dy2 * dy2) * EYE_RADIUS;
                sprite->drawWideLine(EYE_CX - xExt2, edgeY, EYE_CX + xExt2, edgeY, 2, _eyeColor);
            }
            sprite->fillRect(0, EYE_CY + eyeRY, EYE_SPRITE_W, EYE_RADIUS - eyeRY + 2, COLOR_BG);
            edgeY = EYE_CY + eyeRY;
            dy2 = (float)(edgeY - EYE_CY) / EYE_RADIUS;
            if (dy2 >= -1.0f && dy2 <= 1.0f) {
                float xExt2 = sqrt(1.0f - dy2 * dy2) * EYE_RADIUS;
                sprite->drawWideLine(EYE_CX - xExt2, edgeY, EYE_CX + xExt2, edgeY, 2, _eyeColor);
            }
        }
    }

    // 10. Eye outline
    sprite->drawSmoothCircle(EYE_CX, EYE_CY, EYE_RADIUS, _eyeColor, COLOR_BG);
}

void FaceRenderer::_drawMouth(int16_t cx, int16_t cy) {
    _sprite->fillRect(cx - MOUTH_WIDTH - 4, cy - MOUTH_HEIGHT * 2 - 4,
                      MOUTH_WIDTH * 2 + 8, MOUTH_HEIGHT * 4 + 8, COLOR_BG);

    uint16_t mouthColor = _eyeColor;
    int16_t halfWidth = (int16_t)(MOUTH_WIDTH * _mouthWidth);

    if (_mouthOpen > 0.05f) {
        int16_t openHeight = (int16_t)(MOUTH_HEIGHT * 2 * _mouthOpen);
        _sprite->fillSmoothCircle(cx, cy, halfWidth, mouthColor);
        _sprite->fillSmoothCircle(cx, cy, halfWidth - 2, COLOR_BG);
        _sprite->fillEllipse(cx, cy, halfWidth - 3, max((int)(openHeight - 2), 1), 0x4208);
    } else {
        int16_t curveY = (int16_t)(_mouthCurve * 10);
        for (int i = -halfWidth; i <= halfWidth; i++) {
            float t = (float)i / halfWidth;
            int16_t y = cy + (int16_t)(curveY * (1.0f - t * t));
            _sprite->drawPixel(cx + i, y, mouthColor);
            _sprite->drawPixel(cx + i, y + 1, mouthColor);
        }
    }
}

void FaceRenderer::_drawVisorGlow() {
    // Glowing futuristic blue/cyan forehead visor (inspired by reference robot photo)
    int16_t vx = SCREEN_WIDTH / 2;
    int16_t vy = 85;
    float breathMod = sin(_breathPhase) * 2.5f;
    int16_t radX = 58 + (int16_t)breathMod;
    int16_t radY = 16 + (int16_t)(breathMod * 0.5f);

    // Layered glowing cyan/blue visor aura
    _sprite->fillEllipse(vx, vy, radX + 6, radY + 4, 0x0013); // Outer subtle navy glow
    _sprite->fillEllipse(vx, vy, radX + 3, radY + 2, 0x0119); // Mid blue
    _sprite->fillEllipse(vx, vy, radX,     radY,     0x035F); // Vibrant electric blue
    _sprite->fillEllipse(vx, vy - 2, radX - 14, radY - 6, 0x067F); // Glowing cyan
    _sprite->fillEllipse(vx, vy - 4, radX - 28, radY - 9, 0x9FFF); // Soft white-cyan core shine
}

void FaceRenderer::_drawEyebrows(int16_t leftEyeX, int16_t rightEyeX, int16_t eyeY) {
    // Clear previous eyebrow regions right above the eyes
    int16_t browAreaY = eyeY - 26;
    int16_t browAreaH = 26;
    _sprite->fillRect(leftEyeX - 4, browAreaY, EYE_SPRITE_W + 8, browAreaH, COLOR_BG);
    _sprite->fillRect(rightEyeX - 4, browAreaY, EYE_SPRITE_W + 8, browAreaH, COLOR_BG);

    // Determine eyebrow height, tilt, and curve based on expression
    int16_t leftBaseY = eyeY - 8;
    int16_t rightBaseY = eyeY - 8;
    int16_t leftTilt = 0;
    int16_t rightTilt = 0;
    int16_t archHeight = 8;
    uint16_t browColor = 0x05BF; // Cyan-blue outline default

    switch (_currentExpr) {
        case Expression::HAPPY:
        case Expression::SUCCESS:
            leftBaseY -= 8;
            rightBaseY -= 8;
            archHeight = 12;
            browColor = COLOR_IRIS_GREEN;
            break;
        case Expression::THINKING:
            leftBaseY -= 12; // Raised inquisitive left eyebrow
            leftTilt = -4;
            rightBaseY += 2; // Lowered furrowed right eyebrow
            rightTilt = 4;
            archHeight = 6;
            browColor = COLOR_IRIS_YELLOW;
            break;
        case Expression::SCANNING:
            leftBaseY -= 2;
            rightBaseY -= 2;
            archHeight = 2; // Flat and sharp
            browColor = COLOR_IRIS_CYAN;
            break;
        case Expression::ERROR:
            leftTilt = 8; // Slanted down towards center (concerned/sad)
            rightTilt = -8;
            archHeight = 4;
            browColor = COLOR_IRIS_RED;
            break;
        case Expression::WARNING:
            leftTilt = 6;
            rightTilt = -6;
            archHeight = 5;
            browColor = COLOR_IRIS_ORANGE;
            break;
        case Expression::SURPRISED:
            leftBaseY -= 15;
            rightBaseY -= 15;
            archHeight = 14;
            browColor = COLOR_IRIS_YELLOW;
            break;
        case Expression::SLEEPING:
            leftBaseY += 4;
            rightBaseY += 4;
            archHeight = 2;
            browColor = 0x0110;
            break;
        default: // IDLE, TALKING, LISTENING
            archHeight = 7;
            browColor = 0x05BF;
            break;
    }

    // Left Eyebrow (smooth arched cartoon curve)
    int16_t lx1 = leftEyeX + 16;
    int16_t lx2 = leftEyeX + EYE_SPRITE_W - 16;
    int16_t lSpan = lx2 - lx1;
    for (int16_t x = lx1; x <= lx2; x++) {
        float t = 2.0f * (float)(x - lx1) / lSpan - 1.0f; // -1 to +1
        int16_t y = leftBaseY - (int16_t)(archHeight * (1.0f - t * t)) + (int16_t)(t * leftTilt);
        _sprite->drawPixel(x, y - 1, browColor);
        _sprite->drawPixel(x, y, browColor);
        _sprite->drawPixel(x, y + 1, browColor);
    }

    // Right Eyebrow (smooth arched cartoon curve)
    int16_t rx1 = rightEyeX + 16;
    int16_t rx2 = rightEyeX + EYE_SPRITE_W - 16;
    int16_t rSpan = rx2 - rx1;
    for (int16_t x = rx1; x <= rx2; x++) {
        float t = 2.0f * (float)(x - rx1) / rSpan - 1.0f; // -1 to +1
        int16_t y = rightBaseY - (int16_t)(archHeight * (1.0f - t * t)) + (int16_t)(t * rightTilt);
        _sprite->drawPixel(x, y - 1, browColor);
        _sprite->drawPixel(x, y, browColor);
        _sprite->drawPixel(x, y + 1, browColor);
    }
}

void FaceRenderer::_drawExpressionOverlay() {
    int16_t centerX = SCREEN_WIDTH / 2;
    int16_t centerY = SCREEN_HEIGHT / 2;
    int16_t startX = (SCREEN_WIDTH - 320) / 2;

    switch (_currentExpr) {
        case Expression::THINKING:
            {
                unsigned long t = millis() / 500;
                for (int i = 0; i < 3; i++) {
                    uint16_t color = ((t + i) % 3 == 0) ? _eyeColor : COLOR_FACE_BG;
                    _sprite->fillSmoothCircle(centerX - 18 + i * 18, centerY + 55, 4, color);
                }
            }
            break;

        case Expression::SCANNING:
            {
                int scanY = (SCREEN_HEIGHT - 160) / 2 + ((millis() / 10) % 160);
                _sprite->drawFastHLine(startX, scanY, 320, COLOR_IRIS_CYAN);
                _sprite->drawFastHLine(startX, scanY + 1, 320, 0x0410);
            }
            break;

        case Expression::ERROR:
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
        float blinkProgress = (now - _blinkStartTime) / 200.0f;

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
    if (_currentExpr == Expression::IDLE || _currentExpr == Expression::HAPPY) {
        if (now >= _randomMoveTime) {
            _targetEyeX = random(-60, 60) / 100.0f;
            _targetEyeY = random(-30, 30) / 100.0f;
            _randomMoveTime = now + random(2000, 5000);
        }
    }
}

void FaceRenderer::_updateBreathing() {
    _breathPhase += 0.04f;
    if (_breathPhase > 6.28f) _breathPhase -= 6.28f;
    float breath = sin(_breathPhase) * 0.12f;
    _pupilSize = 1.0f + breath;
}

void FaceRenderer::_applyExpression() {
    switch (_currentExpr) {
        case Expression::IDLE:
            _mouthCurve = 0.0f;
            _mouthWidth = 0.8f;
            break;
        case Expression::HAPPY:
            _mouthCurve = 1.0f;
            _mouthWidth = 1.2f;
            _leftEyeOpenness = min(_leftEyeOpenness, 0.7f);
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
            _pupilSize = 1.3f;
            break;
        case Expression::TALKING:
            _mouthWidth = 1.0f;
            break;
        case Expression::ERROR:
            _mouthCurve = -0.8f;
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
            _pupilSize = 0.6f;
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
        case Expression::IDLE:     return 0x03BF;   // Beautiful medium blue
        case Expression::HAPPY:    return COLOR_IRIS_GREEN;
        case Expression::SUCCESS:  return COLOR_IRIS_GREEN;
        case Expression::THINKING: return COLOR_IRIS_YELLOW;
        case Expression::LISTENING:return 0x03BF;   // Blue
        case Expression::TALKING:  return 0x03BF;   // Blue
        case Expression::ERROR:    return COLOR_IRIS_RED;
        case Expression::WARNING:  return COLOR_IRIS_ORANGE;
        case Expression::SLEEPING: return 0x0010;   // Dark blue
        case Expression::SURPRISED:return COLOR_IRIS_YELLOW;
        case Expression::SCANNING: return COLOR_IRIS_CYAN;
        default:                   return 0x03BF;    // Blue
    }
}
