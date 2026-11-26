#include "services/wifi_time_service.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/time.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

namespace desk_buddy {

namespace {

constexpr char TAG[] = "DB:WifiTime";
constexpr char kPortalUrl[] = "http://192.168.4.1/";
constexpr char kStartError[] = "Wi-Fi 启动失败，请重试";
constexpr char kRtcError[] = "板载时钟写入失败，请重试";
constexpr char kSaveError[] = "配置保存失败，请重试";
constexpr int64_t kPortalIdleUs = 5 * 60 * 1000000LL;
constexpr uint32_t kWifiWorkerStackBytes = 6656;
constexpr int64_t kStationTimeoutUs = 15 * 1000000LL;
constexpr int64_t kSntpTimeoutUs = 10 * 1000000LL;

extern const char portal_html_start[] asm("_binary_wifi_time_portal_html_start");

void sleepMs(unsigned ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

bool isProvisioning(WifiSyncState state)
{
    return state == WifiSyncState::STARTING_PORTAL || state == WifiSyncState::PORTAL_ACTIVE ||
           state == WifiSyncState::SCANNING;
}

bool isConnecting(WifiSyncState state)
{
    return state == WifiSyncState::CONNECTING || state == WifiSyncState::SYNCING_SNTP;
}

bool reportError(esp_err_t err, const char *action)
{
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s: %s (free internal=%zu)", action, esp_err_to_name(err),
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
    return err == ESP_OK;
}

class Radio {
public:
    ~Radio()
    {
        stop();
    }

    bool start(const char *hotspot = nullptr)
    {
        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return reportError(err, "esp_netif_init");
        }
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return reportError(err, "esp_event_loop_create_default");
        }

        ESP_LOGI(TAG, "Starting Wi-Fi (free internal=%zu, largest=%zu)",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
        init.nvs_enable = 0;
        if (!reportError(esp_wifi_init(&init), "esp_wifi_init")) {
            return false;
        }
        _initialized = true;
        if (!reportError(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage")) {
            return false;
        }
        const esp_netif_config_t sta_config = ESP_NETIF_DEFAULT_WIFI_STA();
        _sta = esp_netif_new(&sta_config);
        if (!_sta || !reportError(esp_netif_attach_wifi_station(_sta), "attach station") ||
            !reportError(esp_wifi_set_default_wifi_sta_handlers(), "station handlers")) {
            return false;
        }
        if (!reportError(esp_wifi_set_mode(hotspot ? WIFI_MODE_APSTA : WIFI_MODE_STA), "esp_wifi_set_mode")) {
            return false;
        }
        if (hotspot && !configureHotspot(hotspot)) {
            return false;
        }
        _started = reportError(esp_wifi_start(), "esp_wifi_start");
        return _started;
    }

    void stop()
    {
        if (_started) {
            reportError(esp_wifi_stop(), "esp_wifi_stop");
            _started = false;
        }
        if (_ap) {
            esp_netif_destroy_default_wifi(_ap);
            _ap = nullptr;
        }
        if (_sta) {
            esp_netif_destroy_default_wifi(_sta);
            _sta = nullptr;
        }
        if (_initialized) {
            reportError(esp_wifi_deinit(), "esp_wifi_deinit");
            _initialized = false;
            ESP_LOGI(TAG, "Wi-Fi stopped (free internal=%zu, largest=%zu)",
                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        }
    }

    bool connect(const WifiConfig &config)
    {
        wifi_config_t station{};
        std::memcpy(station.sta.ssid, config.ssid, std::strlen(config.ssid));
        std::memcpy(station.sta.password, config.password, std::strlen(config.password));
        station.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        station.sta.failure_retry_cnt = 1;
        station.sta.threshold.authmode = config.password[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
        station.sta.pmf_cfg.capable = true;
        return reportError(esp_wifi_set_config(WIFI_IF_STA, &station), "station configuration") &&
               reportError(esp_wifi_connect(), "esp_wifi_connect");
    }

    bool hasIp() const
    {
        esp_netif_ip_info_t ip{};
        return esp_netif_is_netif_up(_sta) && esp_netif_get_ip_info(_sta, &ip) == ESP_OK && ip.ip.addr != 0;
    }

private:
    bool configureHotspot(const char *hotspot)
    {
        const esp_netif_config_t ap_config = ESP_NETIF_DEFAULT_WIFI_AP();
        _ap = esp_netif_new(&ap_config);
        if (!_ap || !reportError(esp_netif_attach_wifi_ap(_ap), "attach SoftAP") ||
            !reportError(esp_wifi_set_default_wifi_ap_handlers(), "SoftAP handlers")) {
            return false;
        }

        esp_netif_dns_info_t dns{};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = inet_addr("192.168.4.1");
        uint8_t offer_dns = 1;
        if (!reportError(esp_netif_set_dns_info(_ap, ESP_NETIF_DNS_MAIN, &dns), "SoftAP DNS address") ||
            !reportError(esp_netif_dhcps_option(_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                               &offer_dns, sizeof(offer_dns)), "SoftAP DHCP DNS")) {
            return false;
        }
        wifi_config_t config{};
        const size_t length = std::strlen(hotspot);
        std::memcpy(config.ap.ssid, hotspot, length);
        config.ap.ssid_len = length;
        config.ap.channel = 1;
        config.ap.authmode = WIFI_AUTH_OPEN;
        config.ap.max_connection = 2;
        return reportError(esp_wifi_set_config(WIFI_IF_AP, &config), "SoftAP configuration");
    }

    bool _initialized = false;
    bool _started = false;
    esp_netif_t *_sta = nullptr;
    esp_netif_t *_ap = nullptr;
};

bool scanNetworks(WifiTimeStatus &result)
{
    if (!reportError(esp_wifi_scan_start(nullptr, true), "esp_wifi_scan_start")) {
        return false;
    }

    uint16_t count = 0;
    if (!reportError(esp_wifi_scan_get_ap_num(&count), "esp_wifi_scan_get_ap_num")) {
        esp_wifi_clear_ap_list();
        return false;
    }
    auto &access_points = result.access_points;
    const auto weakerSignal = [](const WifiAccessPoint &left, const WifiAccessPoint &right) {
        return left.rssi < right.rssi;
    };
    bool complete = true;
    for (uint16_t i = 0; i < count; ++i) {
        wifi_ap_record_t record{};
        if (!reportError(esp_wifi_scan_get_ap_record(&record), "esp_wifi_scan_get_ap_record")) {
            complete = false;
            break;
        }
        const auto *ssid = reinterpret_cast<const char *>(record.ssid);
        if (!ssid[0] || record.authmode == WIFI_AUTH_WEP) {
            continue;
        }

        const auto end = access_points.begin() + result.access_point_count;
        auto entry = std::find_if(access_points.begin(), end, [ssid](const WifiAccessPoint &ap) {
            return std::strncmp(ap.ssid, ssid, 32) == 0;
        });
        if (entry == end && end != access_points.end()) {
            ++result.access_point_count;
        } else {
            if (entry == end) {
                entry = std::min_element(access_points.begin(), end, weakerSignal);
            }
            if (record.rssi <= entry->rssi) {
                continue;
            }
        }

        std::memcpy(entry->ssid, ssid, 32);
        entry->rssi = record.rssi;
        entry->secured = record.authmode != WIFI_AUTH_OPEN;
    }
    esp_wifi_clear_ap_list();

    std::sort(access_points.begin(), access_points.begin() + result.access_point_count,
              [](const WifiAccessPoint &left, const WifiAccessPoint &right) {
                  return left.rssi > right.rssi;
              });
    return complete;
}

bool localTime(time_t epoch, struct tm &local)
{
    return localtime_r(&epoch, &local) && local.tm_year >= 124 && local.tm_year <= 199;
}

void answerDns(int socket_fd)
{
    uint8_t packet[512];
    sockaddr_in client{};
    socklen_t client_size = sizeof(client);
    const auto count = recvfrom(socket_fd, packet, sizeof(packet) - 16, MSG_DONTWAIT,
                                reinterpret_cast<sockaddr *>(&client), &client_size);
    if (count < 17 || (packet[2] & 0xF8) || packet[4] != 0 || packet[5] != 1) {
        return;
    }

    size_t pos = 12;
    while (pos < static_cast<size_t>(count) && packet[pos] != 0) {
        if (packet[pos] > 63) {
            return;
        }
        pos += packet[pos] + 1;
    }
    if (pos + 5 > static_cast<size_t>(count)) {
        return;
    }
    const bool address_query = packet[pos + 1] == 0 && packet[pos + 2] == 1 &&
                               packet[pos + 3] == 0 && packet[pos + 4] == 1;
    pos += 5;
    packet[2] = 0x84 | (packet[2] & 1);
    packet[3] = 0;
    std::memset(packet + 6, 0, 6);
    if (address_query) {
        packet[7] = 1;
        constexpr uint8_t answer[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4, 192, 168, 4, 1};
        std::memcpy(packet + pos, answer, sizeof(answer));
        pos += sizeof(answer);
    }
    sendto(socket_fd, packet, pos, 0, reinterpret_cast<sockaddr *>(&client), client_size);
}

constexpr size_t kPortalBodyBufferSize = 1 + 32 + 64 + 1;

bool readPortalBody(httpd_req_t *req, char (&body)[kPortalBodyBufferSize])
{
    char marker[2]{};
    if (req->content_len == 0 || req->content_len >= sizeof(body) ||
        httpd_req_get_hdr_value_str(req, "X-DeskBuddy", marker, sizeof(marker)) != ESP_OK ||
        marker[0] != '1') {
        return false;
    }
    size_t received = 0;
    while (received < req->content_len) {
        const int bytes = httpd_req_recv(req, body + received, req->content_len - received);
        if (bytes <= 0) {
            return false;
        }
        received += bytes;
    }
    body[received] = '\0';
    return true;
}

esp_err_t portalClosing(httpd_req_t *req)
{
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "Portal is closing");
}

std::string jsonString(const char *value)
{
    std::string encoded = "\"";
    for (const unsigned char ch : std::string_view(value)) {
        if (ch < 0x20) {
            char escaped[7];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
            encoded += escaped;
        } else {
            if (ch == '"' || ch == '\\') {
                encoded += '\\';
            }
            encoded += static_cast<char>(ch);
        }
    }
    return encoded + '"';
}

}

class WifiTimeService::Portal {
public:
    Portal(WifiTimeService &owner, uint32_t ticket, const WifiTimeStatus &networks)
        : _owner(owner), _ticket(ticket), _networks(networks), _last_activity(esp_timer_get_time()) {}

    ~Portal()
    {
        if (_http) {
            httpd_stop(_http);
        }
        if (_dns >= 0) {
            close(_dns);
        }
    }

    bool start()
    {
        _dns = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_dns < 0) {
            ESP_LOGW(TAG, "Unable to create portal DNS socket: errno %d", errno);
            return false;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);
        addr.sin_addr.s_addr = inet_addr("192.168.4.1");
        if (bind(_dns, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            ESP_LOGW(TAG, "Unable to bind portal DNS socket: errno %d", errno);
            return false;
        }

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.lru_purge_enable = true;
        config.max_open_sockets = 4;
        config.recv_wait_timeout = 2;
        config.send_wait_timeout = 2;
        config.uri_match_fn = httpd_uri_match_wildcard;
        if (!reportError(httpd_start(&_http, &config), "httpd_start")) {
            return false;
        }
        const httpd_uri_t routes[] = {
            {.uri = "/api/networks", .method = HTTP_GET, .handler = getNetworks, .user_ctx = this},
            {.uri = "/api/sync_time", .method = HTTP_POST, .handler = postTime, .user_ctx = this},
            {.uri = "/api/wifi", .method = HTTP_POST, .handler = postWifi, .user_ctx = this},
            {.uri = "/*", .method = HTTP_GET, .handler = getPage, .user_ctx = this},
        };
        for (const auto &route : routes) {
            if (!reportError(httpd_register_uri_handler(_http, &route), "register portal route")) {
                return false;
            }
        }
        return true;
    }

    enum class Result { CANCELLED, EXPIRED, PHONE_SYNCED, ROUTER_SUBMITTED };

    Result wait(WifiConfig &router)
    {
        while (_owner.isCurrent(_ticket)) {
            {
                const std::lock_guard lock(_mutex);
                const int64_t now = esp_timer_get_time();
                if (_ready_at != 0 && now >= _ready_at) {
                    router = _router;
                    return _result;
                }
                if (now - _last_activity >= kPortalIdleUs) {
                    return Result::EXPIRED;
                }
            }
            fd_set sockets;
            FD_ZERO(&sockets);
            FD_SET(_dns, &sockets);
            timeval timeout{.tv_sec = 0, .tv_usec = 100000};
            if (select(_dns + 1, &sockets, nullptr, nullptr, &timeout) > 0) {
                answerDns(_dns);
            }
        }
        return Result::CANCELLED;
    }

private:
    static Portal &from(httpd_req_t *req) { return *static_cast<Portal *>(req->user_ctx); }

    static esp_err_t getPage(httpd_req_t *req)
    {
        if (std::strcmp(req->uri, "/") != 0) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", kPortalUrl);
            return httpd_resp_send(req, nullptr, 0);
        }
        auto &portal = from(req);
        {
            const std::lock_guard lock(portal._mutex);
            portal._last_activity = esp_timer_get_time();
        }
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        return httpd_resp_sendstr(req, portal_html_start);
    }

    static esp_err_t getNetworks(httpd_req_t *req)
    {
        const auto &status = from(req)._networks;
        std::string json = "[";
        for (unsigned i = 0; i < status.access_point_count; ++i) {
            if (i) {
                json += ',';
            }
            const auto &ap = status.access_points[i];
            json += "{\"ssid\":" + jsonString(ap.ssid);
            json += ap.secured ? ",\"secured\":true}" : ",\"secured\":false}";
        }
        json += ']';
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        return httpd_resp_send(req, json.data(), json.size());
    }

    static esp_err_t postTime(httpd_req_t *req)
    {
        char body[kPortalBodyBufferSize];
        if (!readPortalBody(req, body) || req->content_len > 12 ||
            !std::all_of(body, body + req->content_len, [](char c) { return c >= '0' && c <= '9'; })) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid time");
        }
        const time_t epoch = std::strtoll(body, nullptr, 10);
        auto &portal = from(req);
        const std::lock_guard lock(portal._mutex);
        if (portal._ready_at || !portal._owner.isCurrent(portal._ticket)) {
            return portalClosing(req);
        }
        portal._last_activity = esp_timer_get_time();
        {
            const std::lock_guard service_lock(portal._owner._mutex);
            if (!portal._owner.isCurrent(portal._ticket) ||
                !portal._owner.updateClockLocked(epoch, TimeSyncSource::PHONE)) {
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RTC write failed");
            }
        }
        return portal.finish(req, Result::PHONE_SYNCED);
    }

