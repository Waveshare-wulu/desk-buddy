#include "settings_app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "services/clock_service.hpp"
#include "services/motion_service.hpp"
#include "services/power_service.hpp"
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "ui/assets/db_icons.h"
#include "ui/assets/db_ui_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
using namespace theme;

constexpr int kSide = kPadScreen;
constexpr int kCardWidth = kContentWidth;
constexpr int kGridWidth = kColumnWidth;
constexpr int kGridGap = kCardGap;
constexpr const char *kVersion = "v1.0.1";

struct PageStyle {
    const char *title;
    const lv_image_dsc_t *icon;
    uint32_t accent;
    uint32_t soft;
    uint32_t fg;
};

constexpr PageStyle kPages[] = {
    {"显示与护屏", &db_icon_set_display, kAccentAmber, kAccentAmberSoft, kAccentAmberFg},
    {"声音与体感", &db_icon_set_sound, kAccentCyan, kAccentCyanSoft, kAccentCyanFg},
    {"时间与校时", &db_icon_set_time, kAccentViolet, kAccentVioletSoft, kAccentVioletFg},
    {"关于与重置", &db_icon_set_about, kAccentMint, kAccentMintSoft, kAccentMintFg},
};
constexpr uint16_t kDimOptions[4] = {15, 30, 60, 0};
constexpr const char *kDimLabels[4] = {"15秒", "30秒", "60秒", "关闭"};
constexpr uint16_t kSleepOptions[4] = {60, 90, 180, 0};
constexpr const char *kSleepLabels[4] = {"60秒", "90秒", "3分", "常亮"};
constexpr const char *kShakeLabels[3] = {"低灵敏", "中灵敏", "高灵敏"};

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font,
                    uint32_t fg = kTextPrimary)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text_static(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(fg), 0);
    return label;
}

void setTextIfChanged(lv_obj_t *label, const char *text)
{
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

lv_obj_t *makeSurface(lv_obj_t *parent, int width, int height)
{
    auto *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, width, height);
    lv_obj_clear_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return obj;
}

lv_obj_t *makePageCard(lv_obj_t *parent, int y, int height)
{
    auto *card = createCard(parent, kCardWidth, height, 24);
    lv_obj_set_pos(card, kSide, y);
    lv_obj_set_style_pad_all(card, 17, 0);
    return card;
}

template <size_t N>
void buildOptions(lv_obj_t *card, int width, int height, const char *const (&labels)[N],
                  lv_obj_t *(&buttons)[N], lv_event_cb_t callback, void *app)
{
    constexpr int row_width = 408;
    const int gap = (row_width - N * width) / (N - 1);
    auto *row = makeSurface(card, row_width, height);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
    for (size_t i = 0; i < N; ++i) {
        auto *button = createPillButton(row, labels[i], kBgCanvas, kTextSecondary, width, height);
        lv_obj_set_pos(button, i * (width + gap), 0);
        lv_obj_set_user_data(button, reinterpret_cast<void *>(i));
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, app);
        buttons[i] = button;
    }
}

void styleOptionPill(lv_obj_t *btn, bool selected, uint32_t border, uint32_t bg, uint32_t fg)
{
    const auto fill = color(selected ? bg : kBgCanvas);
    lv_obj_set_style_bg_color(btn, fill, 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(fill, 18), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, color(selected ? border : kBorderSubtle), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), color(selected ? fg : kTextSecondary), 0);
}

