#pragma once

#include <atomic>
#include <cstdint>
#include "driver/i2c_master.h"

namespace desk_buddy {

struct DateTime {
    int year = 2026;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int weekday = 0; // 0 = Sunday .. 6 = Saturday
    bool valid = false;
};

class ClockService {
public:
    static ClockService &instance();

    bool init(i2c_master_bus_handle_t i2c_bus);

    bool isTimeValid() const { return _time_valid.load(); }
    bool isRtcPresent() const { return _rtc_present.load(); }

    DateTime getNow() const;
    bool setDateTime(int year, int month, int day, int hour, int minute, int second = 0);

    static const char *weekdayChinese(int weekday);
    static int daysInMonth(int year, int month);

private:
    ClockService() = default;
    bool readRtc(DateTime &out);
    bool writeRtc(const DateTime &dt);
    void syncSystemClock(const DateTime &dt);

    i2c_master_dev_handle_t _rtc_dev = nullptr;
    std::atomic<bool> _rtc_present{false};
    std::atomic<bool> _time_valid{false};
};

}