    static esp_err_t postWifi(httpd_req_t *req)
    {
        char body[kPortalBodyBufferSize];
        if (!readPortalBody(req, body) || req->content_len < 2) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi configuration");
        }
        const size_t ssid_size = static_cast<uint8_t>(body[0]);
        WifiConfig credentials{};
        if (ssid_size >= req->content_len ||
            !makeCredentials({body + 1, ssid_size},
                             {body + 1 + ssid_size, req->content_len - 1 - ssid_size}, credentials)) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID or password");
        }
        auto &portal = from(req);
        const std::lock_guard lock(portal._mutex);
        if (portal._ready_at || !portal._owner.isCurrent(portal._ticket)) {
            return portalClosing(req);
        }
        portal._router = credentials;
        return portal.finish(req, Result::ROUTER_SUBMITTED);
    }

    esp_err_t finish(httpd_req_t *req, Result result)
    {
        httpd_resp_set_type(req, "application/json");
        const esp_err_t err = httpd_resp_sendstr(req, "{\"ok\":true}");
        _result = result;
        _ready_at = esp_timer_get_time() + 500000;
        return err;
    }

    WifiTimeService &_owner;
    const uint32_t _ticket;
    const WifiTimeStatus &_networks;
    std::mutex _mutex;
    int64_t _last_activity;
    int64_t _ready_at = 0;
    int _dns = -1;
    httpd_handle_t _http = nullptr;
    Result _result = Result::CANCELLED;
    WifiConfig _router{};
};

