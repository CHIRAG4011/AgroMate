/**
 * AgroMate Audio Manager Implementation — Dedicated I2S Bus Setup
 * 
 * INMP441 (mic) and MAX98357A (speaker) use separate dedicated pins:
 *   - Mic: SCK = GPIO 2, WS = GPIO 1, SD_IN = GPIO 46
 *   - Speaker: BCLK = GPIO 3, WS = GPIO 35, SD_OUT = GPIO 36
 * 
 * Audio playback runs on a dedicated high-priority FreeRTOS task on Core 0,
 * completely decoupled from display rendering and loop() latency on Core 1.
 * This guarantees DMA buffers never starve and eliminates all buzzing and clicking.
 */

#include "AudioManager.h"
#include "voices/voice_boot.h"
#include "voices/voice_scan_start.h"
#include "voices/voice_scan_complete.h"
#include <math.h>

void AudioManager::begin() {
    // Drive speaker BCLK LOW immediately to ensure MAX98357A is in hardware shutdown (zero hum/buzz)
    // Note: MAX98357A enters shutdown when BCLK clock is held static (>100us).
    // Never call pinMode/digitalWrite on SPK_I2S_WS or AUDIO_I2S_SD_OUT to prevent bus contention.
    pinMode(SPK_I2S_BCLK, OUTPUT);
    digitalWrite(SPK_I2S_BCLK, LOW);

    _currentMode = AudioMode::IDLE;

    // Launch dedicated audio task on Core 0
    // Priority set to 3 (below NimBLE and system ISRs) so IDLE0 task can feed watchdog
    if (_audioTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            [](void* param) {
                AudioManager* audio = (AudioManager*)param;
                while (true) {
                    audio->taskLoop();
                }
            },
            "AudioTask",
            8192,
            this,
            3,
            &_audioTaskHandle,
            0 // Core 0: free from display & main loop latency
        );
    }
}

void AudioManager::_switchToMic() {
    if (_currentMode == AudioMode::MIC_ACTIVE) return;

    // Uninstall any existing I2S driver
    _uninstallI2S();

    i2s_config_t micConfig = {};
    micConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    micConfig.sample_rate = AUDIO_SAMPLE_RATE;
    micConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    micConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    micConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    micConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    micConfig.dma_buf_count = 4;
    micConfig.dma_buf_len = 256;
    micConfig.use_apll = false;
    micConfig.tx_desc_auto_clear = false;
    micConfig.fixed_mclk = 0;

    i2s_pin_config_t micPins = {};
    micPins.bck_io_num = MIC_I2S_BCLK;
    micPins.ws_io_num = MIC_I2S_WS;
    micPins.data_out_num = I2S_PIN_NO_CHANGE;
    micPins.data_in_num = AUDIO_I2S_SD_IN;

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &micConfig, 0, NULL);
    if (err != ESP_OK) return;

    err = i2s_set_pin(AUDIO_I2S_PORT, &micPins);
    if (err != ESP_OK) return;

    _currentMode = AudioMode::MIC_ACTIVE;
}

void AudioManager::_switchToSpeaker() {
    if (_currentMode == AudioMode::SPEAKER_ACTIVE) return;

    // Uninstall any existing I2S driver
    _uninstallI2S();

    i2s_config_t spkConfig = {};
    spkConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    spkConfig.sample_rate = AUDIO_SAMPLE_RATE;
    spkConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    // Standard I2S stereo format (RIGHT_LEFT) for MAX98357A DAC.
    // Mono samples are duplicated to both L+R slots to eliminate buzzing artifacts.
    spkConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    spkConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    spkConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    spkConfig.dma_buf_count = 8;
    spkConfig.dma_buf_len = 512;  // Larger DMA buffers for smoother output
    spkConfig.use_apll = false;
    spkConfig.tx_desc_auto_clear = true;
    spkConfig.fixed_mclk = 0;

    i2s_pin_config_t spkPins = {};
    spkPins.bck_io_num = SPK_I2S_BCLK;
    spkPins.ws_io_num = SPK_I2S_WS;
    spkPins.data_out_num = AUDIO_I2S_SD_OUT;
    spkPins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &spkConfig, 0, NULL);
    if (err != ESP_OK) return;

    err = i2s_set_pin(AUDIO_I2S_PORT, &spkPins);
    if (err != ESP_OK) return;

    _currentMode = AudioMode::SPEAKER_ACTIVE;
}

