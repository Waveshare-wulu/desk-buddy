#include "power_service.hpp"

#include <algorithm>
#include "services/display_service.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Power";
constexpr uint8_t kAxp2101Addr = 0x34;
constexpr uint8_t kRegStatus1 = 0x00;
constexpr uint8_t kRegStatus2 = 0x01;
constexpr uint8_t kRegChipId = 0x03;
constexpr uint8_t kRegPmuConfig = 0x10;
constexpr uint8_t kRegPkeyTiming = 0x27;
constexpr uint8_t kRegIrqEnable1 = 0x41;
constexpr uint8_t kRegIrqStatus1 = 0x49;
constexpr uint8_t kSoftPowerOff = 1u << 0;
constexpr uint8_t kPkeyShort = 1u << 3;
constexpr uint8_t kPkeyLong = 1u << 2;
constexpr uint8_t kPkeyIrqs = kPkeyShort | kPkeyLong;
constexpr uint8_t kPkeyTimingMask = (3u << 4) | (3u << 2);
constexpr uint8_t kPkeyLong1500msOff6s = (1u << 4) | (1u << 2);
constexpr uint8_t kRegAdcCtrl = 0x30;
constexpr uint8_t kAdcVbatEnable = 1u << 0;
constexpr uint8_t kAdcTemperatureSenseEnable = 1u << 1;
constexpr uint8_t kAdcVbusEnable = 1u << 2;
constexpr uint8_t kAdcVsysEnable = 1u << 3;
constexpr uint8_t kRegVbatH = 0x34;
constexpr uint8_t kRegBatPercent = 0xA4;
constexpr int kI2cTimeoutMs = 100;
constexpr uint8_t kDimBrightness = 15;
constexpr uint64_t kPmuPollIntervalMs = 5000;
}

PowerService &PowerService::instance()
{
    static PowerService s_inst;
    return s_inst;
}

bool PowerService::regRead(uint8_t reg, uint8_t *buf, size_t len)
{
    if (!_pmu_dev) return false;
    return i2c_master_transmit_receive(_pmu_dev, &reg, 1, buf, len, kI2cTimeoutMs) == ESP_OK;
}

bool PowerService::regWrite(uint8_t reg, uint8_t val)
{
    if (!_pmu_dev) return false;
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(_pmu_dev, data, sizeof(data), kI2cTimeoutMs) == ESP_OK;
}

bool PowerService::init(i2c_master_bus_handle_t i2c_bus, uint8_t brightness_pct,
                        uint16_t dim_timeout_s, uint16_t sleep_timeout_s)
{
    _user_brightness = std::clamp<uint8_t>(brightness_pct, 15, 100);
    _dim_timeout_s = dim_timeout_s;
    _sleep_timeout_s = sleep_timeout_s;
    _screen_state = ScreenPowerState::ACTIVE;

    if (i2c_bus == nullptr) {
        ESP_LOGW(TAG, "I2C bus NULL, AXP2101 battery monitoring disabled");
        return false;
    }

    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = kAxp2101Addr;
    cfg.scl_speed_hz = 400000;

    if (i2c_master_bus_add_device(i2c_bus, &cfg, &_pmu_dev) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add AXP2101 I2C device");
        return false;
    }

    uint8_t chip_id = 0;
    if (regRead(kRegChipId, &chip_id, 1)) {
        _battery.pmu_present = true;
        uint8_t adc = 0;
        if (regRead(kRegAdcCtrl, &adc, 1)) {
            adc |= kAdcVbatEnable | kAdcVbusEnable | kAdcVsysEnable;
            adc &= static_cast<uint8_t>(~kAdcTemperatureSenseEnable);
            regWrite(kRegAdcCtrl, adc);
        }
        pollPmu();
        _power_key_ready = configurePowerKey();
        if (!_power_key_ready) {
            ESP_LOGW(TAG, "Unable to configure AXP2101 PWR key");
        }
        ESP_LOGI(TAG, "AXP2101 ready (chip=0x%02X, bat=%d%%, %umV, chg=%d)",
                 chip_id, _battery.percent, _battery.batt_mv, _battery.charging);
        return true;
    }

    ESP_LOGW(TAG, "AXP2101 did not respond at 0x34");
    return false;
}

bool PowerService::configurePowerKey()
{
    uint8_t timing = 0;
    uint8_t enable = 0;
    if (!regRead(kRegPkeyTiming, &timing, 1) || !regRead(kRegIrqEnable1, &enable, 1)) {
        return false;
    }
    const uint8_t new_timing = (timing & ~kPkeyTimingMask) | kPkeyLong1500msOff6s;
    return (timing == new_timing || regWrite(kRegPkeyTiming, new_timing)) &&
           regWrite(kRegIrqStatus1, kPkeyIrqs) && regWrite(kRegIrqEnable1, enable | kPkeyIrqs);
}

ButtonPress PowerService::pollPowerKey()
{
    uint8_t status = 0;
    if (!_power_key_ready || !regRead(kRegIrqStatus1, &status, 1)) {
        return ButtonPress::NONE;
    }
    const uint8_t pending = status & kPkeyIrqs;
    // AXP2101 REG 0x49 clears only the status bits written as 1.
    if (!pending || !regWrite(kRegIrqStatus1, pending)) {
        return ButtonPress::NONE;
    }
    return pending & kPkeyLong ? ButtonPress::LONG : ButtonPress::SHORT;
}

