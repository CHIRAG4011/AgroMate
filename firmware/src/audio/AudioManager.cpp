/**
 * AgroMate Audio Manager Implementation — Dedicated I2S Bus Setup
 * 
 * INMP441 (mic) and MAX98357A (speaker) use separate dedicated pins:
 *   - Mic: SCK = GPIO 2, WS = GPIO 1, SD_IN = GPIO 46
 *   - Speaker: BCLK = GPIO 3, WS = GPIO 35, SD_OUT = GPIO 36
 * 
 * The I2S driver switches between RX (microphone) and TX (speaker) modes.
 */

#include "AudioManager.h"
#include <math.h>

void AudioManager::begin() {
    // Start I2S in mic mode to stabilize shared clocks/buses
    _switchToMic();
    Serial.println("[Audio] I2S audio manager initialized");
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

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &micConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Mic I2S install failed: 0x%x\n", err);
        return;
    }

    err = i2s_set_pin(AUDIO_I2S_PORT, &micPins);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Mic pin config failed: 0x%x\n", err);
        return;
    }

    _currentMode = AudioMode::MIC_ACTIVE;
    Serial.println("[Audio] Switched to MIC mode");
}

void AudioManager::_switchToSpeaker() {
    if (_currentMode == AudioMode::SPEAKER_ACTIVE) return;

    // Uninstall any existing I2S driver
    _uninstallI2S();

    i2s_config_t spkConfig = {};
    spkConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    spkConfig.sample_rate = AUDIO_SAMPLE_RATE;
    spkConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    spkConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    spkConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    spkConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    spkConfig.dma_buf_count = 8;
    spkConfig.dma_buf_len = AUDIO_BUFFER_SIZE;
    spkConfig.use_apll = false;
    spkConfig.tx_desc_auto_clear = true;
    spkConfig.fixed_mclk = 0;

    i2s_pin_config_t spkPins = {};
    spkPins.bck_io_num = SPK_I2S_BCLK;
    spkPins.ws_io_num = SPK_I2S_WS;
    spkPins.data_out_num = AUDIO_I2S_SD_OUT;
    spkPins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &spkConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Speaker I2S install failed: 0x%x\n", err);
        return;
    }

    err = i2s_set_pin(AUDIO_I2S_PORT, &spkPins);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Speaker pin config failed: 0x%x\n", err);
        return;
    }

    _currentMode = AudioMode::SPEAKER_ACTIVE;
    Serial.println("[Audio] Switched to SPEAKER mode");
}

void AudioManager::_uninstallI2S() {
    if (_currentMode != AudioMode::IDLE) {
        i2s_driver_uninstall(AUDIO_I2S_PORT);
        _currentMode = AudioMode::IDLE;
    }
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
            Serial.printf("[Audio] Recording done: %u bytes\n", _recordSize);
        }
    }

    if (_playing && _playBuffer) {
        // Ensure we're in speaker mode
        if (_currentMode != AudioMode::SPEAKER_ACTIVE) {
            _switchToSpeaker();
        }

        size_t toWrite = min((size_t)AUDIO_BUFFER_SIZE, _playSize - _playPos);
        if (toWrite > 0) {
            size_t bytesWritten = 0;
            i2s_write(AUDIO_I2S_PORT, _playBuffer + _playPos, toWrite, &bytesWritten, 10);
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
            // Switch back to mic mode (ready for next recording)
            _switchToMic();
        }
    }
}

void AudioManager::startRecording(uint32_t durationMs) {
    if (_recording) return;

    _switchToMic();

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
    
    _switchToSpeaker();

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

    _switchToSpeaker();

    // Generate a 1kHz sine wave test tone at 16-bit, 16kHz sample rate
    const float frequency = 1000.0f;  // 1 kHz
    const float amplitude = 16000.0f; // ~50% volume (max is 32767)
    uint32_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    
    // Process in chunks to avoid large memory allocation
    const size_t chunkSamples = 256;
    int16_t buffer[chunkSamples];
    uint32_t samplesWritten = 0;

    while (samplesWritten < totalSamples) {
        size_t samplesToGen = min((size_t)(totalSamples - samplesWritten), chunkSamples);
        
        for (size_t i = 0; i < samplesToGen; i++) {
            float t = (float)(samplesWritten + i) / (float)AUDIO_SAMPLE_RATE;
            buffer[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
        }

        size_t bytesWritten = 0;
        i2s_write(AUDIO_I2S_PORT, buffer, samplesToGen * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        samplesWritten += samplesToGen;
    }

    // Small silence gap at the end to flush DMA buffers
    memset(buffer, 0, sizeof(buffer));
    size_t dummy = 0;
    i2s_write(AUDIO_I2S_PORT, buffer, sizeof(buffer), &dummy, portMAX_DELAY);

    Serial.printf("[Audio] Test tone complete (%u ms, %u samples)\n", durationMs, totalSamples);

    // Switch back to mic mode
    _switchToMic();
}
