#include "storage_service.hpp"

#include <algorithm>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Storage";
#ifdef DESK_BUDDY_DEVICE_SMOKE
constexpr const char *kNamespace = "desk_buddy_test";
#else
constexpr const char *kNamespace = "desk_buddy";
#endif
constexpr uint32_t kSchemaVersion = 2;
constexpr uint32_t kGameSchemaVersion = 1;
constexpr uint64_t kMeritFlushIntervalMs = 15000;

// Field order and alignment must match the settings written by firmware using schema V1.
struct SettingsDataV1 {
    uint8_t brightness;
    uint16_t dim_timeout_s;
    uint16_t sleep_timeout_s;
    bool sound_enabled;
    uint8_t volume;
    uint8_t shake_sensitivity;
    uint8_t offwork_hour;
    uint8_t offwork_min;
    uint8_t pomodoro_min;
    uint8_t language;
};

struct SettingsBlobV1 {
    uint32_t version;
    SettingsDataV1 data;
};
static_assert(sizeof(SettingsBlobV1) == 20);

struct SettingsBlobV2 {
    uint32_t version;
    SettingsData data;
};

struct Game2048BlobV1 {
    uint32_t version;
    Game2048State state;
};

constexpr uint32_t kWifiSchemaVersion = 1;
constexpr char kWifiKey[] = "wifi_cfg";
constexpr char kButtonGuideKey[] = "btn_guide_seen";

struct WifiConfigBlob {
    uint32_t version;
    WifiConfig data;
};

SettingsData sanitizeSettings(SettingsData settings)
{
    settings.brightness = std::clamp<uint8_t>(settings.brightness, 15, 100);
    settings.volume = std::min<uint8_t>(settings.volume, 100);
    settings.work_start_hour = std::min<uint8_t>(settings.work_start_hour, 23);
    settings.work_start_min = std::min<uint8_t>(settings.work_start_min, 59);
    settings.offwork_hour = std::min<uint8_t>(settings.offwork_hour, 23);
    settings.offwork_min = std::min<uint8_t>(settings.offwork_min, 59);
    settings.pomodoro_min = std::clamp<uint8_t>(settings.pomodoro_min, 1, 120);
    if (settings.shake_sensitivity > 2) {
        settings.shake_sensitivity = static_cast<uint8_t>(ShakeSensitivity::MEDIUM);
    }
    return settings;
}

}

StorageService &StorageService::instance()
{
    static StorageService s_inst;
    return s_inst;
}

bool StorageService::init()
{
    if (_initialized) {
        return true;
    }

    esp_err_t err = nvs_flash_init();
#ifndef DESK_BUDDY_DEVICE_SMOKE
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase (err=%s), reinitializing", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    _initialized = true;
    loadSettings();
    loadMerit();
    ESP_LOGI(TAG, "Storage initialized (merit=%lu, brightness=%u, sound=%d)",
             static_cast<unsigned long>(_merit_total), _settings.brightness, _settings.sound_enabled);
    return true;
}

void StorageService::loadSettings()
{
    _settings = SettingsData{};
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    SettingsBlobV2 blob{};
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(h, "settings", &blob, &len);
    nvs_close(h);

    if (err != ESP_OK) {
        return;
    }
    if (blob.version == kSchemaVersion && len == sizeof(blob)) {
        _settings = sanitizeSettings(blob.data);
    } else if (blob.version == 1 && len == sizeof(SettingsBlobV1)) {
        SettingsBlobV1 legacy{};
        std::memcpy(&legacy, &blob, sizeof(legacy));
        const auto &old = legacy.data;
        _settings = sanitizeSettings({
            .brightness = old.brightness,
            .dim_timeout_s = old.dim_timeout_s,
            .sleep_timeout_s = old.sleep_timeout_s,
            .sound_enabled = old.sound_enabled,
            .volume = old.volume,
            .shake_sensitivity = old.shake_sensitivity,
            .offwork_hour = old.offwork_hour,
            .offwork_min = old.offwork_min,
            .pomodoro_min = old.pomodoro_min,
            .language = old.language,
        });
        if (updateSettings(_settings)) {
            ESP_LOGI(TAG, "Migrated settings from schema V1 to V2");
        } else {
            ESP_LOGW(TAG, "Could not persist migrated settings; migration will retry at next boot");
        }
    }
}

