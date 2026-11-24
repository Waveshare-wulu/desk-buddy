#pragma once

#include <cstdint>
#include "db_game_2048_engine.hpp"
#include "db_shake_detector.hpp"

namespace desk_buddy {

struct SettingsData {
    uint8_t brightness = 85;
    uint16_t dim_timeout_s = 30;      // 0 = disabled, otherwise seconds
    uint16_t sleep_timeout_s = 90;    // 0 = disabled, otherwise seconds
    bool sound_enabled = true;
    uint8_t volume = 65;
    uint8_t shake_sensitivity = static_cast<uint8_t>(ShakeSensitivity::MEDIUM);
    uint8_t offwork_hour = 18;
    uint8_t offwork_min = 0;
    uint8_t pomodoro_min = 25;
    uint8_t language = 0;             // Reserved; UI currently uses zh-CN (0)
    uint8_t work_start_hour = 9;
    uint8_t work_start_min = 0;
    bool wifi_auto_sync = true;
};

enum class TimeSyncSource : uint8_t {
    NONE = 0,
    SNTP,
    PHONE,
    MANUAL,
};

struct WifiConfig {
    char ssid[33] = {};
    char password[65] = {};
    bool configured = false;
    int64_t last_sync_epoch = 0;
    TimeSyncSource last_sync_src = TimeSyncSource::NONE;
};

class StorageService {
public:
    static StorageService &instance();

    bool init();

    const SettingsData &getSettings() const { return _settings; }
    bool updateSettings(const SettingsData &settings);
    bool isButtonGuideSeen() const;
    bool markButtonGuideSeen();
    WifiConfig loadWifiConfig() const;
    bool saveWifiConfig(const WifiConfig &config);

    uint32_t getMeritTotal() const { return _merit_total; }
    void addMerit(uint32_t delta = 1);
    bool flushMeritIfNeeded(bool force = false);

    bool load2048State(Game2048State &out_state);
    bool save2048State(const Game2048State &state);
    void clear2048SavedBoard(uint32_t keep_best_score);

    /** Erases entries in the active Desk Buddy NVS namespace. */
    bool resetAll();

private:
    StorageService() = default;
    void loadSettings();
    void loadMerit();

    bool _initialized = false;
    SettingsData _settings{};
    uint32_t _merit_total = 0;
    bool _merit_dirty = false;
    uint64_t _last_merit_flush_ms = 0;
};

}