WifiTimeService &WifiTimeService::instance()
{
    static WifiTimeService service;
    return service;
}

void WifiTimeService::init()
{
    const std::lock_guard lock(_mutex);
    _config = StorageService::instance().loadWifiConfig();
    uint8_t mac[6]{};
    if (reportError(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), "read SoftAP MAC")) {
        std::snprintf(_hotspot, sizeof(_hotspot), "DeskBuddy-%02X%02X", mac[4], mac[5]);
    }
}

WifiTimeStatus WifiTimeService::status()
{
    const std::lock_guard lock(_mutex);
    auto result = _status;
    if (_config.configured) {
        std::memcpy(result.saved_ssid, _config.ssid, sizeof(result.saved_ssid));
    }
    result.last_sync_epoch = _config.last_sync_epoch;
    result.last_sync_src = _config.last_sync_src;
    return result;
}

bool WifiTimeService::submitLocked(Operation op, const WifiConfig &config, bool save)
{
    if (isConnecting(_status.state)) {
        return false;
    }
    _pending = {op, ++_ticket, config, save};
    _status.access_point_count = 0;
    ++_status.revision;
    switch (op) {
        case Operation::PORTAL:
            _status.state = WifiSyncState::STARTING_PORTAL;
            _status.message = "正在开启热点";
            break;
        case Operation::SCAN:
            _status.state = WifiSyncState::SCANNING;
            _status.message = "正在扫描附近 Wi-Fi";
            break;
        case Operation::SYNC:
            _status.state = WifiSyncState::CONNECTING;
            _status.message = "正在连接 Wi-Fi";
            break;
        default:
            break;
    }
    if (!_worker_running) {
        _worker_running = xTaskCreate([](void *arg) {
            static_cast<WifiTimeService *>(arg)->runWorker();
            vTaskDelete(nullptr);
        }, "db_wifi_time", kWifiWorkerStackBytes, this, 4, nullptr) == pdPASS;
        if (!_worker_running) {
            _pending = {};
            _status.state = WifiSyncState::FAILED;
            _status.message = kStartError;
            ESP_LOGW(TAG, "Unable to allocate Wi-Fi worker stack");
        }
    }
    return _worker_running;
}

