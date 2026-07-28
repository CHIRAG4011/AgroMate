/**
 * AgroMate Audio Manager Implementation — Dual Concurrent I2S Setup
 * 
 * INMP441 (mic) and MAX98357A (speaker) use separate dedicated pins:
 *   - Mic (I2S_NUM_0): SCK = GPIO 2, WS = GPIO 1, SD_IN = GPIO 46
 *   - Speaker (I2S_NUM_1): BCLK = GPIO 3, WS = GPIO 35, SD_OUT = GPIO 36
 * 
 * Both drivers remain installed 100% of the time. Zero driver switching!
 */

#include "AudioManager.h"
#include <math.h>

void AudioManager::begin() {
    _initMicI2S();
    _initSpeakerI2S();
    Serial.println("[Audio] Dual concurrent I2S audio manager initialized (Mic: I2S_0, Speaker: I2S_1)");
}

void AudioManager::_initMicI2S() {
    i2s_config_t micConfig = {};
    micConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    micConfig.sample_rate = AUDIO_SAMPLE_RATE;
    micConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    micConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    micConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    micConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    micConfig.dma_buf_count = 8;
    micConfig.dma_buf_len = AUDIO_BUFFER_SIZE;
    micConfig.use_apll = false;
    micConfig.tx_desc_auto_clear = false;
    micConfig.fixed_mclk = 0;

    i2s_pin_config_t micPins = {};
    micPins.bck_io_num = MIC_I2S_BCLK;
    micPins.ws_io_num = MIC_I2S_WS;
    micPins.data_out_num = I2S_PIN_NO_CHANGE;
    micPins.data_in_num = AUDIO_I2S_SD_IN;

    esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &micConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Mic I2S install failed: 0x%x\n", err);
        return;
    }

    err = i2s_set_pin(MIC_I2S_PORT, &micPins);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Mic pin config failed: 0x%x\n", err);
        return;
    }

    Serial.println("[Audio] Dedicated MIC I2S_0 initialized");
}

void AudioManager::_initSpeakerI2S() {
    i2s_config_t spkConfig = {};
    spkConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    spkConfig.sample_rate = AUDIO_SAMPLE_RATE;
    spkConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    spkConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    spkConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    spkConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    spkConfig.dma_buf_count = 16;
    spkConfig.dma_buf_len = AUDIO_BUFFER_SIZE;
    spkConfig.use_apll = false;
    spkConfig.tx_desc_auto_clear = true; // Transmits silence auto when idle (no buzzing!)
    spkConfig.fixed_mclk = 0;

    i2s_pin_config_t spkPins = {};
    spkPins.bck_io_num = SPK_I2S_BCLK;
    spkPins.ws_io_num = SPK_I2S_WS;
    spkPins.data_out_num = AUDIO_I2S_SD_OUT;
    spkPins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(SPK_I2S_PORT, &spkConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Speaker I2S install failed: 0x%x\n", err);
        return;
    }

    err = i2s_set_pin(SPK_I2S_PORT, &spkPins);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Speaker pin config failed: 0x%x\n", err);
        return;
    }

    Serial.println("[Audio] Dedicated SPEAKER I2S_1 initialized");
}

