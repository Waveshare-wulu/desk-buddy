#include "ui/home/db_desktop.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "esp_log.h"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {

constexpr int kScreenWidth = 480;
constexpr int kStatusHeight = 40;
constexpr int kPageHeight = 412;
constexpr int kDesktopHeight = 440;
constexpr int kStatusSideInset = 48;
constexpr int kStatusVerticalOffset = 4;
constexpr int kBatteryFillWidth = 18;
constexpr int kSwipeDistance = 44;
constexpr int kTapSlop = 22;

lv_obj_t *createContainer(lv_obj_t *parent, int width, int height)
{
    auto *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, width, height);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t *createLabel(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    auto *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, theme::color(color), 0);
    return label;
}

}

void Desktop::create(lv_obj_t *screen, lv_indev_t *touch_input, std::function<void(uint32_t)> launch)
{
    _launch = std::move(launch);
    theme::styleRootScreen(screen);

    auto *status = createContainer(screen, kScreenWidth, kStatusHeight);
    _clock = createLabel(status, theme::fontMd(), theme::kTextPrimary);
    lv_obj_align(_clock, LV_ALIGN_LEFT_MID, kStatusSideInset, kStatusVerticalOffset);
    auto *battery = createContainer(status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(battery, LV_ALIGN_RIGHT_MID, -kStatusSideInset, kStatusVerticalOffset);
    lv_obj_set_flex_flow(battery, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(battery, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(battery, 6, 0);
    _battery = createLabel(battery, theme::fontSm(), theme::kTextSecondary);
    auto *indicator = createContainer(battery, 28, 12);
    auto *outline = createContainer(indicator, 24, 12);
    lv_obj_set_style_radius(outline, 4, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_border_color(outline, theme::color(theme::kTextSecondary), 0);
    auto *terminal = createContainer(indicator, 2, 5);
    lv_obj_align(terminal, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(terminal, theme::color(theme::kTextSecondary), 0);
    lv_obj_set_style_bg_opa(terminal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(terminal, 1, 0);
    _battery_fill = createContainer(outline, kBatteryFillWidth, 6);
    lv_obj_align(_battery_fill, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_set_style_bg_opa(_battery_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_battery_fill, 2, 0);
    _battery_charge = createLabel(indicator, &lv_font_montserrat_12, theme::kTextPrimary);
    lv_label_set_text_static(_battery_charge, LV_SYMBOL_CHARGE);
    lv_obj_align(_battery_charge, LV_ALIGN_CENTER, -2, 0);

    _tileview = lv_tileview_create(screen);
    lv_obj_set_size(_tileview, kScreenWidth, kPageHeight);
    lv_obj_set_pos(_tileview, 0, kStatusHeight);
    lv_obj_set_style_bg_opa(_tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(_tileview, LV_OBJ_FLAG_SCROLLABLE);
    _pages[0] = lv_tileview_add_tile(_tileview, 0, 0, LV_DIR_RIGHT);
    _pages[1] = lv_tileview_add_tile(_tileview, 1, 0, LV_DIR_HOR);
    _pages[2] = lv_tileview_add_tile(_tileview, 2, 0, LV_DIR_LEFT);
    for (auto *page : _pages) {
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    }
    for (int index = 1; index <= 2; ++index) {
        _grids[index] = createContainer(_pages[index], kScreenWidth, kPageHeight);
        lv_obj_set_style_pad_hor(_grids[index], 9, 0);
        lv_obj_set_style_pad_ver(_grids[index], 30, 0);
        lv_obj_set_style_pad_column(_grids[index], 9, 0);
        lv_obj_set_style_pad_row(_grids[index], 30, 0);
        lv_obj_set_flex_flow(_grids[index], LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(_grids[index], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }

    auto *indicator_row = createContainer(screen, kScreenWidth, kDesktopHeight - kPageHeight);
    lv_obj_align(indicator_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(indicator_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(indicator_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(indicator_row, 6, 0);
    for (auto &indicator : _indicators) {
        indicator = createContainer(indicator_row, 6, 6);
        lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, 0);
    }
    lv_indev_add_event_cb(touch_input, onTouchEvent, LV_EVENT_PRESSED, this);
    lv_indev_add_event_cb(touch_input, onTouchEvent, LV_EVENT_RELEASED, this);
    lv_indev_add_event_cb(touch_input, onTouchEvent, LV_EVENT_SHORT_CLICKED, this);
    lv_indev_add_event_cb(touch_input, onTouchEvent, LV_EVENT_CLICKED, this);

    _app_layer = createContainer(screen, kScreenWidth, kDesktopHeight);
    lv_obj_set_pos(_app_layer, 0, kStatusHeight);
}

void Desktop::addApp(uint32_t id, uint8_t page, const char *name, const lv_image_dsc_t *icon)
{
    auto *cell = createContainer(_grids[page], 148, 160);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(cell, reinterpret_cast<void *>(static_cast<uintptr_t>(id)));
    lv_obj_add_event_cb(cell, onAppClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_text_color(cell, theme::color(theme::kTextPrimary), 0);
    lv_obj_set_style_text_color(cell, theme::color(theme::kAccentAmberFg), LV_STATE_PRESSED);

    auto *icon_area = createContainer(cell, 100, 100);
    lv_obj_align(icon_area, LV_ALIGN_TOP_MID, 0, 14);
    auto *image = lv_image_create(icon_area);
    lv_image_set_src(image, icon);
    lv_image_set_scale(image, LV_SCALE_NONE * 100 / icon->header.w);
    lv_obj_center(image);
    auto *label = lv_label_create(cell);
    lv_obj_set_style_text_font(label, theme::fontSm(), 0);
    lv_label_set_text(label, name);
    lv_obj_align_to(label, icon_area, LV_ALIGN_OUT_BOTTOM_MID, 0, 9);
}

void Desktop::showHome(HomeHeader::Callbacks callbacks)
{
    _home_header.create(_pages[0], _pages[1], std::move(callbacks));
    selectPage(1);
}

void Desktop::refreshStatus(const DateTime &time, const BatteryStatus &battery)
{
    if (time.valid) {
        lv_label_set_text_fmt(_clock, "%02d:%02d", time.hour, time.minute);
    } else {
        lv_label_set_text(_clock, "--:--");
    }
    if (battery.battery_connected) {
        lv_label_set_text_fmt(_battery, "%d%%", battery.percent);
    } else {
        lv_label_set_text(_battery, "--%");
    }
    const int percent = battery.battery_connected ? std::clamp(battery.percent, 0, 100) : 0;
    lv_obj_set_width(_battery_fill, (percent * kBatteryFillWidth + 99) / 100);
    lv_obj_set_style_bg_color(_battery_fill,
                              theme::color(percent <= 15 && !battery.charging ? theme::kAccentCoral : theme::kAccentMint), 0);
    if (battery.charging) {
        lv_obj_clear_flag(_battery_charge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_battery_charge, LV_OBJ_FLAG_HIDDEN);
    }
}

void Desktop::onBackButtonPressed()
{
    selectPage(_current_page == 1 ? 0 : 1);
}

void Desktop::selectPage(int page)
{
    if (page < 0 || page >= static_cast<int>(_pages.size()) || page == _current_page) {
        return;
    }
    _current_page = page;
    lv_tileview_set_tile_by_index(_tileview, page, 0, LV_ANIM_OFF);
    for (int index = 0; index < static_cast<int>(_indicators.size()); ++index) {
        const bool active = index == page;
        lv_obj_set_width(_indicators[index], active ? 20 : 6);
        lv_obj_set_style_bg_color(_indicators[index],
                                  theme::color(active ? theme::kAccentAmber : 0xDFD3C0), 0);
    }
    ESP_LOGI("DB:Desktop", "Desktop page %d", page);
}

bool Desktop::isDesktopControl(lv_obj_t *target) const
{
    for (auto *object = target; object; object = lv_obj_get_parent(object)) {
        if (object == _tileview) {
            return true;
        }
    }
    return false;
}

void Desktop::onTouchEvent(lv_event_t *event)
{
    auto *desktop = static_cast<Desktop *>(lv_event_get_user_data(event));
    auto *input = static_cast<lv_indev_t *>(lv_event_get_target(event));
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        desktop->_suppress_click = false;
        desktop->_desktop_touch = desktop->isDesktopControl(static_cast<lv_obj_t *>(lv_event_get_param(event)));
        lv_indev_get_point(input, &desktop->_touch_start);
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        if (!desktop->_desktop_touch) {
            return;
        }
        desktop->_desktop_touch = false;
        lv_point_t end{};
        lv_indev_get_point(input, &end);
        const auto dx = end.x - desktop->_touch_start.x;
        const auto dy = end.y - desktop->_touch_start.y;
        desktop->_suppress_click = std::abs(dx) >= kTapSlop || std::abs(dy) >= kTapSlop;
        if (std::abs(dx) >= kSwipeDistance && std::abs(dx) > std::abs(dy)) {
            desktop->selectPage(desktop->_current_page + (dx < 0 ? 1 : -1));
        }
        return;
    }
    if (desktop->_suppress_click) {
        lv_indev_stop_processing(input);
    }
}

void Desktop::onAppClicked(lv_event_t *event)
{
    auto *desktop = static_cast<Desktop *>(lv_event_get_user_data(event));
    auto *button = lv_event_get_current_target_obj(event);
    const auto id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button)));
    desktop->_launch(id);
}

}
