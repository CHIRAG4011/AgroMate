/**
 * AgroMate Audio Manager Implementation — Time-Multiplexed I2S
 * 
 * INMP441 (mic) and MAX98357A (speaker) share a single I2S bus:
 *   - BCLK = GPIO 2 (shared between mic and speaker)
 *   - WS   = GPIO 1 (shared between mic and speaker)
 *   - SD_IN  = GPIO 46 (INMP441 data → ESP32)
 *   - SD_OUT = GPIO 3  (ESP32 → MAX98357A DIN)
 * 
 * The I2S driver is uninstalled and reinstalled when switching between
 * RX (microphone) and TX (speaker) modes to avoid bus conflicts.
 */

#include "AudioManager.h"

void AudioManager::begin() {
    // Start I2S in mic mode to stabilize shared clocks/buses (fixes camera line corruption)
    _switchToMic();
    Serial.println("[Audio] Time-multiplexed I2S clocks stabilized (mic & speaker disabled)");
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
    // Microphone recording fully removed
}

void AudioManager::stopRecording() {
    _recording = false;
}

uint8_t* AudioManager::getRecordedData(size_t* outSize) {
    *outSize = 0;
    return nullptr;
}

void AudioManager::playBuffer(uint8_t* data, size_t length) {
    // Speaker playback fully removed
}

float AudioManager::getPlaybackLevel() {
    return _playLevel;
}

void AudioManager::playTestTone(uint32_t durationMs) {
    // Speaker verification tone fully removed
}
