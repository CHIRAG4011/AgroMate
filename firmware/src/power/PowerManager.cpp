#include "PowerManager.h"
#include <esp_sleep.h>

void PowerManager::begin(uint32_t idleTimeoutMs) {
    _idleTimeout = idleTimeoutMs;
    _lastActivity = millis();
    // Configure wake sources
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_MODE, LOW);
}

void PowerManager::update() {
    if (_sleeping) return;
    if (millis() - _lastActivity > _idleTimeout) {
        Serial.println("💤 Idle timeout — entering deep sleep");
        enterSleep();
    }
}

void PowerManager::resetIdleTimer() {
    _lastActivity = millis();
    if (_sleeping) wakeUp();
}

void PowerManager::enterSleep() {
    _sleeping = true;
    Serial.println("Entering deep sleep...");
    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
}

void PowerManager::wakeUp() {
    _sleeping = false;
    _lastActivity = millis();
}
