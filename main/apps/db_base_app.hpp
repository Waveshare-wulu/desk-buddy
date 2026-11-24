#pragma once

#include <functional>

#include "brookesia/system_core/app/iapp.hpp"
#include "lvgl.h"
#include "services/motion_service.hpp"
#include "services/sound_service.hpp"

namespace desk_buddy {

class DeskBuddyApp : public esp_brookesia::system::core::IApp {
public:
    DeskBuddyApp(const char *id, const char *name, const lv_image_dsc_t *icon, uint8_t launcher_page);
    ~DeskBuddyApp() override = default;

    esp_brookesia::system::core::AppManifest get_manifest() const override;
    const char *name() const { return _name; }
    const lv_image_dsc_t *icon() const { return _icon; }
    uint8_t launcherPage() const { return _launcher_page; }
    void attach(lv_obj_t *app_layer, std::function<void()> request_close);
    bool isVisible() const { return _root && !lv_obj_has_flag(_root, LV_OBJ_FLAG_HIDDEN); }
    virtual bool onUserButtonPressed() { return false; }
    virtual bool onBackButtonPressed() { return false; }
    virtual void onSoundEnabledChanged() {}

protected:
    lv_obj_t *appRoot() const { return _root; }
    virtual bool run() = 0;
    virtual bool pause();
    virtual bool resume() { return true; }
    virtual bool close();
    static void onBackBtnClicked(lv_event_t *event);

private:
    using AppContext = esp_brookesia::system::core::AppContext;
    static void onHomeSwipe(lv_event_t *event);
    std::expected<void, std::string> on_start(AppContext &context) override;
    std::expected<void, std::string> on_pause(AppContext &context) override;
    std::expected<void, std::string> on_resume(AppContext &context) override;
    std::expected<void, std::string> on_stop(AppContext &context) override;

    const char *_id;
    const char *_name;
    const lv_image_dsc_t *_icon;
    uint8_t _launcher_page;
    lv_obj_t *_app_layer = nullptr;
    lv_obj_t *_root = nullptr;
    lv_point_t _home_swipe_start{};
    std::function<void()> _request_close;
};

}
