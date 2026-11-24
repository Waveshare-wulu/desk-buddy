#pragma once

#include <cstdint>
#include "db_button_logic.hpp"
#include "driver/i2c_master.h"
#include "lvgl.h"

namespace desk_buddy {

struct BatteryStatus {
    bool pmu_present = false;
    bool battery_connected = false;
    bool vbus_in = false;
    bool charging = false;
    int percent = 100;
    uint16_t batt_mv = 0;
};

enum class ScreenPowerState : uint8_t {
    ACTIVE = 0,
    DIMMED,
    SLEEP,
};

class PowerService {
public:
    static PowerService &instance();

    bool init(i2c_master_bus_handle_t i2c_bus, uint8_t brightness_pct,
              uint16_t dim_timeout_s, uint16_t sleep_timeout_s);

    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return _user_brightness; }

    void setTimeouts(uint16_t dim_timeout_s, uint16_t sleep_timeout_s);

    /** Call periodically from LVGL timer (~500ms) to update idle state and poll PMU. */
    void tickLvgl();

    void wakeUp();
    void sleepNow();
    void toggleScreen();
    ButtonPress pollPowerKey();
    bool powerOff();
    [[noreturn]] void reboot();

    BatteryStatus getBatteryStatus() const { return _battery; }
    ScreenPowerState getScreenState() const { return _screen_state; }

private:
    PowerService() = default;
    void pollPmu();
    bool configurePowerKey();
    void applyBacklight(uint8_t percent);
    void showWakeShield(bool show);
    static void onWakeShieldPressed(lv_event_t *e);

    bool regRead(uint8_t reg, uint8_t *buf, size_t len);
    bool regWrite(uint8_t reg, uint8_t val);

    i2c_master_dev_handle_t _pmu_dev = nullptr;
    BatteryStatus _battery{};
    bool _power_key_ready = false;
    uint8_t _user_brightness = 85;
    uint8_t _applied_brightness = 0xFF;
    uint16_t _dim_timeout_s = 30;
    uint16_t _sleep_timeout_s = 90;
    ScreenPowerState _screen_state = ScreenPowerState::ACTIVE;
    uint64_t _last_pmu_poll_ms = 0;
    lv_obj_t *_wake_shield = nullptr;
};

}
