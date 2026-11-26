#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string_view>

#include "services/clock_service.hpp"
#include "services/storage_service.hpp"

namespace desk_buddy {

enum class WifiSyncState : uint8_t {
    IDLE = 0,
    STARTING_PORTAL,
    PORTAL_ACTIVE,
    SCANNING,
    SCAN_READY,
    CONNECTING,
    SYNCING_SNTP,
    SUCCESS,
    FAILED,
};

struct WifiAccessPoint {
    char ssid[33] = {};
    int8_t rssi = 0;
    bool secured = false;
};

struct WifiTimeStatus {
    WifiSyncState state = WifiSyncState::IDLE;
    const char *message = "";
    char saved_ssid[33] = {};
    int64_t last_sync_epoch = 0;
    TimeSyncSource last_sync_src = TimeSyncSource::NONE;
    uint32_t revision = 0;
    std::array<WifiAccessPoint, 24> access_points{};
    uint8_t access_point_count = 0;
};

class WifiTimeService {
public:
    static WifiTimeService &instance();
    void init();
    WifiTimeStatus status();
    const char *hotspotName() const { return _hotspot; }

    bool startSyncSavedWifi();
    bool connectAndSync(std::string_view ssid, std::string_view password);
    bool startCaptivePortal();
    bool startApScan();
    void cancelProvisioning();
    void prepareForShutdown();
    void scheduleBootSync();
    bool setManualTime(const DateTime &time);
    bool resetSettings();

private:
    enum class Operation : uint8_t { NONE, PORTAL, SCAN, SYNC, BOOT_SYNC };
    struct Job {
        Operation operation = Operation::NONE;
        uint32_t ticket = 0;
        WifiConfig config{};
        bool save = false;
    };

    class Portal;
    bool submitLocked(Operation op, const WifiConfig &config = {}, bool save = false);
    void publish(uint32_t ticket, WifiSyncState state, const char *message);
    void runWorker();
    void runJob(const Job &job);
    void syncStation(const Job &job);
    void provision(const Job &job);
    bool isCurrent(uint32_t ticket) const { return _ticket.load() == ticket; }
    bool updateClockLocked(time_t epoch, TimeSyncSource source);
    bool saveConfigLocked(const WifiConfig &config);
    static bool makeCredentials(std::string_view ssid, std::string_view password, WifiConfig &config);

    std::mutex _mutex;
    std::condition_variable _worker_stopped;
    std::atomic<uint32_t> _ticket{0};
    Job _pending{};
    bool _worker_running = false;
    WifiConfig _config{};
    WifiTimeStatus _status{};
    char _hotspot[16] = "DeskBuddy-0000";
};

}
