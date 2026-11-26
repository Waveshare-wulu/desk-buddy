#include "ui/home/db_power_overlay.hpp"

#include <cstdio>
#include <utility>

#include "services/sound_service.hpp"
#include "ui/assets/db_ui_icons.h"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
using namespace theme;

constexpr std::array kTopButtonOrder{ButtonId::BOOT, ButtonId::PWR, ButtonId::KEY};
constexpr std::array kTopButtonCenterX{117, 240, 363};
constexpr const char *kPositions[] = {"顶部左键", "顶部中键", "顶部右键"};
constexpr const char *kCompactPositions[] = {"左键", "中键", "右键"};
constexpr int kGuideHighlightMs = 1400;
constexpr int kConfirmationMs = 3000;
constexpr int kToastMs = 1500;
constexpr int kOnboardingConfirmHeight = 68;
constexpr int kOnboardingConfirmY = 372;
constexpr int kOnboardingConfirmExtClickPad = 28;
constexpr lv_opa_t kMenuScrimOpacity = 112;

struct ButtonStyle {
    const lv_image_dsc_t *icon;
    uint32_t accent;
    uint32_t soft;
    uint32_t fg;
    const char *title;
    const char *short_clause;
    const char *long_clause;
    const char *short_hint;
    const char *long_hint;
};

constexpr ButtonStyle kButtonStyles[] = {
    {&db_icon_pwr_off, kAccentCoral, kAccentCoralSoft, kAccentCoralFg, "电源与熄屏",
     " 亮屏或熄屏 · ", " 电源菜单", "亮 / 熄屏", "长按 电源菜单"},
    {&db_icon_key_back, kAccentCyan, kAccentCyanSoft, kAccentCyanFg, "返回与静音",
     " 返回与切页 · ", " 切换静音", "返回桌面", "长按 一键静音"},
    {&db_icon_key_spark, kAccentAmber, kAccentAmberSoft, kAccentAmberFg, "摸鱼快捷键",
     " 敲木鱼 / 抽签 · ", " 打开设置", "敲木鱼 / 抽签", "长按 系统设置"},
};

const ButtonStyle &buttonStyle(unsigned position)
{
    return kButtonStyles[static_cast<unsigned>(kTopButtonOrder[position])];
}

const char *powerPosition()
{
    for (unsigned i = 0; i < kTopButtonOrder.size(); ++i) {
        if (kTopButtonOrder[i] == ButtonId::PWR) {
            return kPositions[i];
        }
    }
    return "电源键";
}

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t fg = kTextPrimary)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text_static(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(fg), 0);
    return label;
}

lv_obj_t *makeIcon(lv_obj_t *parent, const lv_image_dsc_t *source)
{
    auto *icon = lv_image_create(parent);
    lv_image_set_src(icon, source);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    return icon;
}

lv_obj_t *makeFullLayer(uint32_t bg, lv_opa_t opacity)
{
    auto *layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(layer, color(bg), 0);
    lv_obj_set_style_bg_opa(layer, opacity, 0);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    return layer;
}

void setPillSurface(lv_obj_t *pill, uint32_t bg, uint32_t border, int width)
{
    const auto fill = color(bg);
    lv_obj_set_style_bg_color(pill, fill, 0);
    lv_obj_set_style_bg_color(pill, lv_color_darken(fill, 22), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(pill, color(border), 0);
    lv_obj_set_style_border_width(pill, width, 0);
}

void addGuideSpan(lv_obj_t *group, const char *text, uint32_t fg)
{
    if (auto *span = lv_spangroup_add_span(group)) {
        lv_span_set_text_static(span, text);
        lv_style_set_text_font(lv_span_get_style(span), fontSm());
        lv_style_set_text_color(lv_span_get_style(span), color(fg));
    }
}

struct PowerAction {
    lv_obj_t *button;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *subtitle;
};

PowerAction makeAction(lv_obj_t *parent, const char *title, const char *subtitle,
                       uint32_t bg, uint32_t border, uint32_t fg, const lv_image_dsc_t *icon)
{
    auto *button = createPillButton(parent, title, bg, fg, 182, 152, fontMd());
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    setPillSurface(button, bg, border, 1);
    auto *image = makeIcon(button, icon);
    lv_obj_align(image, LV_ALIGN_CENTER, 0, -26);
    auto *label = lv_obj_get_child(button, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 26);
    auto *detail = makeLabel(button, subtitle, fontSm(), kTextSecondary);
    lv_obj_align(detail, LV_ALIGN_CENTER, 0, 52);
    return {button, image, label, detail};
}

}

