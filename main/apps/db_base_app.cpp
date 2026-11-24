#include "apps/db_base_app.hpp"

#include <cstdlib>

#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {

constexpr int kHomeSwipeWidth = 168;
constexpr int kHomeSwipeHeight = 16;
constexpr int kHomeSwipeDistance = 48;

// Lifecycle callbacks run on a Brookesia worker, outside the LVGL task.
class DisplayLock {
public:
    DisplayLock() : _locked(esp_lv_adapter_lock(-1) == ESP_OK) {}
    ~DisplayLock()
    {
        if (_locked) {
            esp_lv_adapter_unlock();
        }
    }
    explicit operator bool() const { return _locked; }

private:
    const bool _locked;
};

}

DeskBuddyApp::DeskBuddyApp(const char *id, const char *name, const lv_image_dsc_t *icon, uint8_t launcher_page)
    : _id(id), _name(name), _icon(icon), _launcher_page(launcher_page)
{
}

esp_brookesia::system::core::AppManifest DeskBuddyApp::get_manifest() const
{
    return {.id = _id, .name = _name};
}

void DeskBuddyApp::attach(lv_obj_t *app_layer, std::function<void()> request_close)
{
    _app_layer = app_layer;
    _request_close = std::move(request_close);
}

std::expected<void, std::string> DeskBuddyApp::on_start(AppContext &)
{
    const DisplayLock lock;
    if (!lock || !_app_layer) {
        return std::unexpected("Application display layer is unavailable");
    }

    _root = lv_obj_create(_app_layer);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, LV_PCT(100), LV_PCT(100));
    if (!run()) {
        close();
        lv_obj_delete(_root);
        _root = nullptr;
        return std::unexpected("Application view failed to initialize");
    }

    auto *home_edge = lv_obj_create(_root);
    lv_obj_remove_style_all(home_edge);
    lv_obj_set_size(home_edge, kHomeSwipeWidth, kHomeSwipeHeight);
    lv_obj_align(home_edge, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(home_edge, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
    lv_obj_add_flag(home_edge, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(home_edge, onHomeSwipe, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(home_edge, onHomeSwipe, LV_EVENT_RELEASED, this);

    auto *handle = lv_obj_create(home_edge);
    lv_obj_remove_style_all(handle);
    lv_obj_set_size(handle, 64, 4);
    lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, theme::color(theme::kTextMuted), 0);
    lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
    lv_obj_clear_flag(handle, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return {};
}

std::expected<void, std::string> DeskBuddyApp::on_pause(AppContext &)
{
    const DisplayLock lock;
    if (!lock || !pause()) {
        return std::unexpected("Application view failed to pause");
    }
    lv_obj_add_flag(_root, LV_OBJ_FLAG_HIDDEN);
    return {};
}

std::expected<void, std::string> DeskBuddyApp::on_resume(AppContext &)
{
    const DisplayLock lock;
    if (!lock || !resume()) {
        return std::unexpected("Application view failed to resume");
    }
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_HIDDEN);
    return {};
}

std::expected<void, std::string> DeskBuddyApp::on_stop(AppContext &)
{
    const DisplayLock lock;
    if (!lock || !close()) {
        return std::unexpected("Application view failed to close");
    }
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    return {};
}

bool DeskBuddyApp::pause()
{
    MotionService::instance().unregisterShakeListener(this);
    return true;
}

bool DeskBuddyApp::close()
{
    MotionService::instance().unregisterShakeListener(this);
    return true;
}

void DeskBuddyApp::onHomeSwipe(lv_event_t *event)
{
    auto *input = lv_event_get_indev(event);
    if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) {
        return;
    }

    auto *app = static_cast<DeskBuddyApp *>(lv_event_get_user_data(event));
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        app->_home_swipe_start = point;
        return;
    }

    const int rise = app->_home_swipe_start.y - point.y;
    if (rise >= kHomeSwipeDistance && rise > std::abs(point.x - app->_home_swipe_start.x)) {
        ESP_LOGI("DB:Navigation", "Home swipe from %s", app->_id);
        onBackBtnClicked(event);
    }
}

void DeskBuddyApp::onBackBtnClicked(lv_event_t *event)
{
    auto *app = static_cast<DeskBuddyApp *>(lv_event_get_user_data(event));
    if (app && app->_request_close) {
        SoundService::instance().play(SoundEffect::CLICK);
        app->_request_close();
    }
}

}