void AudioManager::update() {
    if (_recording) {
        size_t bytesRead = 0;
        size_t maxRecordBytes = AUDIO_SAMPLE_RATE * 2 * (_recordDuration / 1000);
        size_t remaining = maxRecordBytes - _recordPos;
        size_t toRead = min(remaining, (size_t)AUDIO_BUFFER_SIZE);

        if (toRead > 0 && _recordBuffer) {
            i2s_read(MIC_I2S_PORT, _recordBuffer + _recordPos, toRead, &bytesRead, 10);
            _recordPos += bytesRead;
        }

        if (millis() - _recordStart >= _recordDuration || remaining == 0) {
            _recording = false;
            _recordSize = _recordPos;
            Serial.printf("[Audio] Recording done: %u bytes\n", _recordSize);
        }
    }

    if (_playing && _playBuffer) {
        size_t toWrite = min((size_t)AUDIO_BUFFER_SIZE, _playSize - _playPos);
        if (toWrite > 0) {
            size_t bytesWritten = 0;
            i2s_write(SPK_I2S_PORT, _playBuffer + _playPos, toWrite, &bytesWritten, 10);
            _playPos += bytesWritten;

            // Calculate playback level for mouth animation
            int16_t* samples = (int16_t*)(_playBuffer + _playPos - bytesWritten);
            int32_t sum = 0;
            size_t sampleCount = bytesWritten / 2;
            if (sampleCount > 0) {
                for (size_t i = 0; i < sampleCount; i++) {
                    sum += abs(samples[i]);
                }
                _playLevel = (float)sum / sampleCount / 32768.0f;
            }
        } else {
            _playing = false;
            _playLevel = 0;
            Serial.println("[Audio] Playback complete");
        }
    }

    if (_streamActive) {
        if (_ringCount > 0) {
            _drainRingToI2S(256);
        } else if (_streamFinished) {
            _streamActive = false;
            _streamFinished = false;
            _playLevel = 0.0f;
            Serial.println("[Audio] Streaming audio playback complete");
        }
    }
}

void AudioManager::startAudioStream() {
    Serial.println("[Audio] Starting streaming audio mode...");
    _ringHead = 0;
    _ringTail = 0;
    _ringCount = 0;
    _streamActive = true;
    _streamFinished = false;
    _playing = false;
    _playLevel = 0.0f;
}

void AudioManager::writeAudioStream(const uint8_t* data, size_t length) {
    if (!data || length < 2 || !_streamActive) return;

    size_t sampleCount = length / 2;
    const int16_t* incomingSamples = (const int16_t*)data;

    for (size_t i = 0; i < sampleCount; i++) {
        if (_ringCount >= STREAM_RING_SAMPLES) {
            _drainRingToI2S(256);
        }
        _ringBuffer[_ringHead] = incomingSamples[i];
        _ringHead = (_ringHead + 1) % STREAM_RING_SAMPLES;
        _ringCount++;
    }
}

void AudioManager::stopAudioStream() {
    Serial.println("[Audio] Audio stream finished receiving, draining buffer...");
    _streamFinished = true;
}

void AudioManager::_drainRingToI2S(size_t maxSamples) {
    if (_ringCount == 0) return;

    size_t toProcess = min(maxSamples, (size_t)_ringCount);
    if (toProcess == 0) return;

    size_t chunk1 = min(toProcess, STREAM_RING_SAMPLES - _ringTail);
    size_t samplesToProcess = min((size_t)256, chunk1);
    if (samplesToProcess == 0) return;

    int16_t stereoBuf[512]; // 256 stereo pairs = 512 int16s

    for (size_t i = 0; i < samplesToProcess; i++) {
        int32_t val = ((int32_t)_ringBuffer[_ringTail + i] * 13) / 8; // +4.2dB gain boost
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        int16_t s = (int16_t)val;
        stereoBuf[i * 2]     = s; // Left
        stereoBuf[i * 2 + 1] = s; // Right
    }

    size_t bytesWritten = 0;
    i2s_write(SPK_I2S_PORT, stereoBuf, samplesToProcess * 4, &bytesWritten, 10);

    size_t samplesConsumed = bytesWritten / 4;
    if (samplesConsumed > 0) {
        _ringTail = (_ringTail + samplesConsumed) % STREAM_RING_SAMPLES;
        _ringCount -= samplesConsumed;
    }

    if (samplesConsumed > 0) {
        int32_t sum = 0;
        for (size_t s = 0; s < samplesConsumed; s++) {
            sum += abs(stereoBuf[s * 2]);
        }
        _playLevel = (float)sum / samplesConsumed / 32768.0f;
    }
}

void AudioManager::startRecording(uint32_t durationMs) {
    if (_recording) return;

    size_t bufSize = AUDIO_SAMPLE_RATE * 2 * (durationMs / 1000 + 1);
    if (_recordBuffer) free(_recordBuffer);
    _recordBuffer = (uint8_t*)malloc(bufSize);
    if (!_recordBuffer) {
        Serial.println("[Audio] Failed to allocate recording buffer");
        return;
    }

    _recordDuration = durationMs;
    _recordPos = 0;
    _recordSize = 0;
    _recordStart = millis();
    _recording = true;
    Serial.printf("[Audio] Recording started (%u ms)\n", durationMs);
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

    _playBuffer = data;
    _playSize = length;
    _playPos = 0;
    _playing = true;
    _playLevel = 0;
    Serial.printf("[Audio] Playing buffer: %u bytes\n", length);
}