void ButtonGuide::build(lv_obj_t *parent, bool onboarding)
{
    for (unsigned i = 0; i < _cards.size(); ++i) {
        const auto &data = buttonStyle(i);
        auto &card = _cards[i];
        card.position = i;
        card.onboarding = onboarding;
        card.body = createCard(parent, onboarding ? 140 : kContentWidth, onboarding ? 224 : 112);
        lv_obj_clear_flag(card.body, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_pad_all(card.body, 0, 0);
        card.badge = createBadge(card.body, onboarding ? kCompactPositions[i] : kPositions[i],
                                 onboarding ? kBgCanvas : data.soft, data.fg);
        lv_obj_clear_flag(card.badge, LV_OBJ_FLAG_CLICKABLE);
        auto *icon = makeIcon(card.body, data.icon);
        if (onboarding) {
            lv_obj_set_pos(card.body, kPadScreen + 152 * i, 124);
            lv_obj_align(card.badge, LV_ALIGN_TOP_MID, 0, 16);
            lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 72);
            lv_obj_align(makeLabel(card.body, data.short_hint, fontSm()), LV_ALIGN_TOP_MID, 0, 150);
            lv_obj_align(makeLabel(card.body, data.long_hint, fontSm(), kTextSecondary),
                         LV_ALIGN_TOP_MID, 0, 178);

            card.marker = lv_obj_create(parent);
            lv_obj_remove_style_all(card.marker);
            lv_obj_set_size(card.marker, 36, 5);
            lv_obj_set_pos(card.marker, kTopButtonCenterX[i] - 18, 12);
            lv_obj_set_style_radius(card.marker, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(card.marker, color(data.accent), 0);
            lv_obj_set_style_bg_opa(card.marker, LV_OPA_70, 0);
            lv_obj_clear_flag(card.marker, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        } else {
            lv_obj_set_pos(card.body, kPadScreen, 4 + 123 * i);
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
            lv_obj_align(makeLabel(card.body, data.title, fontMd()), LV_ALIGN_TOP_LEFT, 84, 23);
            lv_obj_align(card.badge, LV_ALIGN_TOP_RIGHT, -16, 19);

            auto *description = lv_spangroup_create(card.body);
            lv_obj_set_size(description, 342, LV_SIZE_CONTENT);
            lv_obj_align(description, LV_ALIGN_TOP_LEFT, 84, 64);
            addGuideSpan(description, "短按", kTextPrimary);
            addGuideSpan(description, data.short_clause, kTextSecondary);
            addGuideSpan(description, "长按", kTextPrimary);
            addGuideSpan(description, data.long_clause, kTextSecondary);
        }
    }
}

void ButtonGuide::style(Card &card, ButtonPress press)
{
    const auto &data = buttonStyle(card.position);
    const bool pressed = press != ButtonPress::NONE;
    lv_obj_set_style_bg_color(card.body, color(pressed ? data.soft : kBgCard), 0);
    lv_obj_set_style_border_width(card.body, pressed ? 2 : 1, 0);
    lv_obj_set_style_border_color(card.body, color(pressed ? data.accent : kBorderSubtle), 0);

    auto *label = lv_obj_get_child(card.badge, 0);
    const char *text = card.onboarding ? kCompactPositions[card.position] : kPositions[card.position];
    if (pressed) {
        text = card.onboarding ? (press == ButtonPress::SHORT ? "短按" : "长按")
                               : (press == ButtonPress::SHORT ? "已按 · 短按" : "已按 · 长按");
    }
    lv_label_set_text_static(label, text);
    lv_obj_set_style_bg_color(card.badge, color(pressed ? data.accent : card.onboarding ? kBgCanvas : data.soft), 0);
    lv_obj_set_style_text_color(label, color(pressed ? kTextOnAccent : data.fg), 0);
    if (card.marker) {
        const int width = pressed ? 56 : 36;
        lv_obj_set_width(card.marker, width);
        lv_obj_set_x(card.marker, kTopButtonCenterX[card.position] - width / 2);
        lv_obj_set_style_bg_opa(card.marker, pressed ? LV_OPA_COVER : LV_OPA_70, 0);
    }
}

void ButtonGuide::highlight(ButtonEvent event)
{
    for (auto &card : _cards) {
        if (card.body && kTopButtonOrder[card.position] == event.id) {
            style(card, event.press);
            if (card.timer) {
                lv_timer_reset(card.timer);
                return;
            }
            card.timer = lv_timer_create([](lv_timer_t *timer) {
                auto &item = *static_cast<Card *>(lv_timer_get_user_data(timer));
                item.timer = nullptr;
                style(item, ButtonPress::NONE);
            }, kGuideHighlightMs, &card);
            if (card.timer) {
                lv_timer_set_repeat_count(card.timer, 1);
            } else {
                style(card, ButtonPress::NONE);
            }
            return;
        }
    }
}

void ButtonGuide::clear()
{
    for (auto &card : _cards) {
        if (card.timer) {
            lv_timer_delete(card.timer);
        }
        card = {};
    }
}

void PowerOverlay::bind(std::function<void(bool)> request_power, std::function<void()> confirm_guide)
{
    _request_power = std::move(request_power);
    _confirm_guide = std::move(confirm_guide);
}

void PowerOverlay::showMenu(const BatteryStatus &battery)
{
    if (_view != View::NONE) {
        return;
    }
    _view = View::MENU;
    _root = makeFullLayer(kTextPrimary, kMenuScrimOpacity);
    lv_obj_add_event_cb(_root, [](lv_event_t *event) {
        static_cast<PowerOverlay *>(lv_event_get_user_data(event))->close();
    }, LV_EVENT_CLICKED, this);

    auto *menu = createCard(_root, 416, 348, 26);
    lv_obj_center(menu);
    lv_obj_set_style_pad_all(menu, 0, 0);
    lv_obj_add_flag(menu, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_align(makeLabel(menu, "电源选项", fontMd()), LV_ALIGN_TOP_LEFT, 19, 23);
    char status[40];
    if (battery.vbus_in && (!battery.charging || !battery.battery_connected)) {
        std::snprintf(status, sizeof(status), "USB 供电");
    } else if (battery.vbus_in) {
        std::snprintf(status, sizeof(status), "充电中 %d%%", battery.percent);
    } else if (battery.battery_connected) {
        std::snprintf(status, sizeof(status), "电池 %d%%", battery.percent);
    } else {
        std::snprintf(status, sizeof(status), "电池 --%%");
    }
    auto *badge = createBadge(menu, status,
                              battery.vbus_in ? kAccentCyanSoft : kAccentMintSoft,
                              battery.vbus_in ? kAccentCyanFg : kAccentMintFg);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -19, 19);

    const auto off = makeAction(menu, "关机", "保存并断电", kAccentCoralSoft, kBorderCoral,
                                kAccentCoralFg, &db_icon_pwr_off);
    _shutdown_button = off.button;
    _shutdown_icon = off.icon;
    _shutdown_title = off.title;
    _shutdown_subtitle = off.subtitle;
    lv_obj_align(off.button, LV_ALIGN_TOP_LEFT, 19, 65);
    lv_obj_add_event_cb(off.button, [](lv_event_t *event) {
        static_cast<PowerOverlay *>(lv_event_get_user_data(event))->onShutdownClicked();
    }, LV_EVENT_CLICKED, this);

    const auto reboot = makeAction(menu, "重启", "保存并重启", kAccentAmberSoft, kBorderAmber,
                                   kAccentAmberFg, &db_icon_pwr_reboot);
    lv_obj_align(reboot.button, LV_ALIGN_TOP_RIGHT, -19, 65);
    lv_obj_add_event_cb(reboot.button, [](lv_event_t *event) {
        auto &self = *static_cast<PowerOverlay *>(lv_event_get_user_data(event));
        self._request_power(true);
    }, LV_EVENT_CLICKED, this);

    auto *cancel = createPillButton(menu, "取消", kBgCanvas, kTextSecondary, 376, 56, fontMd());
    lv_obj_set_style_radius(cancel, 18, 0);
    setPillSurface(cancel, kBgCanvas, kBorderSubtle, 1);
    lv_obj_align(cancel, LV_ALIGN_TOP_MID, 0, 233);
    lv_obj_add_event_cb(cancel, [](lv_event_t *event) {
        static_cast<PowerOverlay *>(lv_event_get_user_data(event))->close();
    }, LV_EVENT_CLICKED, this);

    auto *hint = makeLabel(menu, "", fontSm(), kTextMuted);
    lv_label_set_text_fmt(hint, "短按%s立即熄屏", powerPosition());
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 305);
}

void PowerOverlay::setShutdownConfirmation(bool armed)
{
    setPillSurface(_shutdown_button, armed ? kAccentCoral : kAccentCoralSoft,
                   armed ? kAccentCoral : kBorderCoral, armed ? 2 : 1);
    lv_obj_set_style_opa(_shutdown_icon, armed ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_shutdown_title, color(armed ? kTextOnAccent : kAccentCoralFg), 0);
    lv_obj_set_style_text_color(_shutdown_subtitle, color(armed ? kTextOnAccent : kTextSecondary), 0);
    lv_label_set_text_static(_shutdown_title, armed ? "确认关机" : "关机");
    lv_label_set_text_static(_shutdown_subtitle, armed ? "再次点击断电" : "保存并断电");
}

void PowerOverlay::onShutdownClicked()
{
    if (_confirmation_timer) {
        _request_power(false);
        return;
    }
    _confirmation_timer = lv_timer_create([](lv_timer_t *timer) {
        auto &self = *static_cast<PowerOverlay *>(lv_timer_get_user_data(timer));
        self._confirmation_timer = nullptr;
        self.setShutdownConfirmation(false);
    }, kConfirmationMs, this);
    if (_confirmation_timer) {
        lv_timer_set_repeat_count(_confirmation_timer, 1);
        setShutdownConfirmation(true);
    }
}

void PowerOverlay::showOnboarding()
{
    _view = View::ONBOARDING;
    _root = makeFullLayer(kBgCanvas, LV_OPA_COVER);
    lv_obj_align(makeLabel(_root, "顶部实体按键", fontLg()), LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_align(makeLabel(_root, "按一下顶部按键试试", fontSm(), kTextSecondary), LV_ALIGN_TOP_MID, 0, 83);
    _guide.build(_root, true);

    auto *confirm = createPillButton(_root, "知道了，开始使用", kAccentMint, kTextOnAccent,
                                     kContentWidth, kOnboardingConfirmHeight, fontMd());
    lv_obj_set_style_radius(confirm, 22, 0);
    lv_obj_set_ext_click_area(confirm, kOnboardingConfirmExtClickPad);
    lv_obj_align(confirm, LV_ALIGN_TOP_MID, 0, kOnboardingConfirmY);
    lv_obj_add_event_cb(confirm, [](lv_event_t *event) {
        SoundService::instance().play(SoundEffect::CLICK);
        auto &self = *static_cast<PowerOverlay *>(lv_event_get_user_data(event));
        self._confirm_guide();
    }, LV_EVENT_CLICKED, this);
}

bool PowerOverlay::handleOnboardingButton(ButtonEvent event)
{
    if (_view != View::ONBOARDING) {
        return false;
    }
    _guide.highlight(event);
    return true;
}

void PowerOverlay::showShutdown(bool reboot)
{
    close();
    if (_toast) {
        lv_obj_add_flag(_toast, LV_OBJ_FLAG_HIDDEN);
    }
    _view = View::SHUTDOWN;
    _root = makeFullLayer(kBgCanvas, LV_OPA_COVER);
    lv_obj_align(makeIcon(_root, reboot ? &db_icon_pwr_reboot : &db_icon_pwr_off), LV_ALIGN_CENTER, 0, -51);
    lv_obj_align(makeLabel(_root, reboot ? "正在重启" : "正在关机", fontLg()), LV_ALIGN_CENTER, 0, 8);
    lv_obj_align(makeLabel(_root, "正在保存功德与设置…", fontSm(), kTextSecondary), LV_ALIGN_CENTER, 0, 51);
}

void PowerOverlay::showMuteToast(bool sound_enabled)
{
    if (!_toast) {
        _toast = createBadge(lv_layer_top(), "", kBgCanvas, kTextPrimary);
        lv_obj_clear_flag(_toast, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_pad_hor(_toast, 20, 0);
        lv_obj_set_style_pad_ver(_toast, 7, 0);
        lv_obj_set_style_border_width(_toast, 2, 0);
        auto *label = lv_obj_get_child(_toast, 0);
        lv_obj_set_style_text_font(label, fontMd(), 0);
        _toast_timer = lv_timer_create([](lv_timer_t *timer) {
            auto &self = *static_cast<PowerOverlay *>(lv_timer_get_user_data(timer));
            lv_obj_add_flag(self._toast, LV_OBJ_FLAG_HIDDEN);
            lv_timer_pause(timer);
        }, kToastMs, this);
    }
    auto *label = lv_obj_get_child(_toast, 0);
    lv_label_set_text_static(label, sound_enabled ? "音效已开启" : "已静音");
    lv_obj_set_style_text_color(label, color(sound_enabled ? kAccentMintFg : kAccentCoralFg), 0);
    lv_obj_set_style_bg_color(_toast, color(sound_enabled ? kAccentMintSoft : kAccentCoralSoft), 0);
    lv_obj_set_style_border_color(_toast, color(sound_enabled ? kAccentMint : kAccentCoral), 0);
    lv_obj_align(_toast, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_move_foreground(_toast);
    if (_toast_timer) {
        lv_obj_remove_flag(_toast, LV_OBJ_FLAG_HIDDEN);
        lv_timer_reset(_toast_timer);
        lv_timer_resume(_toast_timer);
    } else {
        lv_obj_add_flag(_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

void PowerOverlay::close()
{
    if (_confirmation_timer) {
        lv_timer_delete(_confirmation_timer);
        _confirmation_timer = nullptr;
    }
    _guide.clear();
    if (_root) {
        lv_obj_delete(_root);
        _root = nullptr;
    }
    _view = View::NONE;
    _shutdown_button = _shutdown_icon = _shutdown_title = _shutdown_subtitle = nullptr;
}

}
