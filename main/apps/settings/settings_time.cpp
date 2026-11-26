#include "settings_app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui/theme/db_theme.hpp"

namespace desk_buddy {
namespace {

using namespace theme;

constexpr const char *kRtcFields[] = {"年", "月", "日", "时", "分"};
constexpr int kRtcSteps[] = {-5, -1, 1, 5};

int wrapRtcField(int value, int min, int max)
{
    const int range = max - min + 1;
    return ((value - min) % range + range) % range + min;
}

constexpr const char *kLowerKeys[] = {
    "a", "b", "c", "d", "e", "f", "g", "\n",
    "h", "i", "j", "k", "l", "m", "n", "\n",
    "o", "p", "q", "r", "s", "t", "u", "\n",
    "⇧", "v", "w", "x", "y", "z", "删除", "\n",
    "123", "符号", "空格", "连接", ""
};
constexpr const char *kUpperKeys[] = {
    "A", "B", "C", "D", "E", "F", "G", "\n",
    "H", "I", "J", "K", "L", "M", "N", "\n",
    "O", "P", "Q", "R", "S", "T", "U", "\n",
    "⇩", "V", "W", "X", "Y", "Z", "删除", "\n",
    "123", "符号", "空格", "连接", ""
};
constexpr const char *kNumberKeys[] = {
    "1", "2", "3", "4", "5", "\n",
    "6", "7", "8", "9", "0", "\n",
    ".", "-", "_", "@", "#", "!", "\n",
    "+", "=", "/", "?", "*", "删除", "\n",
    "abc", "符号", "空格", "连接", ""
};
constexpr const char *kSymbolKeys[] = {
    "!", "@", "#", "$", "%", "^", "&", "\n",
    "*", "(", ")", "[", "]", "{", "}", "\n",
    "<", ">", ":", ";", ",", "~", "`", "\n",
    "\\", "|", "'", "\"", ".", "删除", "\n",
    "abc", "123", "空格", "连接", ""
};

lv_obj_t *labelAt(lv_obj_t *parent, const char *text, int x, int y,
                  const lv_font_t *font = fontSm(), uint32_t foreground = kTextPrimary)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(foreground), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

void styleButton(lv_obj_t *button, uint32_t background, uint32_t foreground)
{
    lv_obj_set_style_bg_color(button, color(background), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(button, 0), color(foreground), 0);
}

void setButton(lv_obj_t *button, const char *text)
{
    lv_label_set_text(lv_obj_get_child(button, 0), text);
}

void useKeymap(lv_obj_t *keys, const char *const *map)
{
    lv_buttonmatrix_set_map(keys, map);
    uint32_t index = 0;
    for (; *map && **map; ++map) {
        if (std::strcmp(*map, "\n") == 0) {
            continue;
        }
        const std::string_view label(*map);
        if (label == "连接") {
            lv_buttonmatrix_set_button_ctrl(keys, index,
                static_cast<lv_buttonmatrix_ctrl_t>(
                    LV_BUTTONMATRIX_CTRL_CUSTOM_1 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_NO_REPEAT));
            lv_buttonmatrix_set_button_width(keys, index, 3);
        } else if (label == "删除") {
            lv_buttonmatrix_set_button_ctrl(keys, index, LV_BUTTONMATRIX_CTRL_CUSTOM_2);
            lv_buttonmatrix_set_button_width(keys, index, 2);
        } else if (label == "123" || label == "abc" || label == "符号" || label == "空格") {
            lv_buttonmatrix_set_button_ctrl(keys, index,
                static_cast<lv_buttonmatrix_ctrl_t>(
                    LV_BUTTONMATRIX_CTRL_CUSTOM_2 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_NO_REPEAT));
            lv_buttonmatrix_set_button_width(keys, index, 2);
        } else if (label == "⇧" || label == "⇩") {
            lv_buttonmatrix_set_button_ctrl(keys, index,
                static_cast<lv_buttonmatrix_ctrl_t>(
                    LV_BUTTONMATRIX_CTRL_CUSTOM_2 | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_NO_REPEAT));
            lv_buttonmatrix_set_button_width(keys, index, 1);
        } else {
            lv_buttonmatrix_set_button_ctrl(keys, index,
                static_cast<lv_buttonmatrix_ctrl_t>(LV_BUTTONMATRIX_CTRL_POPOVER | LV_BUTTONMATRIX_CTRL_NO_REPEAT));
            lv_buttonmatrix_set_button_width(keys, index, 1);
        }
        ++index;
    }
    lv_buttonmatrix_set_selected_button(keys, LV_BUTTONMATRIX_BUTTON_NONE);
}

bool syncInProgress(WifiSyncState state)
{
    return state == WifiSyncState::CONNECTING || state == WifiSyncState::SYNCING_SNTP;
}

}

void SettingsApp::buildTabTime(lv_obj_t *parent)
{
    auto *card = createCard(parent, 444, 244, 24);
    lv_obj_set_pos(card, 18, 4);
    lv_obj_set_style_pad_hor(card, 17, 0);
    lv_obj_set_style_pad_ver(card, 14, 0);
    labelAt(card, "Wi-Fi 网络校时", 0, 1, fontMd());
    _wifi_badge = createBadge(card, "", kAccentAmberSoft, kAccentAmberFg);
    lv_obj_align(_wifi_badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    auto *badge_text = lv_obj_get_child(_wifi_badge, 0);
    lv_obj_set_width(badge_text, 150);
    lv_label_set_long_mode(badge_text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(badge_text, LV_TEXT_ALIGN_CENTER, 0);

    auto *summary = createCard(card, 408, 44, 14);
    lv_obj_set_pos(summary, 0, 36);
    lv_obj_set_style_pad_all(summary, 10, 0);
    lv_obj_set_style_shadow_width(summary, 0, 0);
    lv_obj_set_style_bg_color(summary, color(kBgCanvas), 0);
    _wifi_summary = labelAt(summary, "", 0, 0, fontSm(), kTextSecondary);
    lv_obj_set_width(_wifi_summary, 316);
    lv_label_set_long_mode(_wifi_summary, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(_wifi_summary, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(labelAt(summary, "UTC+8", 0, 0, fontSm(), kTextSecondary), LV_ALIGN_RIGHT_MID, 0, 0);

    _wifi_primary = createPillButton(card, "", kAccentCyanFg, kTextOnAccent, 260, 54);
    lv_obj_set_pos(_wifi_primary, 0, 90);
    lv_obj_add_event_cb(_wifi_primary, onWifiPrimary, LV_EVENT_CLICKED, this);
    _wifi_secondary = createPillButton(card, "", kBgCardElevated, kTextPrimary, 140, 54);
    lv_obj_set_pos(_wifi_secondary, 268, 90);
    lv_obj_add_event_cb(_wifi_secondary, onWifiSecondary, LV_EVENT_CLICKED, this);

    auto *auto_sync = createCard(card, 408, 60, 16);
    lv_obj_set_pos(auto_sync, 0, 154);
    lv_obj_set_style_pad_all(auto_sync, 0, 0);
    lv_obj_set_style_shadow_width(auto_sync, 0, 0);
    lv_obj_set_style_bg_color(auto_sync, color(kBgCanvas), 0);
    lv_obj_set_style_bg_color(auto_sync, color(kBgCardElevated), LV_STATE_PRESSED);
    labelAt(auto_sync, "开机自动校时", 14, 8);
    labelAt(auto_sync, "使用已保存的 Wi-Fi，校时后自动断开", 14, 30, fontSm(), kTextSecondary);
    buildToggle(auto_sync, 68, 36, StorageService::instance().getSettings().wifi_auto_sync, onWifiAutoSync);

    auto *offline = createCard(parent, 444, 98, 24);
    lv_obj_set_pos(offline, 18, 260);
    lv_obj_set_style_pad_all(offline, 17, 0);
    lv_obj_set_style_border_color(offline, color(kAccentAmber), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(offline, color(kBgCanvas), LV_STATE_PRESSED);
    labelAt(offline, "离线手动校准 RTC", 0, 5, fontMd());
    _wifi_rtc_summary = labelAt(offline, "", 0, 37, fontSm(), kTextSecondary);
    auto *action = createBadge(offline, "去校准", kAccentAmberSoft, kAccentAmberFg);
    lv_obj_clear_flag(action, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(action, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(offline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(offline, reinterpret_cast<void *>(static_cast<uintptr_t>(TimeSubView::MANUAL_RTC)));
    lv_obj_add_event_cb(offline, onTimeNavigate, LV_EVENT_CLICKED, this);

    _wifi_revision = UINT32_MAX;
    refreshTimeView();
    _time_timer = lv_timer_create([](lv_timer_t *timer) {
        static_cast<SettingsApp *>(lv_timer_get_user_data(timer))->refreshTimeView();
    }, 400, this);
}

void SettingsApp::stopTimePolling()
{
    if (_time_timer) {
        lv_timer_delete(_time_timer);
        _time_timer = nullptr;
    }
}

void SettingsApp::closeTimeView()
{
    if (_time_view == TimeSubView::QR_PORTAL || _time_view == TimeSubView::AP_LIST) {
        WifiTimeService::instance().cancelProvisioning();
    }
    if (_time_layer) {
        lv_obj_delete(_time_layer);
        _time_layer = nullptr;
    }
    _time_view = TimeSubView::MAIN;
    _rtc_focus_lbl = _rtc_msg_lbl = nullptr;
    for (auto &field : _rtc_fields) {
        field = nullptr;
    }
    _portal_status = _portal_qr = _scan_list = nullptr;
    _password = _password_msg = nullptr;
}

void SettingsApp::showTimeView(TimeSubView view)
{
    const auto previous_view = _time_view;
    closeTimeView();
    _wifi_revision = UINT32_MAX;
    if (view == TimeSubView::MAIN) {
        refreshTimeView();
        return;
    }

    _time_view = view;
    _time_layer = lv_obj_create(appRoot());
    lv_obj_remove_style_all(_time_layer);
    lv_obj_set_size(_time_layer, 480, 424);
    styleRootScreen(_time_layer);
    lv_obj_set_style_radius(_time_layer, 0, 0);

    auto *body = lv_obj_create(_time_layer);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 480, 424 - kTopBarHeight);
    lv_obj_set_pos(body, 0, kTopBarHeight);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    auto &wifi = WifiTimeService::instance();
    TopBarRefs top{};
    TimeSubView back = TimeSubView::MAIN;
    const char *back_text = "‹ 返回";
    switch (view) {
        case TimeSubView::QR_PORTAL:
            top = createAppTopBar(_time_layer, "手机扫码配网", kAccentCyanFg, onTimeNavigate, this);
            buildQrPage(body);
            wifi.startCaptivePortal();
            break;
        case TimeSubView::AP_LIST:
            top = createAppTopBar(_time_layer, "选择附近 Wi-Fi", kAccentCyanFg,
                onTimeNavigate, this, "刷新", onScanRefresh);
            back = TimeSubView::QR_PORTAL;
            back_text = "‹ 扫码";
            buildScanPage(body);
            if (previous_view != TimeSubView::KEYBOARD) {
                wifi.startApScan();
            }
            break;
        case TimeSubView::KEYBOARD:
            top = createAppTopBar(_time_layer, _selected_network.ssid, kAccentCyanFg,
                onTimeNavigate, this, "隐藏", onTogglePassword);
            back = TimeSubView::AP_LIST;
            back_text = "‹ 列表";
            buildPasswordPage(body);
            break;
        case TimeSubView::MANUAL_RTC:
            top = createAppTopBar(_time_layer, "离线手动校准", kAccentAmberFg, onTimeNavigate, this);
            beginRtcEdit();
            buildManualRtc(body);
            break;
        case TimeSubView::MAIN:
            return;
    }
    setButton(top.back_btn, back_text);
    lv_obj_set_user_data(top.back_btn, reinterpret_cast<void *>(static_cast<uintptr_t>(back)));
    refreshTimeView();
}

void SettingsApp::refreshTimeView()
{
    const auto status = WifiTimeService::instance().status();
    const bool changed = _wifi_revision != status.revision;
    _wifi_revision = status.revision;

    if (_time_view == TimeSubView::QR_PORTAL && changed) {
        if (syncInProgress(status.state) || status.state == WifiSyncState::SUCCESS ||
            status.state == WifiSyncState::FAILED) {
            showTimeView(TimeSubView::MAIN);
            return;
        }
        lv_label_set_text(lv_obj_get_child(_portal_status, 0), status.message);
        if (status.state == WifiSyncState::PORTAL_ACTIVE) {
            lv_obj_clear_flag(_portal_qr, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (_time_view == TimeSubView::AP_LIST && changed) {
        fillScanList(status);
        return;
    }
    if (_time_view != TimeSubView::MAIN) {
        return;
    }

    if (changed) {
        const bool configured = status.saved_ssid[0] != '\0';
        const bool busy = syncInProgress(status.state);
        auto *badge = lv_obj_get_child(_wifi_badge, 0);
        lv_label_set_text(badge, busy ? "同步中..." : configured ? status.saved_ssid : "未配网");
        lv_obj_set_style_bg_color(_wifi_badge,
            color(busy ? kAccentCyanSoft : configured ? kAccentMintSoft : kAccentAmberSoft), 0);
        lv_obj_set_style_text_color(badge,
            color(busy ? kAccentCyanFg : configured ? kAccentMintFg : kAccentAmberFg), 0);

        if (status.message[0] && status.state != WifiSyncState::SUCCESS) {
            lv_label_set_text(_wifi_summary, status.message);
        } else if (const time_t epoch = status.last_sync_epoch; epoch > 0) {
            struct tm time{};
            localtime_r(&epoch, &time);
            const char *source = status.last_sync_src == TimeSyncSource::SNTP ? "SNTP" :
                                 status.last_sync_src == TimeSyncSource::PHONE ? "手机" : "手动";
            if (status.state == WifiSyncState::SUCCESS) {
                lv_label_set_text_fmt(_wifi_summary, "校时成功 %02d:%02d (%s) · 已断网",
                                      time.tm_hour, time.tm_min, source);
            } else {
                lv_label_set_text_fmt(_wifi_summary, "上次同步 %02d-%02d %02d:%02d (%s)",
                                      time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, source);
            }
        } else {
            lv_label_set_text(_wifi_summary, configured ? "校时后自动断开 Wi-Fi" : "还未配置 Wi-Fi");
        }
        lv_obj_set_style_text_color(_wifi_summary,
            color(status.state == WifiSyncState::FAILED ? kAccentCoralFg : kTextSecondary), 0);

        setButton(_wifi_primary, busy ? "正在连接并同步..." :
                  configured ? "立即 Wi-Fi 校时" : "手机扫码配网 / 校时");
        styleButton(_wifi_primary, configured ? kAccentMintFg : kAccentCyanFg, kTextOnAccent);
        setButton(_wifi_secondary, configured ? "更换 Wi-Fi" : "手动输入");
        for (auto *button : {_wifi_primary, _wifi_secondary}) {
            if (busy) {
                lv_obj_add_state(button, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(button, LV_STATE_DISABLED);
            }
        }
        if (status.state == WifiSyncState::SUCCESS && _on_time_changed) {
            _on_time_changed();
        }
    }

    const auto now = ClockService::instance().getNow();
    char clock_summary[42];
    if (now.valid) {
        std::snprintf(clock_summary, sizeof(clock_summary), "当前 %04d-%02d-%02d %02d:%02d",
                      now.year, now.month, now.day, now.hour, now.minute);
    } else {
        std::snprintf(clock_summary, sizeof(clock_summary), "时间未校准");
    }
    if (std::strcmp(clock_summary, lv_label_get_text(_wifi_rtc_summary)) != 0) {
        lv_label_set_text(_wifi_rtc_summary, clock_summary);
    }
}

void SettingsApp::beginRtcEdit()
{
    constexpr uint8_t minute_field = 4;
    _rtc_focus = minute_field;
    _rtc_saved = false;
    _rtc_step_repeated = false;
    const auto now = ClockService::instance().getNow();
    _draft_year = now.valid ? now.year : 2026;
    _draft_month = now.valid ? now.month : 11;
    _draft_day = now.valid ? now.day : 20;
    _draft_hour = now.valid ? now.hour : 14;
    _draft_min = now.valid ? now.minute : 32;
}

void SettingsApp::buildManualRtc(lv_obj_t *parent)
{
    constexpr int widths[] = {156, 134, 134, 216, 216};
    constexpr int positions[] = {18, 184, 328, 18, 246};
    for (int i = 0; i < 5; ++i) {
        const bool time_field = i >= 3;
        auto *field = createCard(parent, widths[i], time_field ? 82 : 76, time_field ? 22 : 20);
        lv_obj_set_pos(field, positions[i], time_field ? 90 : 4);
        lv_obj_set_style_pad_all(field, 0, 0);
        lv_obj_set_style_bg_color(field, color(kBgCanvas), LV_STATE_PRESSED);
        lv_obj_add_flag(field, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(field, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(field, onRtcFocus, LV_EVENT_CLICKED, this);
        _rtc_fields[i] = field;

        auto *title = labelAt(field, kRtcFields[i], 0, 0, time_field ? fontMd() : fontSm(), kTextSecondary);
        auto *value = labelAt(field, "", 0, 0, time_field ? fontXl() : fontLg());
        if (time_field) {
            lv_obj_align(title, LV_ALIGN_LEFT_MID, 22, 0);
            lv_obj_align(value, LV_ALIGN_RIGHT_MID, -22, 0);
        } else {
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
            lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, -7);
        }
    }

    _rtc_focus_lbl = labelAt(parent, "", 22, 182, fontSm(), kAccentVioletFg);
    _rtc_msg_lbl = labelAt(parent, "", 0, 0, fontSm(), kTextMuted);
    lv_obj_align(_rtc_msg_lbl, LV_ALIGN_TOP_RIGHT, -22, 182);

    constexpr const char *step_labels[] = {"-5", "-1", "+1", "+5"};
    for (int i = 0; i < 4; ++i) {
        auto *step = createPillButton(parent, step_labels[i], kBgCardElevated, kTextPrimary, 102, 62, fontLg());
        lv_obj_set_pos(step, 18 + i * 114, 212);
        lv_obj_set_style_radius(step, 18, 0);
        lv_obj_set_style_border_width(step, 1, 0);
        lv_obj_set_style_border_color(step, color(0xE2D5C2), 0);
        lv_obj_set_user_data(step, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(step, onRtcStep, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(step, onRtcStep, LV_EVENT_LONG_PRESSED_REPEAT, this);
        lv_obj_add_event_cb(step, onRtcStep, LV_EVENT_CLICKED, this);
    }

    auto *save = createPillButton(parent, "保存并写入板载时钟", kAccentMint, kTextOnAccent, 444, 64, fontMd());
    lv_obj_set_style_radius(save, 22, 0);
    lv_obj_set_pos(save, 18, 288);
    lv_obj_add_event_cb(save, onRtcApply, LV_EVENT_CLICKED, this);
    refreshRtcEditor();
}

void SettingsApp::refreshRtcEditor()
{
    const int values[] = {_draft_year, _draft_month, _draft_day, _draft_hour, _draft_min};
    for (int i = 0; i < 5; ++i) {
        auto *field = _rtc_fields[i];
        const bool selected = i == _rtc_focus;
        const bool time_field = i >= 3;
        const uint32_t accent = time_field ? kAccentAmber : kAccentViolet;
        const uint32_t fg = time_field ? kAccentAmberFg : kAccentVioletFg;
        const uint32_t soft = time_field ? kAccentAmberSoft : kAccentVioletSoft;
        lv_obj_set_style_bg_color(field, color(selected ? soft : kBgCard), 0);
        lv_obj_set_style_border_width(field, 2, 0);
        lv_obj_set_style_border_color(field, color(selected ? accent : kBorderSubtle), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(field, 0), color(selected ? fg : kTextSecondary), 0);
        auto *value = lv_obj_get_child(field, 1);
        lv_obj_set_style_text_color(value, color(selected && time_field ? fg : kTextPrimary), 0);
        char text[16];
        std::snprintf(text, sizeof(text), i == 0 ? "%04d" : "%02d", values[i]);
        if (std::strcmp(lv_label_get_text(value), text) != 0) {
            lv_label_set_text(value, text);
        }
    }
    lv_label_set_text_fmt(_rtc_focus_lbl, "正在调节：%s", kRtcFields[_rtc_focus]);
    lv_obj_set_style_text_color(_rtc_focus_lbl, color(_rtc_focus >= 3 ? kAccentAmberFg : kAccentVioletFg), 0);
    lv_label_set_text_static(_rtc_msg_lbl, _rtc_saved ? "已写入板载时钟!" : "支持长按连发");
    lv_obj_set_style_text_color(_rtc_msg_lbl, color(_rtc_saved ? kAccentMintFg : kTextMuted), 0);
}

void SettingsApp::onRtcFocus(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *field = lv_event_get_target_obj(e);
    self->_rtc_focus = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(field)));
    SoundService::instance().play(SoundEffect::CLICK);
    self->refreshRtcEditor();
}

void SettingsApp::onRtcStep(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    const auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        self->_rtc_step_repeated = false;
        return;
    }
    if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
        self->_rtc_step_repeated = true;
    } else {
        if (self->_rtc_step_repeated) {
            return;
        }
        SoundService::instance().play(SoundEffect::CLICK);
    }

    auto *button = lv_event_get_target_obj(e);
    const auto index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button));
    const int delta = kRtcSteps[index];
    auto &year = self->_draft_year;
    auto &month = self->_draft_month;
    auto &day = self->_draft_day;
    auto &hour = self->_draft_hour;
    auto &minute = self->_draft_min;
    switch (self->_rtc_focus) {
        case 0: year = std::clamp(year + delta, 2024, 2099); break;
        case 1: month = wrapRtcField(month + delta, 1, 12); break;
        case 2: day = wrapRtcField(day + delta, 1, ClockService::daysInMonth(year, month)); break;
        case 3: hour = wrapRtcField(hour + delta, 0, 23); break;
        case 4: minute = wrapRtcField(minute + delta, 0, 59); break;
    }
    day = std::min(day, ClockService::daysInMonth(year, month));
    self->_rtc_saved = false;
    self->refreshRtcEditor();
}

void SettingsApp::onRtcApply(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    if (!self) return;
    self->_rtc_saved = false;
    auto &wifi = WifiTimeService::instance();
    const DateTime draft{
        .year = self->_draft_year, .month = self->_draft_month, .day = self->_draft_day,
        .hour = self->_draft_hour, .minute = self->_draft_min,
    };
    if (wifi.setManualTime(draft)) {
        SoundService::instance().play(SoundEffect::COMPLETE);
        self->_rtc_saved = true;
        self->refreshRtcEditor();
        if (self->_on_time_changed) {
            self->_on_time_changed();
        }
    } else {
        const auto state = wifi.status().state;
        lv_label_set_text(self->_rtc_msg_lbl,
            state == WifiSyncState::CONNECTING || state == WifiSyncState::SYNCING_SNTP
                ? "网络校时中，稍后再试" : "写入失败，请重试");
        lv_obj_set_style_text_color(self->_rtc_msg_lbl, theme::color(theme::kAccentCoralFg), 0);
    }
}

void SettingsApp::buildQrPage(lv_obj_t *parent)
{
    auto *card = createCard(parent, kContentWidth, 300, 24);
    lv_obj_set_pos(card, kPadScreen, 8);
    lv_obj_set_style_pad_hor(card, 16, 0);
    lv_obj_set_style_pad_ver(card, 12, 0);
    _portal_status = createBadge(card, "", kAccentMintSoft, kAccentMintFg);
    lv_obj_align(_portal_status, LV_ALIGN_TOP_LEFT, 0, 0);
    auto *ip = labelAt(card, "192.168.4.1", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(ip, LV_ALIGN_TOP_RIGHT, 0, 4);

    auto *frame = createCard(card, 168, 168, 16);
    lv_obj_set_pos(frame, 0, 36);
    lv_obj_set_style_pad_all(frame, 0, 0);
    _portal_qr = lv_qrcode_create(frame);
    lv_qrcode_set_size(_portal_qr, 148);
    lv_qrcode_set_dark_color(_portal_qr, color(kTextPrimary));
    lv_qrcode_set_light_color(_portal_qr, color(0xFFFFFF));
    lv_obj_center(_portal_qr);
    const auto *hotspot = WifiTimeService::instance().hotspotName();
    char qr[48];
    const int size = std::snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", hotspot);
    if (lv_qrcode_update(_portal_qr, qr, size) != LV_RESULT_OK) {
        labelAt(frame, "可手动加入热点", 15, 72, fontSm(), kTextSecondary);
    }
    lv_obj_add_flag(_portal_qr, LV_OBJ_FLAG_HIDDEN);
    auto *ssid = createBadge(card, hotspot, kAccentAmberSoft, kAccentAmberFg);
    lv_obj_update_layout(card);
    lv_obj_align_to(ssid, frame, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    labelAt(card, "1  相机扫码加入热点", 184, 50, fontMd(), kAccentCyanFg);
    labelAt(card, "2  打开弹出的配网页", 184, 104, fontMd(), kAccentAmberFg);
    labelAt(card, "也可在浏览器输入上方地址", 184, 132, fontSm(), kTextSecondary);
    labelAt(card, "3  选择 Wi-Fi 或手机校时", 184, 166, fontMd(), kAccentMintFg);
    auto *hint = createBadge(card, "退出此页将自动关闭热点", kAccentCyanSoft, kAccentCyanFg);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto *scan = createPillButton(parent, "没有手机？扫描 Wi-Fi 并用屏幕键盘输入",
        kBgCardElevated, kTextPrimary, kContentWidth, 48);
    lv_obj_set_pos(scan, kPadScreen, 319);
    lv_obj_set_user_data(scan, reinterpret_cast<void *>(static_cast<uintptr_t>(TimeSubView::AP_LIST)));
    lv_obj_add_event_cb(scan, onTimeNavigate, LV_EVENT_CLICKED, this);
}

void SettingsApp::buildScanPage(lv_obj_t *parent)
{
    auto *card = createCard(parent, kContentWidth, 302);
    lv_obj_set_pos(card, kPadScreen, 8);
    lv_obj_set_style_pad_all(card, 12, 0);
    labelAt(card, "仅支持 2.4 GHz Wi-Fi · 中文热点名称建议用手机查看", 0, 0, fontSm(), kTextSecondary);
    _scan_list = lv_obj_create(card);
    lv_obj_remove_style_all(_scan_list);
    lv_obj_set_pos(_scan_list, 0, 29);
    lv_obj_set_size(_scan_list, kContentWidth - 26, 246);
    lv_obj_add_flag(_scan_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_scan_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_scan_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(_scan_list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(_scan_list, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(_scan_list, color(kTextMuted), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_scan_list, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_scan_list, 6, 0);

    auto *qr = createPillButton(parent, "密码太复杂？切回手机扫码配网",
        kAccentCyanSoft, kAccentCyanFg, kContentWidth, 48);
    lv_obj_set_pos(qr, kPadScreen, 319);
    lv_obj_set_user_data(qr, reinterpret_cast<void *>(static_cast<uintptr_t>(TimeSubView::QR_PORTAL)));
    lv_obj_add_event_cb(qr, onTimeNavigate, LV_EVENT_CLICKED, this);
}

void SettingsApp::fillScanList(const WifiTimeStatus &status)
{
    lv_obj_clean(_scan_list);
    if (status.access_point_count == 0) {
        labelAt(_scan_list, status.message[0] ? status.message : "未找到附近 Wi-Fi", 0, 0,
                fontSm(), kTextSecondary);
        return;
    }

    for (unsigned i = 0; i < status.access_point_count; ++i) {
        const auto &network = status.access_points[i];
        auto *row = createPillButton(_scan_list, network.ssid, kBgCanvas, kTextPrimary, LV_PCT(100), 54, fontMd());
        lv_obj_set_style_radius(row, 14, 0);
        auto *name = lv_obj_get_child(row, 0);
        lv_obj_set_width(name, 278);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 38, 0);
        auto *auth = labelAt(row, network.secured ? "加密" : "开放", 0, 0, fontSm(), kTextSecondary);
        lv_obj_align(auth, LV_ALIGN_RIGHT_MID, 0, 0);
        const int strength = network.rssi >= -55 ? 4 : network.rssi >= -67 ? 3 : network.rssi >= -78 ? 2 : 1;
        for (int bar = 0; bar < 4; ++bar) {
            auto *segment = lv_obj_create(row);
            lv_obj_remove_style_all(segment);
            lv_obj_clear_flag(segment, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
            lv_obj_set_size(segment, 5, 5 + 4 * bar);
            lv_obj_set_pos(segment, 2 + 7 * bar, 25 - 4 * bar);
            lv_obj_set_style_radius(segment, 2, 0);
            lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(segment, color(bar < strength ? kAccentMintFg : kBgTrack), 0);
        }
        lv_obj_set_user_data(row, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(row, onSelectNetwork, LV_EVENT_CLICKED, this);
    }
}

void SettingsApp::buildPasswordPage(lv_obj_t *parent)
{
    _password = lv_textarea_create(parent);
    lv_textarea_set_one_line(_password, true);
    lv_obj_set_size(_password, 368, 52);
    lv_obj_set_pos(_password, 12, 4);
    lv_obj_set_style_radius(_password, 16, 0);
    lv_obj_set_style_bg_color(_password, color(kBgCard), 0);
    lv_obj_set_style_bg_opa(_password, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_password, 2, 0);
    lv_obj_set_style_border_color(_password, color(kAccentCyan), 0);
    lv_obj_set_style_pad_hor(_password, 14, 0);
    lv_obj_set_style_pad_ver(_password, 10, 0);
    lv_obj_set_style_text_font(_password, fontMd(), 0);
    lv_obj_set_style_text_color(_password, color(kTextPrimary), 0);
    lv_obj_set_style_text_color(_password, color(kTextMuted), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(_password, color(kAccentCyanFg), LV_PART_CURSOR);
    lv_obj_set_style_border_width(_password, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(_password, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_add_state(_password, LV_STATE_FOCUSED);
    lv_textarea_set_placeholder_text(_password, "请输入 Wi-Fi 密码");
    lv_textarea_set_password_bullet(_password, "•");
    lv_textarea_set_password_show_time(_password, 800);
    lv_textarea_set_password_mode(_password, false);
    lv_textarea_set_max_length(_password, 64);

    auto *clear = createPillButton(parent, "清空", kBgCardElevated, kTextPrimary, 80, 52);
    lv_obj_set_pos(clear, 388, 4);
    lv_obj_set_style_border_width(clear, 1, 0);
    lv_obj_set_style_border_color(clear, color(kBorderSubtle), 0);
    lv_obj_add_event_cb(clear, [](lv_event_t *event) {
        auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(event));
        SoundService::instance().play(SoundEffect::CLICK);
        lv_textarea_set_text(self->_password, "");
        self->refreshPasswordStatus();
    }, LV_EVENT_CLICKED, this);

    _password_msg = labelAt(parent, "", 16, 60, fontSm(), kTextSecondary);
    refreshPasswordStatus();

    auto *keys = lv_buttonmatrix_create(parent);
    lv_obj_set_size(keys, 468, 284);
    lv_obj_set_pos(keys, 6, 84);
    lv_obj_set_style_text_font(keys, fontMd(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keys, color(kTextPrimary), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keys, color(kBgCanvas), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(keys, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(keys, color(kBgCard), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keys, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keys, color(kAccentAmberSoft),
        static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(keys, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(keys, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keys, color(kBorderSubtle), LV_PART_ITEMS);
    lv_obj_set_style_border_color(keys, color(kAccentAmber),
        static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(keys, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(keys, 4, 0);
    lv_obj_set_style_pad_row(keys, 5, 0);
    lv_obj_set_style_pad_column(keys, 4, 0);
    lv_obj_set_style_radius(keys, 16, LV_PART_MAIN);
    lv_obj_set_style_radius(keys, 12, LV_PART_ITEMS);
    lv_obj_add_event_cb(keys, [](lv_event_t *event) {
        auto *task = lv_event_get_draw_task(event);
        auto *target = lv_event_get_target_obj(event);
        auto *base = static_cast<lv_draw_dsc_base_t *>(lv_draw_task_get_draw_dsc(task));
        if (!base || base->part != LV_PART_ITEMS) {
            return;
        }
        const bool is_connect = lv_buttonmatrix_has_button_ctrl(target, base->id1, LV_BUTTONMATRIX_CTRL_CUSTOM_1);
        const bool is_func = lv_buttonmatrix_has_button_ctrl(target, base->id1, LV_BUTTONMATRIX_CTRL_CUSTOM_2);
        const bool is_pressed = lv_obj_has_state(target, LV_STATE_PRESSED) &&
            lv_buttonmatrix_get_selected_button(target) == base->id1;
        if (auto *rect = lv_draw_task_get_fill_dsc(task)) {
            if (is_connect) {
                rect->color = color(is_pressed ? kAccentMint : kAccentMintFg);
            } else if (is_func && !is_pressed) {
                rect->color = color(kBgCardElevated);
            }
        } else if (auto *border = lv_draw_task_get_border_dsc(task)) {
            if (is_connect) {
                border->color = color(kAccentMintFg);
            }
        } else if (auto *label = lv_draw_task_get_label_dsc(task)) {
            if (is_connect) {
                label->color = color(kTextOnAccent);
            }
        }
    }, LV_EVENT_DRAW_TASK_ADDED, nullptr);
    lv_obj_add_flag(keys, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    useKeymap(keys, kLowerKeys);
    lv_obj_add_event_cb(keys, onPasswordKey, LV_EVENT_VALUE_CHANGED, this);
}

void SettingsApp::refreshPasswordStatus()
{
    if (!_password || !_password_msg) {
        return;
    }
    const size_t len = std::strlen(lv_textarea_get_text(_password));
    if (len == 0) {
        lv_label_set_text(_password_msg, "已输入 0 位 · 密码需 8 位以上");
        lv_obj_set_style_text_color(_password_msg, color(kTextSecondary), 0);
    } else if (len < 8) {
        lv_label_set_text_fmt(_password_msg, "已输入 %u 位 · 还需 %u 位",
                              static_cast<unsigned>(len), static_cast<unsigned>(8 - len));
        lv_obj_set_style_text_color(_password_msg, color(kAccentAmberFg), 0);
    } else {
        lv_label_set_text_fmt(_password_msg, "已输入 %u 位 · 可以连接", static_cast<unsigned>(len));
        lv_obj_set_style_text_color(_password_msg, color(kAccentMintFg), 0);
    }
}

void SettingsApp::onWifiPrimary(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto &wifi = WifiTimeService::instance();
    if (wifi.status().saved_ssid[0]) {
        wifi.startSyncSavedWifi();
        self->refreshTimeView();
    } else {
        self->showTimeView(TimeSubView::QR_PORTAL);
    }
}

void SettingsApp::onWifiSecondary(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    self->showTimeView(WifiTimeService::instance().status().saved_ssid[0]
        ? TimeSubView::QR_PORTAL : TimeSubView::AP_LIST);
}

void SettingsApp::onWifiAutoSync(lv_event_t *e)
{
    auto &storage = StorageService::instance();
    auto settings = storage.getSettings();
    auto *toggle = lv_event_get_target_obj(e);
    settings.wifi_auto_sync = lv_obj_has_state(toggle, LV_STATE_CHECKED);
    if (!storage.updateSettings(settings)) {
        if (storage.getSettings().wifi_auto_sync) {
            lv_obj_add_state(toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(toggle, LV_STATE_CHECKED);
        }
        auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
        lv_label_set_text(self->_wifi_summary, "保存失败，请重试");
    }
}

void SettingsApp::onTimeNavigate(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *target = lv_event_get_target_obj(e);
    const auto view = static_cast<TimeSubView>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    SoundService::instance().play(SoundEffect::CLICK);
    self->showTimeView(view);
}

void SettingsApp::onScanRefresh(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto &wifi = WifiTimeService::instance();
    if (wifi.status().state != WifiSyncState::SCANNING) {
        wifi.startApScan();
        self->refreshTimeView();
    }
}

void SettingsApp::onSelectNetwork(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto &wifi = WifiTimeService::instance();
    const auto status = wifi.status();
    const auto index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
    if (index >= status.access_point_count) {
        return;
    }
    self->_selected_network = status.access_points[index];
    if (self->_selected_network.secured) {
        self->showTimeView(TimeSubView::KEYBOARD);
    } else {
        wifi.connectAndSync(self->_selected_network.ssid, "");
        self->showTimeView(TimeSubView::MAIN);
    }
}

void SettingsApp::onPasswordKey(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    auto *keys = lv_event_get_target_obj(e);
    const auto *text = lv_buttonmatrix_get_button_text(keys, lv_buttonmatrix_get_selected_button(keys));
    if (!text) {
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    const std::string_view key(text);
    if (key == "连接") {
        const auto *password = lv_textarea_get_text(self->_password);
        if (!password[0]) {
            lv_label_set_text(self->_password_msg, "请输入 Wi-Fi 密码");
            lv_obj_set_style_text_color(self->_password_msg, color(kAccentCoralFg), 0);
            return;
        }
        if (WifiTimeService::instance().connectAndSync(self->_selected_network.ssid,
                                                       password)) {
            self->showTimeView(TimeSubView::MAIN);
        } else {
            const auto state = WifiTimeService::instance().status().state;
            if (state == WifiSyncState::FAILED || syncInProgress(state)) {
                self->showTimeView(TimeSubView::MAIN);
            } else {
                lv_label_set_text(self->_password_msg, "密码需 8 至 63 字符或 64 位十六进制密钥");
                lv_obj_set_style_text_color(self->_password_msg, color(kAccentCoralFg), 0);
            }
        }
    } else if (key == "⇧") {
        useKeymap(keys, kUpperKeys);
    } else if (key == "⇩" || key == "abc") {
        useKeymap(keys, kLowerKeys);
    } else if (key == "123") {
        useKeymap(keys, kNumberKeys);
    } else if (key == "符号") {
        useKeymap(keys, kSymbolKeys);
    } else if (key == "删除") {
        lv_textarea_delete_char(self->_password);
        self->refreshPasswordStatus();
    } else {
        lv_textarea_add_text(self->_password, key == "空格" ? " " : text);
        self->refreshPasswordStatus();
    }
}

void SettingsApp::onTogglePassword(lv_event_t *e)
{
    auto *self = static_cast<SettingsApp *>(lv_event_get_user_data(e));
    SoundService::instance().play(SoundEffect::CLICK);
    const bool was_hidden = lv_textarea_get_password_mode(self->_password);
    lv_textarea_set_password_mode(self->_password, !was_hidden);
    setButton(lv_event_get_current_target_obj(e), was_hidden ? "隐藏" : "显示");
}

}