void drawCapsuleSlider(lv_event_t *e)
{
    auto *slider = lv_event_get_target_obj(e);
    auto *layer = lv_event_get_layer(e);
    lv_area_t track{};
    lv_obj_get_coords(slider, &track);
    const int32_t width = lv_area_get_width(&track);
    const int32_t height = lv_area_get_height(&track);
    if (width <= height || height <= 12) {
        return;
    }

    const int32_t min = lv_slider_get_min_value(slider);
    const int32_t max = lv_slider_get_max_value(slider);
    const int32_t value = std::clamp(lv_slider_get_value(slider), min, max);
    const int32_t range = max - min;
    const int32_t travel = width - height;
    const int32_t fill_width = range > 0
        ? height + ((value - min) * travel + range / 2) / range
        : width;

    const lv_opa_t opa = lv_obj_get_style_opa_recursive(slider, LV_PART_MAIN);
    lv_area_t fill = track;
    fill.x2 = track.x1 + fill_width - 1;

    lv_draw_rect_dsc_t fill_dsc;
    lv_draw_rect_dsc_init(&fill_dsc);
    fill_dsc.base.layer = layer;
    fill_dsc.radius = LV_RADIUS_CIRCLE;
    fill_dsc.bg_color = lv_obj_get_style_bg_color(slider, LV_PART_INDICATOR);
    fill_dsc.bg_opa = opa;
    lv_draw_rect(layer, &fill_dsc, &fill);

    constexpr int32_t kKnobInset = 6;
    lv_area_t knob{
        .x1 = fill.x2 - height + 1 + kKnobInset,
        .y1 = track.y1 + kKnobInset,
        .x2 = fill.x2 - kKnobInset,
        .y2 = track.y2 - kKnobInset,
    };
    lv_draw_rect_dsc_t knob_dsc;
    lv_draw_rect_dsc_init(&knob_dsc);
    knob_dsc.base.layer = layer;
    knob_dsc.radius = LV_RADIUS_CIRCLE;
    knob_dsc.bg_color = color(kBgCard);
    knob_dsc.bg_opa = opa == LV_OPA_COVER ? LV_OPA_COVER : LV_OPA_80;
    knob_dsc.shadow_width = 4;
    knob_dsc.shadow_offset_y = 1;
    knob_dsc.shadow_color = color(kTextPrimary);
    knob_dsc.shadow_opa = LV_OPA_MIX2(36, opa);
    lv_draw_rect(layer, &knob_dsc, &knob);
}