bool WifiTimeService::startSyncSavedWifi()
{
    const std::lock_guard lock(_mutex);
    return _config.configured && submitLocked(Operation::SYNC, _config);
}

bool WifiTimeService::connectAndSync(std::string_view ssid, std::string_view password)
{
    WifiConfig credentials{};
    if (!makeCredentials(ssid, password, credentials)) {
        return false;
    }
    const std::lock_guard lock(_mutex);
    return submitLocked(Operation::SYNC, credentials, true);
}

bool WifiTimeService::startCaptivePortal()
{
    const std::lock_guard lock(_mutex);
    return submitLocked(Operation::PORTAL);
}

bool WifiTimeService::startApScan()
{
    const std::lock_guard lock(_mutex);
    return submitLocked(Operation::SCAN);
}

void WifiTimeService::cancelProvisioning()
{
    const std::lock_guard lock(_mutex);
    if (isProvisioning(_status.state)) {
        ++_ticket;
        _pending = {};
        _status.state = WifiSyncState::IDLE;
        _status.message = "";
        ++_status.revision;
    }
}

void WifiTimeService::prepareForShutdown()
{
    std::unique_lock lock(_mutex);
    ++_ticket;
    _pending = {};
    _worker_stopped.wait(lock, [this] { return !_worker_running; });
    _status.state = WifiSyncState::IDLE;
    _status.message = "";
    ++_status.revision;
}