float AudioManager::getPlaybackLevel() {
    return _playLevel;
}

void AudioManager::playTestTone(uint32_t durationMs) {
    Serial.println("[Audio] Playing test tone...");
    _playToneBlocking(1000.0f, durationMs, 16000.0f);
}

// ── Helper: Play a single blocking tone (Stereo I2S) ──
void AudioManager::_playToneBlocking(float freqHz, uint32_t durationMs, float amplitude) {
    uint32_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    const size_t chunkSamples = 256;
    int16_t stereoBuf[chunkSamples * 2];
    uint32_t written = 0;

    while (written < totalSamples) {
        size_t n = min((size_t)(totalSamples - written), chunkSamples);
        for (size_t i = 0; i < n; i++) {
            float t = (float)(written + i) / (float)AUDIO_SAMPLE_RATE;
            int16_t sample = (int16_t)(amplitude * sinf(2.0f * M_PI * freqHz * t));
            stereoBuf[i * 2]     = sample;
            stereoBuf[i * 2 + 1] = sample;
        }
        size_t bw = 0;
        i2s_write(SPK_I2S_PORT, stereoBuf, n * 4, &bw, portMAX_DELAY);
        written += n;
    }
}

// ── Scan Progress: Short high-pitched beep ──
void AudioManager::playScanProgressBeep() {
    _playToneBlocking(880.0f, 80, 16000.0f);  // A5 - 80ms, clean tone

    int16_t silence[128] = {0};
    size_t d = 0;
    i2s_write(SPK_I2S_PORT, silence, sizeof(silence), &d, portMAX_DELAY);
}

// ── Play pre-recorded PCM from PROGMEM flash (Stereo I2S) ──
void AudioManager::playPcmFromProgmem(const uint8_t* data, size_t length) {
    Serial.printf("[Audio] Playing PROGMEM voice clip (%u bytes, %.1fs)\n", length, (float)length / 32000.0f);

    const size_t MONO_CHUNK = 512;  // Read 512 bytes (256 samples) from flash
    int16_t monoBuf[256];
    int16_t stereoBuf[512];        // 1024 bytes stereo
    size_t pos = 0;

    while (pos < length) {
        size_t toRead = min(MONO_CHUNK, length - pos);
        toRead &= ~1;
        memcpy_P(monoBuf, data + pos, toRead);
        
        size_t numSamples = toRead / 2;
        for (size_t i = 0; i < numSamples; i++) {
            int32_t val = ((int32_t)monoBuf[i] * 13) / 8; // +4.2dB gain boost
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            int16_t s = (int16_t)val;
            stereoBuf[i * 2]     = s;
            stereoBuf[i * 2 + 1] = s;
        }

        size_t bw = 0;
        i2s_write(SPK_I2S_PORT, stereoBuf, numSamples * 4, &bw, portMAX_DELAY);
        pos += toRead;
    }

    memset(stereoBuf, 0, sizeof(stereoBuf));
    size_t d = 0;
    i2s_write(SPK_I2S_PORT, stereoBuf, sizeof(stereoBuf), &d, portMAX_DELAY);

    Serial.println("[Audio] Voice clip complete");
}

// ── Scan Start Voice: Speaks "Scanning started..." from flash ──
#include "voices/voice_scan_start.h"
void AudioManager::playScanStartVoice() {
    Serial.println("[Audio] Playing 'Scan Started' voice");
    playPcmFromProgmem(VOICE_SCAN_START, VOICE_SCAN_START_SIZE);
}

// ── Scan Complete Voice: Speaks "Scan completed..." from flash ──
#include "voices/voice_scan_complete.h"
void AudioManager::playScanCompleteVoice() {
    Serial.println("[Audio] Playing 'Scan Complete' voice");
    playPcmFromProgmem(VOICE_SCAN_COMPLETE, VOICE_SCAN_COMPLETE_SIZE);
}
