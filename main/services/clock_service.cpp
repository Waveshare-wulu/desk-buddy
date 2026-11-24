#include "clock_service.hpp"

#include <ctime>
#include <sys/time.h>
#include "esp_log.h"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Clock";
constexpr uint8_t kPcf85063Addr = 0x51;
constexpr uint8_t kRegSeconds = 0x04;
constexpr int kI2cTimeoutMs = 100;

uint8_t bcdToDec(uint8_t bcd)
{
    return static_cast<uint8_t>(((bcd >> 4) * 10) + (bcd & 0x0F));
}

uint8_t decToBcd(uint8_t dec)
{
    return static_cast<uint8_t>(((dec / 10) << 4) | (dec % 10));
}

int computeWeekday(int y, int m, int d)
{
    static constexpr int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

}

ClockService &ClockService::instance()
{
    static ClockService s_inst;
    return s_inst;
}

bool ClockService::init(i2c_master_bus_handle_t i2c_bus)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    if (i2c_bus == nullptr) {
        ESP_LOGW(TAG, "I2C bus handle is NULL, running without hardware RTC");
        return false;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kPcf85063Addr;
    dev_cfg.scl_speed_hz = 400000;

    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &_rtc_dev) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to attach PCF85063A on I2C");
        return false;
    }

    DateTime dt{};
    if (readRtc(dt)) {
        _rtc_present = true;
        if (dt.valid) {
            syncSystemClock(dt);
            _time_valid = true;
            ESP_LOGI(TAG, "RTC time loaded: %04d-%02d-%02d %02d:%02d:%02d",
                     dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        } else {
            _time_valid = false;
            ESP_LOGW(TAG, "PCF85063A present but time uncalibrated (OS flag set or out of range)");
        }
        return true;
    }

    ESP_LOGW(TAG, "PCF85063A did not respond at 0x51");
    return false;
}

bool ClockService::readRtc(DateTime &out)
{
    if (!_rtc_dev) {
        return false;
    }
    uint8_t reg = kRegSeconds;
    uint8_t raw[7] = {};
    esp_err_t err = i2c_master_transmit_receive(_rtc_dev, &reg, 1, raw, sizeof(raw), kI2cTimeoutMs);
    if (err != ESP_OK) {
        return false;
    }

    const bool os_stopped = (raw[0] & 0x80) != 0;
    out.second = bcdToDec(raw[0] & 0x7F);
    out.minute = bcdToDec(raw[1] & 0x7F);
    out.hour = bcdToDec(raw[2] & 0x3F);
    out.day = bcdToDec(raw[3] & 0x3F);
    out.weekday = raw[4] & 0x07;
    out.month = bcdToDec(raw[5] & 0x1F);
    out.year = 2000 + bcdToDec(raw[6]);

    out.valid = !os_stopped &&
                out.year >= 2024 && out.year <= 2099 &&
                out.month >= 1 && out.month <= 12 &&
                out.day >= 1 && out.day <= daysInMonth(out.year, out.month) &&
                out.hour >= 0 && out.hour <= 23 &&
                out.minute >= 0 && out.minute <= 59 &&
                out.second >= 0 && out.second <= 59;

    if (out.valid) {
        out.weekday = computeWeekday(out.year, out.month, out.day);
    }
    return true;
}

bool ClockService::writeRtc(const DateTime &dt)
{
    if (!_rtc_dev) {
        return false;
    }
    uint8_t buf[8] = {};
    buf[0] = kRegSeconds;
    buf[1] = decToBcd(static_cast<uint8_t>(dt.second)) & 0x7F;
    buf[2] = decToBcd(static_cast<uint8_t>(dt.minute));
    buf[3] = decToBcd(static_cast<uint8_t>(dt.hour));
    buf[4] = decToBcd(static_cast<uint8_t>(dt.day));
    buf[5] = static_cast<uint8_t>(dt.weekday & 0x07);
    buf[6] = decToBcd(static_cast<uint8_t>(dt.month));
    buf[7] = decToBcd(static_cast<uint8_t>(dt.year >= 2000 ? dt.year - 2000 : 26));

    esp_err_t err = i2c_master_transmit(_rtc_dev, buf, sizeof(buf), kI2cTimeoutMs);
    return err == ESP_OK;
}

void ClockService::syncSystemClock(const DateTime &dt)
{
    struct tm t = {};
    t.tm_year = dt.year - 1900;
    t.tm_mon = dt.month - 1;
    t.tm_mday = dt.day;
    t.tm_hour = dt.hour;
    t.tm_min = dt.minute;
    t.tm_sec = dt.second;
    t.tm_isdst = 0;

    time_t epoch = mktime(&t);
    if (epoch > 0) {
        struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
        settimeofday(&tv, nullptr);
    }
}

bool ClockService::setDateTime(int year, int month, int day, int hour, int minute, int second)
{
    if (year < 2024 || year > 2099 || month < 1 || month > 12) {
        return false;
    }
    const int max_day = daysInMonth(year, month);
    if (day < 1) day = 1;
    if (day > max_day) day = max_day;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }

    DateTime dt{};
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    dt.weekday = computeWeekday(year, month, day);
    dt.valid = true;

    if (!writeRtc(dt)) {
        ESP_LOGW(TAG, "PCF85063A RTC write failed");
        return false;
    }
    syncSystemClock(dt);
    _rtc_present = true;
    _time_valid = true;
    ESP_LOGI(TAG, "Updated PCF85063A RTC: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    return true;
}

DateTime ClockService::getNow() const
{
    DateTime dt{};
    time_t now = 0;
    struct tm ti = {};
    dt.valid = isTimeValid();
    time(&now);
    localtime_r(&now, &ti);

    if (dt.valid) {
        dt.year = ti.tm_year + 1900;
        dt.month = ti.tm_mon + 1;
        dt.day = ti.tm_mday;
        dt.hour = ti.tm_hour;
        dt.minute = ti.tm_min;
        dt.second = ti.tm_sec;
        dt.weekday = ti.tm_wday;
    } else {
        dt.year = 2026;
        dt.month = 1;
        dt.day = 1;
        dt.hour = ti.tm_hour;
        dt.minute = ti.tm_min;
        dt.second = ti.tm_sec;
        dt.weekday = ti.tm_wday;
    }
    return dt;
}

const char *ClockService::weekdayChinese(int weekday)
{
    static constexpr const char *kWeekdays[7] = {
        "周日", "周一", "周二", "周三", "周四", "周五", "周六"
    };
    if (weekday < 0 || weekday > 6) {
        return "未知";
    }
    return kWeekdays[weekday];
}

int ClockService::daysInMonth(int year, int month)
{
    static constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 31;
    }
    if (month == 2) {
        const bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return kDays[month - 1];
}

}
