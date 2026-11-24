#include "sound_service.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Sound";
constexpr uint32_t kSampleRate = 16000;
constexpr float kTwoPi = 6.28318530718f;
}

SoundService &SoundService::instance()
{
    static SoundService s_inst;
    return s_inst;
}

bool SoundService::init(bool enabled, uint8_t volume)
{
    _enabled = enabled;
    _volume = std::clamp<uint8_t>(volume, 0, 100);

    esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
    if (!spk) {
        ESP_LOGW(TAG, "ES8311 speaker init returned NULL, running silently");
        _hw_ready = false;
        return false;
    }

    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = kSampleRate;
    fs.channel = 2;
    fs.bits_per_sample = 16;

    if (esp_codec_dev_open(spk, &fs) != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 open failed, running silently");
        _hw_ready = false;
        return false;
    }

    esp_codec_dev_set_out_vol(spk, _volume);
    esp_codec_dev_set_out_mute(spk, !_enabled);

    _codec_handle = spk;
    _queue = xQueueCreate(2, sizeof(SoundEffect));
    if (!_queue) {
        _hw_ready = false;
        return false;
    }

    if (xTaskCreate(audioTaskEntry, "db_audio", 6144, this, 3, nullptr) != pdPASS) {
        _hw_ready = false;
        return false;
    }

    _hw_ready = true;
    ESP_LOGI(TAG, "Sound service ready (ES8311, enabled=%d, vol=%u)", _enabled.load(), _volume.load());
    return true;
}

void SoundService::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (_hw_ready && _codec_handle) {
        auto *spk = static_cast<esp_codec_dev_handle_t>(_codec_handle);
        esp_codec_dev_set_out_mute(spk, !enabled);
        if (enabled) {
            esp_codec_dev_set_out_vol(spk, _volume);
            gpio_set_level(BSP_POWER_AMP_IO, 1);
        }
    }
}

void SoundService::setVolume(uint8_t volume)
{
    _volume = std::clamp<uint8_t>(volume, 0, 100);
    if (_hw_ready && _codec_handle && _enabled) {
        auto *spk = static_cast<esp_codec_dev_handle_t>(_codec_handle);
        esp_codec_dev_set_out_vol(spk, _volume);
    }
}

void SoundService::prepareForShutdown()
{
    setEnabled(false);
    if (_queue) {
        xQueueReset(static_cast<QueueHandle_t>(_queue));
    }
    while (_playing.load()) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    gpio_set_level(BSP_POWER_AMP_IO, 0);
    gpio_set_direction(BSP_POWER_AMP_IO, GPIO_MODE_OUTPUT);
}

void SoundService::play(SoundEffect effect)
{
    if (!_hw_ready || !_enabled || _volume == 0 || !_queue) {
        return;
    }
    xQueueSend(static_cast<QueueHandle_t>(_queue), &effect, 0);
}

void SoundService::audioTaskEntry(void *arg)
{
    static_cast<SoundService *>(arg)->audioTaskLoop();
}

void SoundService::audioTaskLoop()
{
    SoundEffect effect = SoundEffect::CLICK;
    while (true) {
        if (xQueueReceive(static_cast<QueueHandle_t>(_queue), &effect, portMAX_DELAY) == pdTRUE) {
            _playing = true;
            if (_enabled && _volume > 0) {
                playSynthesized(effect);
            }
            _playing = false;
        }
    }
}

void SoundService::playSynthesized(SoundEffect effect)
{
    if (!_codec_handle) {
        return;
    }
    auto *spk = static_cast<esp_codec_dev_handle_t>(_codec_handle);

    int duration_ms = 60;
    switch (effect) {
        case SoundEffect::CLICK:     duration_ms = 18; break;
        case SoundEffect::WOOD_FISH: duration_ms = 95; break;
        case SoundEffect::COIN:      duration_ms = 140; break;
        case SoundEffect::DICE:      duration_ms = 150; break;
        case SoundEffect::COMPLETE:  duration_ms = 260; break;
    }

    const int frames = (static_cast<int>(kSampleRate) * duration_ms) / 1000;
    std::vector<int16_t> pcm(static_cast<size_t>(frames) * 2, 0);

    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        float sample = 0.0f;

        switch (effect) {
            case SoundEffect::CLICK: {
                const float env = std::exp(-t * 160.0f);
                sample = env * std::sin(kTwoPi * 1200.0f * t) * 0.45f;
                break;
            }
            case SoundEffect::WOOD_FISH: {
                const float attack = (t < 0.002f) ? (t / 0.002f) : 1.0f;
                const float env1 = std::exp(-t * 36.0f);
                const float env2 = std::exp(-t * 68.0f);
                const float f1 = 440.0f + 90.0f * std::exp(-t * 80.0f);
                const float f2 = 860.0f;
                sample = attack * (0.72f * env1 * std::sin(kTwoPi * f1 * t) +
                                   0.28f * env2 * std::sin(kTwoPi * f2 * t));
                break;
            }
            case SoundEffect::COIN: {
                const float env = std::exp(-t * 22.0f);
                sample = env * (0.6f * std::sin(kTwoPi * 1568.0f * t) +
                                0.4f * std::sin(kTwoPi * 2349.0f * t));
                break;
            }
            case SoundEffect::DICE: {
                const float offsets[3] = {0.0f, 0.045f, 0.095f};
                const float freqs[3] = {620.0f, 740.0f, 560.0f};
                for (int k = 0; k < 3; ++k) {
                    const float dt = t - offsets[k];
                    if (dt >= 0.0f && dt < 0.035f) {
                        sample += 0.65f * std::exp(-dt * 95.0f) * std::sin(kTwoPi * freqs[k] * dt);
                    }
                }
                break;
            }
            case SoundEffect::COMPLETE: {
                if (t < 0.12f) {
                    const float env = std::exp(-t * 14.0f);
                    sample = 0.65f * env * std::sin(kTwoPi * 523.25f * t);
                } else {
                    const float dt = t - 0.12f;
                    const float env = std::exp(-dt * 14.0f);
                    sample = 0.70f * env * std::sin(kTwoPi * 783.99f * dt);
                }
                break;
            }
        }

        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        const int16_t s16 = static_cast<int16_t>(clamped * 22000.0f);
        pcm[static_cast<size_t>(i) * 2] = s16;
        pcm[static_cast<size_t>(i) * 2 + 1] = s16;
    }

    if (_enabled) {
        esp_codec_dev_write(spk, pcm.data(), static_cast<int>(pcm.size() * sizeof(int16_t)));
    }
}

}
