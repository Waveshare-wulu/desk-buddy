#pragma once

#include <atomic>
#include <functional>
#include "apps/db_base_app.hpp"
#include "services/wifi_time_service.hpp"
#include "ui/home/db_power_overlay.hpp"

namespace desk_buddy {

class SettingsApp : public DeskBuddyApp {
public:
    using TimeChangedCb = std::function<void()>;

    explicit SettingsApp(TimeChangedCb on_time_changed = nullptr, std::function<void()> open_power = {});
    void setLaunchDirectTime(bool direct);
    bool handleButtonGuide(ButtonEvent event);
    ~SettingsApp() override = default;

protected:
    bool run(void) override;
    bool pause(void) override;
    bool resume(void) override;
    bool close(void) override;
    bool onBackButtonPressed() override;
    void onSoundEnabledChanged() override;

private:
    enum class Tab : uint8_t {
        DISPLAY = 0,
        SOUND = 1,
        TIME = 2,
        ABOUT = 3,
    };

    enum class TimeSubView : uint8_t { MAIN, QR_PORTAL, AP_LIST, KEYBOARD, MANUAL_RTC };

    void buildUi();
    void showHub();
    void stopHubRefresh();
    void buildHub();
    void refreshHub();
    void showTab(Tab tab);
    void showButtonGuide();
    void updateTopAction(const char *text, uint32_t fg);
    void buildTabDisplay(lv_obj_t *parent);
    void buildTabSound(lv_obj_t *parent);
    lv_obj_t *buildToggle(lv_obj_t *row, int width, int height, bool checked, lv_event_cb_t callback);
    void buildTabTime(lv_obj_t *parent);
    void buildManualRtc(lv_obj_t *parent);
    void beginRtcEdit();
    void refreshRtcEditor();
    void showTimeView(TimeSubView view);
    void closeTimeView();
    void stopTimePolling();
    void refreshTimeView();
    void buildQrPage(lv_obj_t *parent);
    void buildScanPage(lv_obj_t *parent);
    void buildPasswordPage(lv_obj_t *parent);
    void refreshPasswordStatus();
    void fillScanList(const WifiTimeStatus &status);
    void buildTabAbout(lv_obj_t *parent);
    void refreshActiveTab();

    static void onBackClicked(lv_event_t *e);
    static void onTopActionClicked(lv_event_t *e);
    static void onTabClicked(lv_event_t *e);
    static void onToggleClicked(lv_event_t *e);
    static void onBrightnessChanged(lv_event_t *e);
    static void onDimSelected(lv_event_t *e);
    static void onSleepSelected(lv_event_t *e);
    static void onSoundSwitched(lv_event_t *e);
    static void onVolumeChanged(lv_event_t *e);
    static void onShakeSelected(lv_event_t *e);
    static void onRtcFocus(lv_event_t *e);
    static void onRtcStep(lv_event_t *e);
    static void onRtcApply(lv_event_t *e);
    static void onWifiPrimary(lv_event_t *e);
    static void onWifiSecondary(lv_event_t *e);
    static void onWifiAutoSync(lv_event_t *e);
    static void onTimeNavigate(lv_event_t *e);
    static void onScanRefresh(lv_event_t *e);
    static void onSelectNetwork(lv_event_t *e);
    static void onPasswordKey(lv_event_t *e);
    static void onTogglePassword(lv_event_t *e);
    static void onFactoryReset(lv_event_t *e);

    TimeChangedCb _on_time_changed;
    std::function<void()> _open_power;
    ButtonGuide _button_guide;
    std::atomic<bool> _launch_direct_time{false};
    bool _direct_time_mode = false;
    bool _in_hub = true;
    bool _in_button_guide = false;
    Tab _active_tab = Tab::DISPLAY;

    int _draft_year = 2026;
    int _draft_month = 11;
    int _draft_day = 20;
    int _draft_hour = 14;
    int _draft_min = 32;
    uint8_t _rtc_focus = 4;
    bool _rtc_saved = false;
    bool _rtc_step_repeated = false;
    bool _reset_armed = false;

    lv_obj_t *_back_btn = nullptr;
    lv_obj_t *_title_lbl = nullptr;
    lv_obj_t *_top_action = nullptr;
    lv_obj_t *_top_badge = nullptr;
    lv_obj_t *_content_box = nullptr;
    lv_obj_t *_hub_values[4] = {};
    lv_obj_t *_hub_details[4] = {};
    lv_timer_t *_hub_timer = nullptr;

    lv_obj_t *_bright_val_lbl = nullptr;
    lv_obj_t *_dim_btns[4] = {};
    lv_obj_t *_sleep_btns[4] = {};

    lv_obj_t *_sound_sub_lbl = nullptr;
    lv_obj_t *_sound_switch = nullptr;
    lv_obj_t *_volume_slider = nullptr;
    lv_obj_t *_vol_val_lbl = nullptr;
    lv_obj_t *_shake_btns[3] = {};

    lv_obj_t *_rtc_fields[5] = {};
    lv_obj_t *_rtc_focus_lbl = nullptr;
    lv_obj_t *_rtc_msg_lbl = nullptr;
    TimeSubView _time_view = TimeSubView::MAIN;
    lv_timer_t *_time_timer = nullptr;
    uint32_t _wifi_revision = UINT32_MAX;
    lv_obj_t *_time_layer = nullptr;
    lv_obj_t *_wifi_badge = nullptr;
    lv_obj_t *_wifi_summary = nullptr;
    lv_obj_t *_wifi_primary = nullptr;
    lv_obj_t *_wifi_secondary = nullptr;
    lv_obj_t *_wifi_rtc_summary = nullptr;
    lv_obj_t *_portal_status = nullptr;
    lv_obj_t *_portal_qr = nullptr;
    lv_obj_t *_scan_list = nullptr;
    lv_obj_t *_password = nullptr;
    lv_obj_t *_password_msg = nullptr;
    WifiAccessPoint _selected_network{};

    lv_obj_t *_bat_val_lbl = nullptr;
    lv_obj_t *_bat_sub_lbl = nullptr;
    lv_obj_t *_imu_val_lbl = nullptr;
    lv_obj_t *_aud_val_lbl = nullptr;
    lv_obj_t *_mem_val_lbl = nullptr;
    lv_obj_t *_mem_sub_lbl = nullptr;
    lv_obj_t *_reset_btn = nullptr;
    lv_obj_t *_reset_btn_lbl = nullptr;
};

}