bool StorageService::updateSettings(const SettingsData &settings)
{
    if (!_initialized) {
        return false;
    }

    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open settings failed: %s", esp_err_to_name(err));
        return false;
    }

    const SettingsBlobV2 blob{kSchemaVersion, sanitizeSettings(settings)};
    err = nvs_set_blob(h, "settings", &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        _settings = blob.data;
    }
    return err == ESP_OK;
}

bool StorageService::isButtonGuideSeen() const
{
    nvs_handle_t h = 0;
    if (!_initialized || nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    uint8_t seen = 0;
    const auto result = nvs_get_u8(h, kButtonGuideKey, &seen);
    nvs_close(h);
    return result == ESP_OK && seen == 1;
}

bool StorageService::markButtonGuideSeen()
{
    nvs_handle_t h = 0;
    if (!_initialized || nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    auto result = nvs_set_u8(h, kButtonGuideKey, 1);
    if (result == ESP_OK) {
        result = nvs_commit(h);
    }
    nvs_close(h);
    return result == ESP_OK;
}

void StorageService::loadMerit()
{
    _merit_total = 0;
    _merit_dirty = false;
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint32_t val = 0;
    if (nvs_get_u32(h, "merit_total", &val) == ESP_OK) {
        _merit_total = val;
    }
    nvs_close(h);
}

WifiConfig StorageService::loadWifiConfig() const
{
    nvs_handle_t h = 0;
    if (!_initialized || nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return {};
    }
    WifiConfigBlob blob{};
    size_t len = sizeof(blob);
    const esp_err_t err = nvs_get_blob(h, kWifiKey, &blob, &len);
    nvs_close(h);
    if (err != ESP_OK || len != sizeof(blob) || blob.version != kWifiSchemaVersion) {
        return {};
    }
    blob.data.ssid[sizeof(blob.data.ssid) - 1] = '\0';
    blob.data.password[sizeof(blob.data.password) - 1] = '\0';
    blob.data.configured = blob.data.configured && blob.data.ssid[0] != '\0';
    return blob.data;
}

bool StorageService::saveWifiConfig(const WifiConfig &config)
{
    nvs_handle_t h = 0;
    if (!_initialized || nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    const WifiConfigBlob blob{kWifiSchemaVersion, config};
    esp_err_t err = nvs_set_blob(h, kWifiKey, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err == ESP_OK;
}

void StorageService::addMerit(uint32_t delta)
{
    _merit_total += delta;
    _merit_dirty = true;
}

bool StorageService::flushMeritIfNeeded(bool force)
{
    if (!_merit_dirty) {
        return true;
    }
    if (!_initialized) {
        return false;
    }
    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    if (!force && _last_merit_flush_ms != 0 && (now_ms - _last_merit_flush_ms) < kMeritFlushIntervalMs) {
        return true;
    }

    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_u32(h, "merit_total", _merit_total);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        _merit_dirty = false;
        _last_merit_flush_ms = now_ms;
        ESP_LOGD(TAG, "Flushed merit_total=%lu", static_cast<unsigned long>(_merit_total));
    }
    return err == ESP_OK;
}

bool StorageService::load2048State(Game2048State &out_state)
{
    if (!_initialized) {
        return false;
    }
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    Game2048BlobV1 blob{};
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(h, "g2048_v1", &blob, &len);
    nvs_close(h);

    if (err == ESP_OK && len == sizeof(blob) && blob.version == kGameSchemaVersion) {
        out_state = blob.state;
        return true;
    }
    return false;
}

bool StorageService::save2048State(const Game2048State &state)
{
    if (!_initialized) {
        return false;
    }
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    Game2048BlobV1 blob{kGameSchemaVersion, state};
    esp_err_t err = nvs_set_blob(h, "g2048_v1", &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err == ESP_OK;
}

void StorageService::clear2048SavedBoard(uint32_t keep_best_score)
{
    Game2048State empty{};
    empty.best_score = keep_best_score;
    save2048State(empty);
}

bool StorageService::resetAll()
{
    if (!_initialized) {
        return false;
    }
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        _settings = SettingsData{};
        _merit_total = 0;
        _merit_dirty = false;
    }
    return err == ESP_OK;
}

}
