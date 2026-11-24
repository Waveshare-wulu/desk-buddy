#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "lvgl.h"
#include "services/clock_service.hpp"
#include "services/power_service.hpp"
#include "ui/home/db_home_header.hpp"

namespace desk_buddy {

class Desktop {
public:
    void create(lv_obj_t *screen, lv_indev_t *touch_input, std::function<void(uint32_t)> launch);
    void addApp(uint32_t id, uint8_t page, const char *name, const lv_image_dsc_t *icon);
    void showHome(HomeHeader::Callbacks callbacks);
    void onBackButtonPressed();
    void refreshStatus(const DateTime &time, const BatteryStatus &battery);
    lv_obj_t *appLayer() const { return _app_layer; }
    HomeHeader &homeHeader() { return _home_header; }

private:
    static void onTouchEvent(lv_event_t *event);
    static void onAppClicked(lv_event_t *event);
    void selectPage(int page);
    bool isDesktopControl(lv_obj_t *target) const;

    HomeHeader _home_header;
    std::function<void(uint32_t)> _launch;
    std::array<lv_obj_t *, 3> _pages{};
    std::array<lv_obj_t *, 3> _grids{};
    std::array<lv_obj_t *, 3> _indicators{};
    lv_obj_t *_tileview = nullptr;
    lv_obj_t *_app_layer = nullptr;
    lv_obj_t *_clock = nullptr;
    lv_obj_t *_battery = nullptr;
    lv_obj_t *_battery_fill = nullptr;
    lv_obj_t *_battery_charge = nullptr;
    int _current_page = 0;
    lv_point_t _touch_start{};
    bool _desktop_touch = false;
    bool _suppress_click = false;
};

}