lv_obj_t *makeSlider(lv_obj_t *parent, int min, int value, uint32_t accent)
{
    constexpr int kSliderHeight = 44;
    auto *slider = lv_slider_create(parent);
    lv_obj_remove_style_all(slider);
    lv_obj_set_size(slider, 408, kSliderHeight);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_style_bg_color(slider, color(kBgTrack), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(slider, kSliderHeight / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, color(accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, drawCapsuleSlider, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_slider_set_range(slider, min, 100);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    return slider;
}

}

SettingsApp::SettingsApp(TimeChangedCb on_time_changed, std::function<void()> open_power)
    : DeskBuddyApp("deskbuddy.settings", i18n::tr(i18n::StrId::APP_SETTINGS), &db_icon_settings, 2)
    , _on_time_changed(std::move(on_time_changed))
    , _open_power(std::move(open_power))
{
}

void SettingsApp::setLaunchDirectTime(bool direct)
{
    _launch_direct_time.store(direct);
}

bool SettingsApp::run(void)
{
    _direct_time_mode = _launch_direct_time.exchange(false);
    buildUi();
    if (_direct_time_mode) {
        showTab(Tab::TIME);
    } else {
        showHub();
    }
    return true;
}

bool SettingsApp::resume(void)
{
    if (_in_hub) {
        showHub();
    } else if (_in_button_guide) {
        showButtonGuide();
    } else {
        showTab(_active_tab);
    }
    return true;
}

bool SettingsApp::pause(void)
{
    _button_guide.clear();
    stopHubRefresh();
    stopTimePolling();
    closeTimeView();
    return DeskBuddyApp::pause();
}

bool SettingsApp::close(void)
{
    _launch_direct_time.store(false);
    _direct_time_mode = false;
    _button_guide.clear();
    stopHubRefresh();
    stopTimePolling();
    closeTimeView();
    _content_box = nullptr;
    return DeskBuddyApp::close();
}

void SettingsApp::buildUi()
{
    auto *root = appRoot();
    styleRootScreen(root);

    const auto top = createAppTopBar(root, "", kAccentSlate, onBackClicked, this);
    _back_btn = top.back_btn;
    _title_lbl = top.title_label;
    _top_action = createTopBarButton(top.bar, "电源", kTextPrimary);
    lv_obj_align(_top_action, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(_top_action, onTopActionClicked, LV_EVENT_CLICKED, this);
    _top_badge = createBadge(top.bar, "可试按", kBgCanvas, kTextMuted);
    lv_obj_clear_flag(_top_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(_top_badge, LV_ALIGN_RIGHT_MID, 0, 0);

    _content_box = makeSurface(root, 480, 366);
    lv_obj_set_pos(_content_box, 0, 54);
}

void SettingsApp::showHub()
{
    stopTimePolling();
    closeTimeView();
    stopHubRefresh();
    _button_guide.clear();
    _in_hub = true;
    _in_button_guide = false;
    lv_label_set_text_static(lv_obj_get_child(_back_btn, 0), "‹ 返回");
    lv_label_set_text_static(_title_lbl, i18n::tr(i18n::StrId::APP_SETTINGS));
    updateTopAction("电源", kTextPrimary);

    lv_obj_clean(_content_box);
    buildHub();
    refreshHub();
    _hub_timer = lv_timer_create([](lv_timer_t *timer) {
        static_cast<SettingsApp *>(lv_timer_get_user_data(timer))->refreshHub();
    }, 1000, this);
}

void SettingsApp::stopHubRefresh()
{
    if (_hub_timer) {
        lv_timer_delete(_hub_timer);
        _hub_timer = nullptr;
    }
}

void SettingsApp::buildHub()
{
    for (int i = 0; i < 4; ++i) {
        const auto &page = kPages[i];
        auto *card = createCard(_content_box, kGridWidth, 174, 26);
        lv_obj_set_pos(card, kSide + (i % 2) * (kGridWidth + kGridGap),
                       4 + (i / 2) * (174 + kGridGap));
        lv_obj_set_style_pad_hor(card, 17, 0);
        lv_obj_set_style_pad_ver(card, 12, 0);
        lv_obj_set_style_border_color(card, color(page.accent), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(card, color(kBgCanvas), LV_STATE_PRESSED);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(card, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(card, onTabClicked, LV_EVENT_CLICKED, this);

        auto *icon = lv_image_create(card);
        lv_image_set_src(icon, page.icon);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(makeLabel(card, page.title, fontSm(), kTextSecondary), 0, 56);
        _hub_values[i] = makeLabel(card, "", fontLg());
        lv_obj_set_pos(_hub_values[i], 0, 82);
        _hub_details[i] = makeLabel(card, "", fontSm(), kTextSecondary);
        lv_obj_set_width(_hub_details[i], 180);
        lv_label_set_long_mode(_hub_details[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(_hub_details[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
}

void SettingsApp::refreshHub()
{
    const auto &cfg = StorageService::instance().getSettings();
    const auto now = ClockService::instance().getNow();
    const auto battery = PowerService::instance().getBatteryStatus();
    char text[80];

    std::snprintf(text, sizeof(text), "%u%%", cfg.brightness);
    setTextIfChanged(_hub_values[0], text);
    char dim[24] = "关闭降亮";
    char sleep[24] = "常亮";
    if (cfg.dim_timeout_s) {
        std::snprintf(dim, sizeof(dim), "%u秒降亮", cfg.dim_timeout_s);
    }
    if (cfg.sleep_timeout_s) {
        std::snprintf(sleep, sizeof(sleep), "%u秒熄屏", cfg.sleep_timeout_s);
    }
    std::snprintf(text, sizeof(text), "%s · %s", dim, sleep);
    setTextIfChanged(_hub_details[0], text);

    std::snprintf(text, sizeof(text), "%u%%", cfg.volume);
    setTextIfChanged(_hub_values[1], cfg.sound_enabled ? text : "已静音");
    std::snprintf(text, sizeof(text), "摇一摇 · %s", kShakeLabels[cfg.shake_sensitivity % 3]);
    setTextIfChanged(_hub_details[1], text);

    std::snprintf(text, sizeof(text), "%02d:%02d", now.hour, now.minute);
    setTextIfChanged(_hub_values[2], now.valid ? text : "--:--");
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d", now.year, now.month, now.day);
    setTextIfChanged(_hub_details[2], now.valid ? text : "Wi-Fi 校时");

    std::snprintf(text, sizeof(text), "%d%%", battery.percent);
    setTextIfChanged(_hub_values[3], battery.battery_connected ? text : "--%");
    setTextIfChanged(_hub_details[3], "硬件状态 · 实体按键");
}

void SettingsApp::showTab(Tab tab)
{
    stopHubRefresh();
    stopTimePolling();
    closeTimeView();
    _button_guide.clear();
    _in_hub = false;
    _in_button_guide = false;
    _active_tab = tab;
    _reset_armed = false;

    const auto &page = kPages[static_cast<int>(tab)];
    lv_label_set_text_static(lv_obj_get_child(_back_btn, 0), _direct_time_mode ? "‹ 返回" : "‹ 设置");
    lv_label_set_text_static(_title_lbl, page.title);
    updateTopAction(tab == Tab::ABOUT ? "按键" : nullptr, kAccentMintFg);

    lv_obj_clean(_content_box);
    switch (tab) {
        case Tab::DISPLAY: buildTabDisplay(_content_box); break;
        case Tab::SOUND:   buildTabSound(_content_box); break;
        case Tab::TIME:    buildTabTime(_content_box); break;
        case Tab::ABOUT:   buildTabAbout(_content_box); break;
    }
    refreshActiveTab();
}

void SettingsApp::updateTopAction(const char *text, uint32_t fg)
{
    lv_obj_set_flag(_top_badge, LV_OBJ_FLAG_HIDDEN, !_in_button_guide);
    lv_obj_set_flag(_top_action, LV_OBJ_FLAG_HIDDEN, !text);
    if (text) {
        auto *label = lv_obj_get_child(_top_action, 0);
        lv_label_set_text_static(label, text);
        lv_obj_set_style_text_color(label, color(fg), 0);
    }
}

void SettingsApp::showButtonGuide()
{
    _button_guide.clear();
    _in_button_guide = true;
    lv_label_set_text_static(lv_obj_get_child(_back_btn, 0), "‹ 关于");
    lv_label_set_text_static(_title_lbl, "实体按键");
    updateTopAction(nullptr, kTextPrimary);
    lv_obj_clean(_content_box);
    _button_guide.build(_content_box, false);
}

bool SettingsApp::handleButtonGuide(ButtonEvent event)
{
    if (!_in_button_guide) {
        return false;
    }
    if (event.id == ButtonId::BOOT && event.press == ButtonPress::SHORT) {
        onBackButtonPressed();
    } else {
        _button_guide.highlight(event);
    }
    return true;
}

void SettingsApp::onSoundEnabledChanged()
{
    if (_in_hub) {
        refreshHub();
    } else if (!_in_button_guide && _active_tab == Tab::SOUND) {
        refreshActiveTab();
    }
}

void SettingsApp::buildTabDisplay(lv_obj_t *parent)
{
    const auto &cfg = StorageService::instance().getSettings();
    auto *brightness = makePageCard(parent, 4, 114);
    lv_obj_set_style_pad_ver(brightness, 14, 0);
    makeLabel(brightness, "屏幕亮度", fontMd());
    _bright_val_lbl = makeLabel(brightness, "", fontMd(), kAccentAmberFg);
    lv_obj_align(_bright_val_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
    auto *slider = makeSlider(brightness, 15, cfg.brightness, kAccentAmber);
    lv_obj_add_event_cb(slider, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    auto *dim = makePageCard(parent, 129, 112);
    lv_obj_set_style_pad_ver(dim, 12, 0);
    makeLabel(dim, "空闲自动降亮", fontSm());
    lv_obj_align(makeLabel(dim, "保护 AMOLED 屏幕", fontSm(), kTextMuted), LV_ALIGN_TOP_RIGHT, 0, 0);
    buildOptions(dim, 96, 54, kDimLabels, _dim_btns, onDimSelected, this);

    auto *sleep = makePageCard(parent, 252, 112);
    lv_obj_set_style_pad_ver(sleep, 12, 0);
    makeLabel(sleep, "空闲自动熄屏", fontSm());
    lv_obj_align(makeLabel(sleep, "轻触任意处唤醒", fontSm(), kTextMuted), LV_ALIGN_TOP_RIGHT, 0, 0);
    buildOptions(sleep, 96, 54, kSleepLabels, _sleep_btns, onSleepSelected, this);
}

lv_obj_t *SettingsApp::buildToggle(lv_obj_t *row, int width, int height, bool checked, lv_event_cb_t callback)
{
    auto *toggle = lv_switch_create(row);
    lv_obj_set_size(toggle, width, height);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_radius(toggle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(toggle, color(0xDDD4C5), LV_PART_MAIN);
    lv_obj_set_style_bg_color(toggle, color(kAccentMint), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP,
        static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
    lv_obj_set_style_radius(toggle, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(toggle, -4, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(toggle, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(toggle, color(kBgCard), LV_PART_KNOB);
    lv_obj_set_state(toggle, LV_STATE_CHECKED, checked);
    lv_obj_add_event_cb(toggle, callback, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_clear_flag(toggle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(row, toggle);
    lv_obj_add_event_cb(row, onToggleClicked, LV_EVENT_CLICKED, nullptr);
    return toggle;
}

void SettingsApp::onToggleClicked(lv_event_t *e)
{
    auto *toggle = static_cast<lv_obj_t *>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
    lv_obj_set_state(toggle, LV_STATE_CHECKED, !lv_obj_has_state(toggle, LV_STATE_CHECKED));
    lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, nullptr);
}

void SettingsApp::buildTabSound(lv_obj_t *parent)
{
    const auto &cfg = StorageService::instance().getSettings();
    auto *card = makePageCard(parent, 4, 196);
    lv_obj_set_style_pad_ver(card, 13, 0);

    auto *row = makeSurface(card, 408, 68);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(row, 18, 0);
    lv_obj_set_style_bg_color(row, color(kBgCanvas), 0);
    lv_obj_set_style_bg_color(row, color(kBgCardElevated), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);

    lv_obj_set_pos(makeLabel(row, "提示与木鱼音效", fontMd()), 14, 10);
    _sound_sub_lbl = makeLabel(row, "", fontSm(), kTextSecondary);
    lv_obj_set_pos(_sound_sub_lbl, 14, 38);
    _sound_switch = buildToggle(row, 72, 38, cfg.sound_enabled, onSoundSwitched);

    auto *divider = makeSurface(card, 408, 1);
    lv_obj_set_pos(divider, 0, 78);
    lv_obj_set_style_bg_color(divider, color(kBorderSubtle), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    lv_obj_set_pos(makeLabel(card, "扬声器音量", fontSm(), kTextSecondary), 0, 92);
    _vol_val_lbl = makeLabel(card, "", fontMd(), kAccentCyanFg);
    lv_obj_align(_vol_val_lbl, LV_ALIGN_TOP_RIGHT, 0, 88);
    _volume_slider = makeSlider(card, 0, cfg.volume, kAccentCyan);
    lv_obj_add_event_cb(_volume_slider, onVolumeChanged, LV_EVENT_VALUE_CHANGED, this);

    auto *shake = makePageCard(parent, 214, 146);
    lv_obj_set_style_pad_ver(shake, 13, 0);
    makeLabel(shake, "摇一摇灵敏度", fontMd());
    lv_obj_set_pos(makeLabel(shake, "用于随机实验室摇晃抽签与掷骰子", fontSm(), kTextSecondary), 0, 28);
    const bool imu_ready = MotionService::instance().isReady();
    auto *badge = createBadge(shake, imu_ready ? "IMU 正常" : "IMU 未就绪",
                             imu_ready ? kAccentMintSoft : kAccentAmberSoft,
                             imu_ready ? kAccentMintFg : kAccentAmberFg);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    buildOptions(shake, 130, 58, kShakeLabels, _shake_btns, onShakeSelected, this);
}

void SettingsApp::buildTabAbout(lv_obj_t *parent)
{
    lv_obj_t *brand = theme::createCard(parent, kCardWidth, 72, 22);
    lv_obj_set_pos(brand, kSide, 4);
    lv_obj_set_style_border_color(brand, theme::color(0xF0DFC6), 0);
    lv_obj_set_style_pad_hor(brand, 18, 0);
    lv_obj_set_style_pad_ver(brand, 11, 0);

    lv_obj_t *bn = lv_label_create(brand);
    lv_label_set_text(bn, "摸鱼搭子 Desk Buddy");
    lv_obj_set_style_text_font(bn, theme::fontMd(), 0);
    lv_obj_set_style_text_color(bn, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(bn, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *bs = lv_label_create(brand);
    lv_label_set_text(bs, "ESP32-S3-Touch-AMOLED-2.16 · 480x480");
    lv_obj_set_style_text_font(bs, theme::fontSm(), 0);
    lv_obj_set_style_text_color(bs, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(bs, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *ver = theme::createBadge(brand, kVersion, theme::kBgCard, theme::kAccentAmberFg);
    lv_obj_set_style_border_width(ver, 1, 0);
    lv_obj_set_style_border_color(ver, theme::color(0xEFE2CE), 0);
    lv_obj_align(ver, LV_ALIGN_RIGHT_MID, 0, 0);

    auto makeStatTile = [&](int x, int y, const char *title, uint32_t fg, lv_obj_t **val_out, lv_obj_t **sub_out) {
        lv_obj_t *c = theme::createCard(parent, kGridWidth, 96, 20);
        lv_obj_set_pos(c, x, y);
        lv_obj_set_style_pad_hor(c, 14, 0);
        lv_obj_set_style_pad_ver(c, 10, 0);

        lv_obj_t *t = lv_label_create(c);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_font(t, theme::fontSm(), 0);
        lv_obj_set_style_text_color(t, theme::color(theme::kTextMuted), 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *v = lv_label_create(c);
        lv_obj_set_style_text_font(v, theme::fontMd(), 0);
        lv_obj_set_style_text_color(v, theme::color(fg), 0);
        lv_obj_align(v, LV_ALIGN_LEFT_MID, 0, 0);
        *val_out = v;

        if (sub_out) {
            lv_obj_t *s = lv_label_create(c);
            lv_obj_set_style_text_font(s, theme::fontSm(), 0);
            lv_obj_set_style_text_color(s, theme::color(theme::kTextSecondary), 0);
            lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 0, 0);
            *sub_out = s;
        }
    };

    lv_obj_t *imu_sub = nullptr;
    lv_obj_t *aud_sub = nullptr;
    makeStatTile(kSide, 88,  "电池电量",   theme::kAccentMintFg,   &_bat_val_lbl, &_bat_sub_lbl);
    makeStatTile(kSide + kGridWidth + kGridGap, 88,  "六轴传感器", theme::kAccentVioletFg, &_imu_val_lbl, &imu_sub);
    makeStatTile(kSide, 196, "音频芯片",   theme::kAccentCyanFg,   &_aud_val_lbl, &aud_sub);
    makeStatTile(kSide + kGridWidth + kGridGap, 196, "可用内存",   theme::kAccentAmberFg,  &_mem_val_lbl, &_mem_sub_lbl);
    if (imu_sub) lv_label_set_text(imu_sub, "QMI8658 IMU");
    if (aud_sub) lv_label_set_text(aud_sub, "ES8311 扬声器");

    _reset_btn = theme::createPillButton(parent, "恢复默认设置与清空记录", theme::kAccentCoralSoft, theme::kAccentCoralFg, kCardWidth, 60, theme::fontMd());
    lv_obj_set_style_radius(_reset_btn, 18, 0);
    lv_obj_set_style_border_width(_reset_btn, 1, 0);
    lv_obj_set_style_border_color(_reset_btn, theme::color(theme::kBorderCoral), 0);
    lv_obj_align(_reset_btn, LV_ALIGN_TOP_MID, 0, 304);
    lv_obj_add_event_cb(_reset_btn, onFactoryReset, LV_EVENT_CLICKED, this);
    _reset_btn_lbl = lv_obj_get_child(_reset_btn, 0);
}

void SettingsApp::refreshActiveTab()
{
    const auto &cfg = StorageService::instance().getSettings();
    char buf[96];

    if (_active_tab == Tab::DISPLAY) {
        std::snprintf(buf, sizeof(buf), "%u%%", cfg.brightness);
        setTextIfChanged(_bright_val_lbl, buf);
        for (int i = 0; i < 4; ++i) {
            styleOptionPill(_dim_btns[i], cfg.dim_timeout_s == kDimOptions[i],
                            kAccentAmber, kAccentAmberSoft, kAccentAmberFg);
            styleOptionPill(_sleep_btns[i], cfg.sleep_timeout_s == kSleepOptions[i],
                            kAccentMint, kAccentMintSoft, kAccentMintFg);
        }
    } else if (_active_tab == Tab::SOUND) {
        setTextIfChanged(_sound_sub_lbl, !cfg.sound_enabled ? "已静音 · 点击恢复" :
                        SoundService::instance().isHardwareReady() ? "整行点击可切换静音" :
                        "无扬声器模式 · 点击静音");
        lv_obj_set_state(_sound_switch, LV_STATE_CHECKED, cfg.sound_enabled);
        lv_obj_set_style_opa(_volume_slider, cfg.sound_enabled ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_set_style_text_color(_vol_val_lbl, color(cfg.sound_enabled ? kAccentCyanFg : kTextMuted), 0);
        std::snprintf(buf, sizeof(buf), "%u%%", cfg.volume);
        setTextIfChanged(_vol_val_lbl, buf);
        for (int i = 0; i < 3; ++i) {
            styleOptionPill(_shake_btns[i], (cfg.shake_sensitivity % 3) == i,
                            kAccentViolet, kAccentVioletSoft, kAccentVioletFg);
        }
    } else if (_active_tab == Tab::ABOUT) {
        const auto bat = PowerService::instance().getBatteryStatus();
        const size_t sram_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
        const size_t psram_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;

        if (_bat_val_lbl) {
            std::snprintf(buf, sizeof(buf), "%d%%", bat.percent);
            lv_label_set_text(_bat_val_lbl, bat.battery_connected ? buf : "--%");
        }
        if (_bat_sub_lbl) {
            std::snprintf(buf, sizeof(buf), "%umV · %s",
                          bat.batt_mv, bat.charging ? "充电中" : (bat.vbus_in ? "USB供电" : "电池"));
            lv_label_set_text(_bat_sub_lbl, buf);
        }
        if (_imu_val_lbl) {
            lv_label_set_text(_imu_val_lbl, MotionService::instance().isReady() ? "已就绪" : "未就绪");
        }
        if (_aud_val_lbl) {
            lv_label_set_text(_aud_val_lbl, SoundService::instance().isHardwareReady() ? "已就绪" : "静音降级");
        }
        if (_mem_val_lbl) {
            std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(psram_kb) / 1024.0);
            lv_label_set_text(_mem_val_lbl, buf);
        }
        if (_mem_sub_lbl) {
            std::snprintf(buf, sizeof(buf), "SRAM %u KB", static_cast<unsigned>(sram_kb));
            lv_label_set_text(_mem_sub_lbl, buf);
        }
        if (_reset_btn && _reset_btn_lbl) {
            lv_obj_set_style_bg_color(_reset_btn, theme::color(
                _reset_armed ? theme::kAccentCoral : theme::kAccentCoralSoft), 0);
            lv_obj_set_style_bg_color(_reset_btn, lv_color_darken(color(
                _reset_armed ? kAccentCoral : kAccentCoralSoft), 22), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(_reset_btn_lbl, theme::color(
                _reset_armed ? theme::kTextOnAccent : theme::kAccentCoralFg), 0);
            lv_label_set_text(_reset_btn_lbl, _reset_armed ? "再次点击确认清空全部数据!" : "恢复默认设置与清空记录");
        }
    }
}

bool SettingsApp::onBackButtonPressed()
{
    if (_in_hub) {
        return false;
    }
    if (_in_button_guide) {
        showTab(Tab::ABOUT);
    } else if (_time_view != TimeSubView::MAIN) {
        switch (_time_view) {
            case TimeSubView::KEYBOARD:   showTimeView(TimeSubView::AP_LIST); break;
            case TimeSubView::AP_LIST:    showTimeView(TimeSubView::QR_PORTAL); break;
            default:                     showTimeView(TimeSubView::MAIN); break;
        }
    } else if (_direct_time_mode) {
        return false;
    } else {
        showHub();
    }
    return true;
}

void SettingsApp::onBackClicked(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    if (self->onBackButtonPressed()) {
        SoundService::instance().play(SoundEffect::CLICK);
    } else {
        DeskBuddyApp::onBackBtnClicked(e);
    }
}

void SettingsApp::onTopActionClicked(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    if (self->_in_hub && self->_open_power) {
        self->_open_power();
    } else if (!self->_in_button_guide && self->_active_tab == Tab::ABOUT) {
        self->showButtonGuide();
    }
}

void SettingsApp::onTabClicked(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const auto idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    self->showTab(static_cast<Tab>(idx));
}

void SettingsApp::onBrightnessChanged(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *sl = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !sl) return;
    const auto val = static_cast<uint8_t>(lv_slider_get_value(sl));
    PowerService::instance().setBrightness(val);
    auto cfg = StorageService::instance().getSettings();
    cfg.brightness = val;
    StorageService::instance().updateSettings(cfg);
    self->refreshActiveTab();
}

void SettingsApp::onDimSelected(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const auto idx = static_cast<int>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    auto cfg = StorageService::instance().getSettings();
    cfg.dim_timeout_s = kDimOptions[idx % 4];
    StorageService::instance().updateSettings(cfg);
    PowerService::instance().setTimeouts(cfg.dim_timeout_s, cfg.sleep_timeout_s);
    self->refreshActiveTab();
}

void SettingsApp::onSleepSelected(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const auto idx = static_cast<int>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    auto cfg = StorageService::instance().getSettings();
    cfg.sleep_timeout_s = kSleepOptions[idx % 4];
    StorageService::instance().updateSettings(cfg);
    PowerService::instance().setTimeouts(cfg.dim_timeout_s, cfg.sleep_timeout_s);
    self->refreshActiveTab();
}

void SettingsApp::onSoundSwitched(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !sw) return;
    const bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    auto cfg = StorageService::instance().getSettings();
    cfg.sound_enabled = enabled;
    StorageService::instance().updateSettings(cfg);
    SoundService::instance().setEnabled(enabled);
    if (enabled) {
        SoundService::instance().play(SoundEffect::CLICK);
    }
    self->refreshActiveTab();
}

void SettingsApp::onVolumeChanged(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *sl = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !sl) return;
    const auto val = static_cast<uint8_t>(lv_slider_get_value(sl));
    auto cfg = StorageService::instance().getSettings();
    const bool was_muted = !cfg.sound_enabled;
    cfg.volume = val;
    cfg.sound_enabled = true;
    StorageService::instance().updateSettings(cfg);
    SoundService::instance().setVolume(val);
    if (was_muted) {
        SoundService::instance().setEnabled(true);
    }
    self->refreshActiveTab();
}

void SettingsApp::onShakeSelected(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const auto idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    auto cfg = StorageService::instance().getSettings();
    cfg.shake_sensitivity = idx % 3;
    StorageService::instance().updateSettings(cfg);
    MotionService::instance().setSensitivity(static_cast<ShakeSensitivity>(cfg.shake_sensitivity));
    self->refreshActiveTab();
}

void SettingsApp::onFactoryReset(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (!self->_reset_armed) {
        self->_reset_armed = true;
        self->refreshActiveTab();
        return;
    }
    self->_reset_armed = false;
    WifiTimeService::instance().resetSettings();
    const auto &cfg = StorageService::instance().getSettings();
    PowerService::instance().setBrightness(cfg.brightness);
    PowerService::instance().setTimeouts(cfg.dim_timeout_s, cfg.sleep_timeout_s);
    SoundService::instance().setEnabled(cfg.sound_enabled);
    SoundService::instance().setVolume(cfg.volume);
    MotionService::instance().setSensitivity(static_cast<ShakeSensitivity>(cfg.shake_sensitivity));
    self->refreshActiveTab();
}

}
