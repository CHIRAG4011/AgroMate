#pragma once
/**
 * AgroMate Audio Manager — Time-Multiplexed I2S for INMP441 + MAX98357A
 * 
 * Uses a SINGLE I2S port (I2S_NUM_0) shared between the microphone and
 * speaker. The driver is reconfigured when switching between recording
 * and playback modes. This saves 2 GPIO pins vs. using separate I2S ports.
 * 
 * Workflow: IDLE → switchToMic() → record → switchToSpeaker() → play → IDLE
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include "../config.h"

enum class AudioMode : uint8_t {
    IDLE = 0,
    MIC_ACTIVE = 1,     // I2S configured for INMP441 input
    SPEAKER_ACTIVE = 2  // I2S configured for MAX98357A output
};

class AudioManager {
public:
    void begin();
    void update();
    void startRecording(uint32_t durationMs = 5000);
    void stopRecording();
    bool isRecording() { return _recording; }
    uint8_t* getRecordedData(size_t* outSize);
    void playBuffer(uint8_t* data, size_t length);
    void playTestTone(uint32_t durationMs = 3000);
    bool isPlaying() { return _playing; }
    float getPlaybackLevel();

    // Scan voice feedback tones
    void playScanStartTone();     // Ascending 3-note chime
    void playScanProgressBeep();  // Short periodic beep
    void playScanCompleteTone();  // Success arpeggio chime

private:
    bool _recording = false;
    bool _playing = false;
    AudioMode _currentMode = AudioMode::IDLE;
    
    // Recording state
    uint8_t* _recordBuffer = nullptr;
    size_t _recordSize = 0;
    size_t _recordPos = 0;
    uint32_t _recordDuration = 0;
    unsigned long _recordStart = 0;
    
    // Playback state
    uint8_t* _playBuffer = nullptr;
    size_t _playSize = 0;
    size_t _playPos = 0;
    float _playLevel = 0.0f;
    
    // I2S mode switching
    void _switchToMic();
    void _switchToSpeaker();
    void _uninstallI2S();
    
    // Helper: play a single tone at given frequency for given duration
    void _playToneBlocking(float freqHz, uint32_t durationMs, float amplitude = 12000.0f);
};