void WifiTimeService::scheduleBootSync()
{
    const std::lock_guard lock(_mutex);
    if (!_config.configured) {
        return;
    }
    const auto now = time(nullptr);
    struct tm local_now{}, last_sync{};
    if (ClockService::instance().isTimeValid() && localTime(now, local_now) &&
        localTime(_config.last_sync_epoch, last_sync) && now >= _config.last_sync_epoch &&
        now - _config.last_sync_epoch < 86400 && local_now.tm_year == last_sync.tm_year &&
        local_now.tm_yday == last_sync.tm_yday) {
        return;
    }
    submitLocked(Operation::BOOT_SYNC, _config);
}

bool WifiTimeService::setManualTime(const DateTime &date)
{
    const std::lock_guard lock(_mutex);
    if (isConnecting(_status.state) || isProvisioning(_status.state)) {
        return false;
    }
    ++_ticket;
    _pending = {};
    struct tm local{};
    local.tm_year = date.year - 1900;
    local.tm_mon = date.month - 1;
    local.tm_mday = date.day;
    local.tm_hour = date.hour;
    local.tm_min = date.minute;
    local.tm_sec = date.second;
    const bool updated = updateClockLocked(mktime(&local), TimeSyncSource::MANUAL);
    if (updated) {
        _status.state = WifiSyncState::IDLE;
        _status.message = "";
    }
    return updated;
}