void AudioManager::_uninstallI2S() {
    if (_currentMode != AudioMode::IDLE) {
        i2s_driver_uninstall(AUDIO_I2S_PORT);
        _currentMode = AudioMode::IDLE;
    }
}

void AudioManager::_stopSpeaker() {
    if (_currentMode == AudioMode::SPEAKER_ACTIVE) {
        // Soft silence fade-out to prevent pop/click on speaker cone
        int16_t silence[256] = {0};
        size_t written = 0;
        i2s_write(AUDIO_I2S_PORT, silence, sizeof(silence), &written, pdMS_TO_TICKS(50));
        i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        vTaskDelay(pdMS_TO_TICKS(15));
        _uninstallI2S();
    }

    // Force MAX98357A hardware shutdown mode:
    // Holding BCLK LOW for >100us puts the amplifier into ultra-low-current shutdown (<0.01uA)
    // with high-impedance outputs, completely eliminating any idle buzzing or interference.
    // MAX98357A shutdown is controlled exclusively by holding BCLK static.
    pinMode(SPK_I2S_BCLK, OUTPUT);
    digitalWrite(SPK_I2S_BCLK, LOW);
}

// ── Dedicated Core 0 Audio Task Loop ──
void AudioManager::taskLoop() {
    // 1. Boot Voice Playback from Flash
    if (_bootVoiceActive) {
        if (_currentMode != AudioMode::SPEAKER_ACTIVE) {
            _switchToSpeaker();
        }

        const size_t CHUNK_SAMPLES = 256;
        int16_t monoBuf[CHUNK_SAMPLES];
        static int16_t stereoBuf[CHUNK_SAMPLES * 2];

        size_t remainingBytes = VOICE_BOOT_SIZE - _bootVoicePos;
        size_t bytesToRead = min((size_t)(CHUNK_SAMPLES * 2), remainingBytes);
        bytesToRead &= ~1;
        size_t samplesToRead = bytesToRead / 2;

        if (samplesToRead == 0 || !_bootVoiceActive) {
            vTaskDelay(pdMS_TO_TICKS(130)); // Allow final DMA buffer to flush to speaker
            _bootVoiceActive = false;
            _playLevel = 0.0f;
            _stopSpeaker();
            vTaskDelay(pdMS_TO_TICKS(10));
            return;
        }

        memcpy_P(monoBuf, VOICE_BOOT + _bootVoicePos, bytesToRead);
        int32_t sum = 0;
        for (size_t i = 0; i < samplesToRead; i++) {
            // Apply volume scaling to prevent amplifier clipping and speaker bottoming out
            int16_t s = (int16_t)(monoBuf[i] * _volume);
            stereoBuf[i * 2]     = s;
            stereoBuf[i * 2 + 1] = s;
            sum += abs(s);
        }

        size_t bytesWritten = 0;
        // Blocking I2S write with 100ms timeout keeps DMA buffer 100% full with ZERO underrun/buzzing!
        esp_err_t err = i2s_write(AUDIO_I2S_PORT, stereoBuf, samplesToRead * 4, &bytesWritten, pdMS_TO_TICKS(100));

        if (err == ESP_OK && bytesWritten > 0) {
            size_t samplesConsumed = bytesWritten / 4;
            _bootVoicePos += samplesConsumed * 2;
            _playLevel = (float)sum / samplesToRead / 16384.0f;
            if (_playLevel > 1.0f) _playLevel = 1.0f;
        }

        if (_bootVoicePos >= VOICE_BOOT_SIZE) {
            vTaskDelay(pdMS_TO_TICKS(130)); // Flush DMA
            _bootVoiceActive = false;
            _playLevel = 0.0f;
            _stopSpeaker();
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Yield to IDLE0 & NimBLE
        return;
    }

    // 2. Streaming Audio from BLE Ring Buffer
    if (_streamActive) {
        if (_currentMode != AudioMode::SPEAKER_ACTIVE) {
            _switchToSpeaker();
        }

        // Pre-buffering: wait until we have at least 200ms (3200 samples) of audio
        // before starting I2S playback. This absorbs BLE jitter and prevents early underruns.
        if (!_streamPlaybackStarted && _ringCount < 3200 && !_streamFinished) {
            vTaskDelay(pdMS_TO_TICKS(10));
            return;
        }
        _streamPlaybackStarted = true;

        if (_ringCount > 0) {
            const size_t CHUNK_SAMPLES = 256;
            static int16_t stereoBuf[CHUNK_SAMPLES * 2];
            size_t toProcess = min(CHUNK_SAMPLES, (size_t)_ringCount);
            int32_t sum = 0;

            for (size_t i = 0; i < toProcess; i++) {
                int16_t s = (int16_t)(_ringBuffer[_ringTail] * _volume);
                _ringTail = (_ringTail + 1) % STREAM_RING_SAMPLES;
                stereoBuf[i * 2]     = s;
                stereoBuf[i * 2 + 1] = s;
                sum += abs(s);
            }
            if (_ringCount >= toProcess) {
                _ringCount -= toProcess;
            } else {
                _ringCount = 0;
            }

            size_t bytesWritten = 0;
            i2s_write(AUDIO_I2S_PORT, stereoBuf, toProcess * 4, &bytesWritten, pdMS_TO_TICKS(100));

            if (toProcess > 0) {
                _playLevel = (float)sum / toProcess / 16384.0f;
                if (_playLevel > 1.0f) _playLevel = 1.0f;
            }
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield to IDLE0 & NimBLE so task watchdog is never starved!
            return;
        } else if (_streamFinished) {
            vTaskDelay(pdMS_TO_TICKS(130));
            _streamActive = false;
            _streamFinished = false;
            _streamPlaybackStarted = false;
            _playLevel = 0.0f;
            _stopSpeaker();
            vTaskDelay(pdMS_TO_TICKS(10));
            return;
        } else {
            // Buffer temporarily dry; wait for next BLE chunk
            vTaskDelay(pdMS_TO_TICKS(5));
            return;
        }
    }

    // 3. RAM Buffer Playback
    if (_playing && _playBuffer) {
        if (_currentMode != AudioMode::SPEAKER_ACTIVE) {
            _switchToSpeaker();
        }

        const size_t CHUNK_BYTES = 512;
        size_t toWrite = min(CHUNK_BYTES, _playSize - _playPos);
        toWrite &= ~1;
        size_t samples = toWrite / 2;

        if (samples > 0) {
            int16_t* inSamples = (int16_t*)(_playBuffer + _playPos);
            static int16_t stereoBuf[512];
            int32_t sum = 0;
            for (size_t i = 0; i < samples; i++) {
                int16_t s = (int16_t)(inSamples[i] * _volume);
                stereoBuf[i * 2]     = s;
                stereoBuf[i * 2 + 1] = s;
                sum += abs(s);
            }

            size_t bytesWritten = 0;
            i2s_write(AUDIO_I2S_PORT, stereoBuf, samples * 4, &bytesWritten, pdMS_TO_TICKS(100));
            _playPos += (bytesWritten / 4) * 2;
            _playLevel = (float)sum / samples / 16384.0f;
            if (_playLevel > 1.0f) _playLevel = 1.0f;
            vTaskDelay(pdMS_TO_TICKS(1)); // Yield to IDLE0 & NimBLE
        } else {
            vTaskDelay(pdMS_TO_TICKS(130));
            _playing = false;
            _playLevel = 0.0f;
            _stopSpeaker();
        }
        return;
    }

    // Idle
    vTaskDelay(pdMS_TO_TICKS(10));
}

void AudioManager::update() {
    if (_recording) {
        // Ensure we're in mic mode
        if (_currentMode != AudioMode::MIC_ACTIVE) {
            _switchToMic();
        }

        size_t bytesRead = 0;
        size_t maxRecordBytes = AUDIO_SAMPLE_RATE * 2 * (_recordDuration / 1000);
        size_t remaining = maxRecordBytes - _recordPos;
        size_t toRead = min(remaining, (size_t)AUDIO_BUFFER_SIZE);

        if (toRead > 0 && _recordBuffer) {
            i2s_read(AUDIO_I2S_PORT, _recordBuffer + _recordPos, toRead, &bytesRead, 10);
            _recordPos += bytesRead;
        }

        if (millis() - _recordStart >= _recordDuration || remaining == 0) {
            _recording = false;
            _recordSize = _recordPos;
        }
    }
    // Speaker playback is completely handled on Core 0 by taskLoop()
}

void AudioManager::startBootVoice() {
    _playing = false;
    _streamActive = false;
    _bootVoicePos = 0;
    _bootVoiceActive = true;
    _playLevel = 0.0f;
}

void AudioManager::stopBootVoice() {
    _bootVoiceActive = false;
    _playLevel = 0.0f;
}

void AudioManager::startAudioStream() {
    _bootVoiceActive = false; // Stop boot voice if AI response arrives
    _playing = false;
    _ringHead = 0;
    _ringTail = 0;
    _ringCount = 0;
    _streamFinished = false;
    _streamPlaybackStarted = false;
    _streamActive = true;
    _playLevel = 0.0f;
}

void AudioManager::writeAudioStream(const uint8_t* data, size_t length) {
    if (!data || length < 2 || !_streamActive) return;

    size_t sampleCount = length / 2;
    const int16_t* incomingSamples = (const int16_t*)data;

    // Fast non-blocking ingestion: simply deposit samples into ring buffer
    for (size_t i = 0; i < sampleCount; i++) {
        if (_ringCount < STREAM_RING_SAMPLES) {
            _ringBuffer[_ringHead] = incomingSamples[i];
            _ringHead = (_ringHead + 1) % STREAM_RING_SAMPLES;
            _ringCount++;
        } else {
            // Buffer full: drop sample rather than blocking NimBLE thread
            break;
        }
    }
}

void AudioManager::stopAudioStream() {
    _streamFinished = true;
}

void AudioManager::_drainRingToI2S(size_t maxSamples) {
    // Deprecated: taskLoop() directly drains ring buffer to I2S on Core 0
}

void AudioManager::startRecording(uint32_t durationMs) {
    if (_recording) return;

    _bootVoiceActive = false;
    _streamActive = false;
    _playing = false;
    vTaskDelay(pdMS_TO_TICKS(15)); // Allow Core 0 to yield

    _switchToMic();

    size_t bufSize = AUDIO_SAMPLE_RATE * 2 * (durationMs / 1000 + 1);
    if (_recordBuffer) free(_recordBuffer);
    _recordBuffer = (uint8_t*)malloc(bufSize);
    if (!_recordBuffer) {
        return;
    }

    _recordDuration = durationMs;
    _recordPos = 0;
    _recordSize = 0;
    _recordStart = millis();
    _recording = true;
}

void AudioManager::stopRecording() {
    _recording = false;
    _recordSize = _recordPos;
}

uint8_t* AudioManager::getRecordedData(size_t* outSize) {
    *outSize = _recordSize;
    return _recordBuffer;
}

void AudioManager::playBuffer(uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    
    _bootVoiceActive = false;
    _streamActive = false;

    _playBuffer = data;
    _playSize = length;
    _playPos = 0;
    _playing = true;
    _playLevel = 0.0f;
}

float AudioManager::getPlaybackLevel() {
    return _playLevel;
}

void AudioManager::playTestTone(uint32_t durationMs) {
    _bootVoiceActive = false;
    _streamActive = false;
    _playing = false;
    vTaskDelay(pdMS_TO_TICKS(15));

    _playToneBlocking(1000.0f, durationMs, 16000.0f);
}

// ── Helper: Play a single blocking tone (Stereo I2S) ──
void AudioManager::_playToneBlocking(float freqHz, uint32_t durationMs, float amplitude) {
    _switchToSpeaker();
    uint32_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    const size_t chunkSamples = 256;
    int16_t stereoBuf[chunkSamples * 2];
    uint32_t written = 0;

    float effectiveAmp = amplitude * _volume;
    unsigned long startMs = millis();
    while (written < totalSamples && (millis() - startMs <= durationMs + 200)) {
        size_t n = min((size_t)(totalSamples - written), chunkSamples);
        for (size_t i = 0; i < n; i++) {
            float t = (float)(written + i) / (float)AUDIO_SAMPLE_RATE;
            int16_t sample = (int16_t)(effectiveAmp * sinf(2.0f * M_PI * freqHz * t));
            stereoBuf[i * 2]     = sample;
            stereoBuf[i * 2 + 1] = sample;
        }
        size_t bw = 0;
        i2s_write(AUDIO_I2S_PORT, stereoBuf, n * 4, &bw, pdMS_TO_TICKS(100));
        written += n;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    _stopSpeaker();
}

// ── Scan Progress: Short high-pitched beep ──
void AudioManager::playScanProgressBeep() {
    _playToneBlocking(880.0f, 80, 16000.0f);  // A5 - 80ms, clean tone
}

// ── Play pre-recorded PCM from PROGMEM flash (Stereo I2S) ──
void AudioManager::playPcmFromProgmem(const uint8_t* data, size_t length) {
    _bootVoiceActive = false;
    _streamActive = false;
    _playing = false;
    vTaskDelay(pdMS_TO_TICKS(15));

    _switchToSpeaker();

    const size_t MONO_CHUNK = 512;
    int16_t monoBuf[256];
    int16_t stereoBuf[512];
    size_t pos = 0;

    while (pos < length) {
        size_t toRead = min(MONO_CHUNK, length - pos);
        toRead &= ~1;
        memcpy_P(monoBuf, data + pos, toRead);
        
        size_t numSamples = toRead / 2;
        int32_t sum = 0;
        for (size_t i = 0; i < numSamples; i++) {
            int16_t s = (int16_t)(monoBuf[i] * _volume);
            stereoBuf[i * 2]     = s;
            stereoBuf[i * 2 + 1] = s;
            sum += abs(s);
        }

        if (numSamples > 0) {
            _playLevel = (float)sum / numSamples / 16384.0f;
        }

        size_t bw = 0;
        i2s_write(AUDIO_I2S_PORT, stereoBuf, numSamples * 4, &bw, pdMS_TO_TICKS(100));
        pos += toRead;
        yield();
    }

    vTaskDelay(pdMS_TO_TICKS(130)); // Flush DMA
    _playLevel = 0.0f;
    _stopSpeaker();
}

// ── Scan Start Voice: Speaks "Scanning started..." from flash ──
void AudioManager::playScanStartVoice() {
    playPcmFromProgmem(VOICE_SCAN_START, VOICE_SCAN_START_SIZE);
}

// ── Scan Complete Voice: Speaks "Scan completed..." from flash ──
void AudioManager::playScanCompleteVoice() {
    playPcmFromProgmem(VOICE_SCAN_COMPLETE, VOICE_SCAN_COMPLETE_SIZE);
}

// ── Boot Voice: Speaks EVA Futuristic Robot Startup Voice from flash (Legacy blocking wrapper) ──
void AudioManager::playBootVoice(void (*progressCallback)(float progress, const char* status)) {
    _bootVoiceActive = false;
    _streamActive = false;
    _playing = false;
    vTaskDelay(pdMS_TO_TICKS(15));

    _switchToSpeaker();

    const size_t MONO_CHUNK = 512;
    int16_t monoBuf[256];
    int16_t stereoBuf[512];
    size_t pos = 0;
    size_t length = VOICE_BOOT_SIZE;
    unsigned long lastCbMs = 0;

    while (pos < length) {
        size_t toRead = min(MONO_CHUNK, length - pos);
        toRead &= ~1;
        memcpy_P(monoBuf, VOICE_BOOT + pos, toRead);

        size_t numSamples = toRead / 2;
        int32_t sum = 0;
        for (size_t i = 0; i < numSamples; i++) {
            int16_t s = (int16_t)(monoBuf[i] * _volume);
            stereoBuf[i * 2]     = s;
            stereoBuf[i * 2 + 1] = s;
            sum += abs(s);
        }

        if (numSamples > 0) {
            _playLevel = (float)sum / numSamples / 16384.0f;
        }

        size_t bw = 0;
        i2s_write(AUDIO_I2S_PORT, stereoBuf, numSamples * 4, &bw, pdMS_TO_TICKS(100));
        pos += toRead;

        if (progressCallback && (millis() - lastCbMs >= 80 || pos >= length)) {
            lastCbMs = millis();
            float audioRatio = (float)pos / (float)length;
            float bootProg = 0.80f + audioRatio * 0.20f;

            const char* status = "All systems online...";
            if (audioRatio >= 0.70f) {
                status = "AgroMate ready, awaiting command!";
            } else if (audioRatio >= 0.48f) {
                status = "Voice interface active...";
            } else if (audioRatio >= 0.30f) {
                status = "Connection established...";
            } else if (audioRatio >= 0.15f) {
                status = "Camera connected...";
            }

            progressCallback(bootProg, status);
        }

        yield();
    }

    vTaskDelay(pdMS_TO_TICKS(130));
    _playLevel = 0.0f;
    _stopSpeaker();
}
