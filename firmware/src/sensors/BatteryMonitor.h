#pragma once
class BatteryMonitor {
public:
    void begin() {}
    float getVoltage() { return 4.1f; }
    uint8_t getPercentage() { return 85; }
};