bool WifiTimeService::resetSettings()
{
    const std::lock_guard lock(_mutex);
    ++_ticket;
    _pending = {};
    const uint32_t revision = _status.revision + 1;
    _status = {};
    _status.revision = revision;
    if (!StorageService::instance().resetAll()) {
        return false;
    }
    _config = {};
    return true;
}

void WifiTimeService::publish(uint32_t ticket, WifiSyncState state, const char *message)
{
    const std::lock_guard lock(_mutex);
    if (isCurrent(ticket)) {
        _status.state = state;
        _status.message = message;
        ++_status.revision;
    }
}

void WifiTimeService::runWorker()
{
    for (;;) {
        Job job;
        {
            const std::lock_guard lock(_mutex);
            if (_pending.operation == Operation::NONE) {
                _worker_running = false;
                _worker_stopped.notify_all();
                return;
            }
            job = _pending;
            _pending = {};
        }
        runJob(job);
    }
}

void WifiTimeService::runJob(const Job &job)
{
    switch (job.operation) {
        case Operation::BOOT_SYNC:
            for (int i = 0; i < 10 && isCurrent(job.ticket); ++i) {
                sleepMs(250);
            }
            if (!isCurrent(job.ticket)) {
                return;
            }
            publish(job.ticket, WifiSyncState::CONNECTING, "正在连接 Wi-Fi");
            [[fallthrough]];
        case Operation::SYNC:
            syncStation(job);
            break;
        case Operation::PORTAL:
            provision(job);
            break;
        case Operation::SCAN: {
            Radio radio;
            WifiTimeStatus results;
            const bool scanned = isCurrent(job.ticket) && radio.start() && isCurrent(job.ticket) && scanNetworks(results);
            radio.stop();
            const std::lock_guard lock(_mutex);
            if (isCurrent(job.ticket)) {
                _status.access_points = results.access_points;
                _status.access_point_count = results.access_point_count;
                _status.state = scanned ? WifiSyncState::SCAN_READY : WifiSyncState::FAILED;
                _status.message = scanned ? "" : "扫描失败，请刷新重试";
                ++_status.revision;
            }
            break;
        }
        case Operation::NONE:
            break;
    }
}

void WifiTimeService::syncStation(const Job &job)
{
    Radio radio;
    const char *error = kStartError;
    bool synchronized = false;
    if (isCurrent(job.ticket) && radio.start() && radio.connect(job.config)) {
        const int64_t deadline = esp_timer_get_time() + kStationTimeoutUs;
        while (isCurrent(job.ticket) && !radio.hasIp() && esp_timer_get_time() < deadline) {
            sleepMs(100);
        }
        if (isCurrent(job.ticket) && radio.hasIp()) {
            bool credentials_saved = true;
            if (job.save) {
                const std::lock_guard lock(_mutex);
                if (isCurrent(job.ticket)) {
                    auto config = job.config;
                    config.last_sync_epoch = _config.last_sync_epoch;
                    config.last_sync_src = _config.last_sync_src;
                    credentials_saved = saveConfigLocked(config);
                }
            }
            publish(job.ticket, WifiSyncState::SYNCING_SNTP, "已连接，正在网络校时");
            const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
                3, ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "cn.pool.ntp.org", "time.apple.com"));
            error = "网络校时超时，请重试";
            if (isCurrent(job.ticket) && reportError(esp_netif_sntp_init(&config), "esp_netif_sntp_init")) {
                const int64_t sync_deadline = esp_timer_get_time() + kSntpTimeoutUs;
                while (isCurrent(job.ticket) && esp_timer_get_time() < sync_deadline) {
                    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(200)) == ESP_OK) {
                        const std::lock_guard lock(_mutex);
                        if (isCurrent(job.ticket)) {
                            synchronized = updateClockLocked(time(nullptr), TimeSyncSource::SNTP);
                            error = synchronized ? kSaveError : kRtcError;
                        }
                        break;
                    }
                }
                esp_netif_sntp_deinit();
            }
            synchronized = synchronized && credentials_saved;
        } else {
            error = "连接超时，请检查 Wi-Fi 密码和信号";
        }
    }
    radio.stop();
    publish(job.ticket, synchronized ? WifiSyncState::SUCCESS : WifiSyncState::FAILED,
            synchronized ? "校时成功，Wi-Fi 已关闭" : error);
}