bool PowerService::powerOff()
{
    uint8_t config = 0;
    return _battery.pmu_present && regRead(kRegPmuConfig, &config, 1) &&
           regWrite(kRegPmuConfig, config | kSoftPowerOff);
}

void PowerService::reboot()
{
    esp_restart();
}

void PowerService::pollPmu()
{
    if (!_battery.pmu_present) {
        return;
    }
    uint8_t st[2] = {0, 0};
    if (!regRead(kRegStatus1, st, 2)) {
        return;
    }

    const bool vbus_good = (st[0] & (1u << 5)) != 0;
    const bool bat_conn = (st[0] & (1u << 3)) != 0;
    const bool chg = ((st[1] >> 5) == 0x01);
    const bool vbus_in = ((st[1] & (1u << 3)) == 0) && vbus_good;

    _battery.battery_connected = bat_conn;
    _battery.vbus_in = vbus_in;
    _battery.charging = chg;

    if (bat_conn) {
        uint8_t pct = 0;
        if (regRead(kRegBatPercent, &pct, 1)) {
            _battery.percent = std::clamp<int>(pct, 0, 100);
        }
        uint8_t vraw[2] = {0, 0};
        if (regRead(kRegVbatH, vraw, 2)) {
            _battery.batt_mv = static_cast<uint16_t>(((vraw[0] & 0x1Fu) << 8) | vraw[1]);
        }
    } else {
        _battery.percent = 100;
        _battery.batt_mv = 0;
    }
}

void PowerService::setBrightness(uint8_t percent)
{
    _user_brightness = std::clamp<uint8_t>(percent, 15, 100);
    if (_screen_state == ScreenPowerState::ACTIVE) {
        applyBacklight(_user_brightness);
    }
}

void PowerService::setTimeouts(uint16_t dim_timeout_s, uint16_t sleep_timeout_s)
{
    _dim_timeout_s = dim_timeout_s;
    _sleep_timeout_s = sleep_timeout_s;
    if (_screen_state != ScreenPowerState::ACTIVE) {
        wakeUp();
    }
}

void PowerService::applyBacklight(uint8_t percent)
{
    if (_applied_brightness == percent) {
        return;
    }
    _applied_brightness = percent;
    setDisplayBrightness(percent);
}

void PowerService::showWakeShield(bool show)
{
    if (show) {
        if (!_wake_shield) {
            _wake_shield = lv_obj_create(lv_layer_top());
            lv_obj_remove_style_all(_wake_shield);
            lv_obj_set_size(_wake_shield, LV_PCT(100), LV_PCT(100));
            lv_obj_set_pos(_wake_shield, 0, 0);
            lv_obj_set_style_bg_color(_wake_shield, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(_wake_shield, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(_wake_shield, 0, 0);
            lv_obj_set_style_radius(_wake_shield, 0, 0);
            lv_obj_add_flag(_wake_shield, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(_wake_shield, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(_wake_shield, onWakeShieldPressed, LV_EVENT_PRESSED, this);
        }
        lv_obj_clear_flag(_wake_shield, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_wake_shield);
    } else if (_wake_shield) {
        lv_obj_add_flag(_wake_shield, LV_OBJ_FLAG_HIDDEN);
    }
}

void PowerService::onWakeShieldPressed(lv_event_t *e)
{
    auto *self = static_cast<PowerService *>(lv_event_get_user_data(e));
    if (self) {
        self->wakeUp();
    }
}

void PowerService::wakeUp()
{
    showWakeShield(false);
    if (_screen_state == ScreenPowerState::SLEEP) {
        lv_refr_now(lv_obj_get_display(_wake_shield));
    }
    _screen_state = ScreenPowerState::ACTIVE;
    applyBacklight(_user_brightness);
    lv_display_trigger_activity(nullptr);
}

void PowerService::sleepNow()
{
    _screen_state = ScreenPowerState::SLEEP;
    showWakeShield(true);
    applyBacklight(0);
}

void PowerService::toggleScreen()
{
    if (_screen_state == ScreenPowerState::SLEEP) {
        wakeUp();
    } else {
        sleepNow();
    }
}

void PowerService::tickLvgl()
{
    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);

    if (_battery.pmu_present && (now_ms - _last_pmu_poll_ms) >= kPmuPollIntervalMs) {
        _last_pmu_poll_ms = now_ms;
        pollPmu();
    }

    // LVGL measures inactivity from input activity timestamps, independent of redraws.
    const uint32_t idle_ms = lv_display_get_inactive_time(nullptr);
    const uint32_t idle_s = idle_ms / 1000u;

    if (_screen_state == ScreenPowerState::SLEEP) {
        return;
    }

    if (_sleep_timeout_s > 0 && idle_s >= _sleep_timeout_s) {
        ESP_LOGI(TAG, "Idle %lus -> AMOLED sleep", static_cast<unsigned long>(idle_s));
        sleepNow();
        return;
    }

    if (_dim_timeout_s > 0 && idle_s >= _dim_timeout_s) {
        if (_screen_state != ScreenPowerState::DIMMED) {
            ESP_LOGD(TAG, "Idle %lus -> AMOLED dim", static_cast<unsigned long>(idle_s));
            _screen_state = ScreenPowerState::DIMMED;
            applyBacklight(std::min<uint8_t>(_user_brightness, kDimBrightness));
        }
    } else if (_screen_state == ScreenPowerState::DIMMED) {
        _screen_state = ScreenPowerState::ACTIVE;
        applyBacklight(_user_brightness);
    }
}

}
