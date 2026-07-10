#pragma once
#include <Arduino.h>
#include "../config.h"

class PowerManager {
public:
    void begin(uint32_t idleTimeoutMs);
    void update();
    void resetIdleTimer();
    void enterSleep();
    void wakeUp();
    bool isSleeping() { return _sleeping; }

private:
    uint32_t _idleTimeout = 300000;
    unsigned long _lastActivity = 0;
    bool _sleeping = false;
};