void WifiTimeService::provision(const Job &job)
{
    Portal::Result result = Portal::Result::CANCELLED;
    WifiConfig router{};
    WifiTimeStatus networks;
    Radio radio;
    if (isCurrent(job.ticket) && radio.start() && isCurrent(job.ticket)) {
        scanNetworks(networks);
    }
    radio.stop();

    if (isCurrent(job.ticket) && radio.start(_hotspot)) {
        {
            Portal portal(*this, job.ticket, networks);
            if (isCurrent(job.ticket) && portal.start()) {
                publish(job.ticket, WifiSyncState::PORTAL_ACTIVE, "热点已开启 · 等待手机扫码");
                result = portal.wait(router);
            }
        }
    }
    radio.stop();
    if (!isCurrent(job.ticket)) {
        return;
    }
    switch (result) {
        case Portal::Result::ROUTER_SUBMITTED:
            publish(job.ticket, WifiSyncState::CONNECTING, "正在连接 Wi-Fi");
            syncStation({Operation::SYNC, job.ticket, router, true});
            break;
        case Portal::Result::PHONE_SYNCED:
            publish(job.ticket, WifiSyncState::SUCCESS, "手机校时成功，热点已关闭");
            break;
        case Portal::Result::EXPIRED:
            publish(job.ticket, WifiSyncState::FAILED, "热点等待超时，请重新扫码");
            break;
        case Portal::Result::CANCELLED:
            publish(job.ticket, WifiSyncState::FAILED, kStartError);
            break;
    }
}

bool WifiTimeService::updateClockLocked(time_t epoch, TimeSyncSource source)
{
    struct tm local{};
    if (!localTime(epoch, local) ||
        !ClockService::instance().setDateTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                             local.tm_hour, local.tm_min, local.tm_sec)) {
        return false;
    }
    auto config = _config;
    config.last_sync_epoch = epoch;
    config.last_sync_src = source;
    if (!saveConfigLocked(config)) {
        _config = config;
    }
    ++_status.revision;
    return true;
}

bool WifiTimeService::saveConfigLocked(const WifiConfig &config)
{
    if (!StorageService::instance().saveWifiConfig(config)) {
        ESP_LOGW(TAG, "Unable to persist Wi-Fi settings");
        return false;
    }
    _config = config;
    return true;
}

bool WifiTimeService::makeCredentials(std::string_view ssid, std::string_view password, WifiConfig &config)
{
    if (ssid.empty() || ssid.size() > 32 || ssid.find('\0') != std::string_view::npos ||
        password.size() > 64 || (!password.empty() && password.size() < 8)) {
        return false;
    }
    const bool printable = std::all_of(password.begin(), password.end(), [](unsigned char c) {
        return c >= 0x20 && c <= 0x7E;
    });
    const bool raw_key = password.size() == 64 && std::all_of(password.begin(), password.end(), [](unsigned char c) {
        return std::isxdigit(c);
    });
    if (!printable || (password.size() == 64 && !raw_key)) {
        return false;
    }
    std::memcpy(config.ssid, ssid.data(), ssid.size());
    if (!password.empty()) {
        std::memcpy(config.password, password.data(), password.size());
    }
    config.configured = true;
    return true;
}

}
