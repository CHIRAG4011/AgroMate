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
    bool isPlaying() { return _playing || _streamActive || _bootVoiceActive; }
    float getPlaybackLevel();

    // Boot voice — Non-blocking background playback from flash
    void startBootVoice();
    void stopBootVoice();
    bool isBootVoiceActive() const { return _bootVoiceActive; }
    void playBootVoice(void (*progressCallback)(float progress, const char* status) = nullptr); // Legacy blocking support
    void playScanStartVoice();     // "Scanning started..."
    void playScanCompleteVoice();  // "Scan completed..."
    void playScanProgressBeep();   // Short periodic beep (tone only)
    void playPcmFromProgmem(const uint8_t* data, size_t length);

    // Streaming PCM Audio (for long TTS speech over BLE without RAM limits)
    void startAudioStream();
    void writeAudioStream(const uint8_t* data, size_t length);
    void stopAudioStream();
    bool isStreaming() { return _streamActive; }

    void setVolume(float vol) { _volume = constrain(vol, 0.05f, 1.0f); }
    float getVolume() const { return _volume; }

    // Dedicated FreeRTOS Audio Task on Core 0
    void taskLoop();

private:
    float _volume = 1.0f; // Maximum volume — no attenuation, full DAC range
    TaskHandle_t _audioTaskHandle = NULL;
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

    // Boot voice playback state (non-blocking)
    bool _bootVoiceActive = false;
    size_t _bootVoicePos = 0;

    // Streaming Ring Buffer (8192 int16_t samples = 16KB)
    static const size_t STREAM_RING_SAMPLES = 16384; // 16384 samples = 1 second at 16kHz, absorbs BLE jitter
    int16_t _ringBuffer[STREAM_RING_SAMPLES];
    volatile size_t _ringHead = 0;
    volatile size_t _ringTail = 0;
    volatile size_t _ringCount = 0;
    bool _streamActive = false;
    bool _streamFinished = false;
    bool _streamPlaybackStarted = false; // True once pre-buffer threshold reached
    void _drainRingToI2S(size_t maxSamples);
    
    // I2S mode switching
    void _switchToMic();
    void _switchToSpeaker();
    void _stopSpeaker();
    void _uninstallI2S();
    
    // Helper: play a single tone at given frequency for given duration
    void _playToneBlocking(float freqHz, uint32_t durationMs, float amplitude = 30000.0f);
};
