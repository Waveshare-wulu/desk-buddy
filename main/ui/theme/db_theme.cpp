#include "db_theme.hpp"

#include <cstring>

#include "src/core/lv_obj_event_private.h"

namespace desk_buddy::theme {

namespace {

constexpr int kMinTouchHeight = 48;
constexpr int kTopButtonHeight = 44;
constexpr int kBackButtonWidth = 108;

void constrainTouchWidth(lv_event_t *event)
{
    auto *hit = lv_event_get_hit_test_info(event);
    lv_area_t bounds{};
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &bounds);
    hit->res = hit->res && hit->point->x >= bounds.x1 && hit->point->x <= bounds.x2;
}

}

lv_obj_t *createTopBarButton(lv_obj_t *bar, const char *text, uint32_t fg, int width,
                            const lv_font_t *font)
{
    auto *button = createPillButton(bar, text, kBgCard, fg, width, kTopButtonHeight, font);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, color(kBorderSubtle), 0);
    return button;
}

void styleRootScreen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, color(kBgCanvas), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_text_font(screen, fontMd(), 0);
    lv_obj_set_style_text_color(screen, color(kTextPrimary), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *createCard(lv_obj_t *parent, int w, int h, int radius)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, color(kBgCard), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, color(kBorderSubtle), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);
    lv_obj_set_style_shadow_color(card, color(kShadowWarm), 0);
    lv_obj_set_style_shadow_opa(card, 22, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void expandTouchHeight(lv_obj_t *control, int visible_height)
{
    if (visible_height >= kMinTouchHeight) {
        return;
    }

    lv_obj_set_ext_click_area(control, (kMinTouchHeight - visible_height + 1) / 2);
    lv_obj_add_flag(control, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(control, constrainTouchWidth, LV_EVENT_HIT_TEST, nullptr);
}

void MalletKnockController::attach(ApplyAngleFn apply, void *context)
{
    detach();
    _apply = apply;
    _context = context;
    _motion.reset();
    if (!_apply) {
        return;
    }
    _timer = lv_timer_create(onTimer, MalletMotion::kTickMs, this);
    if (_timer) {
        lv_timer_pause(_timer);
    }
}

void MalletKnockController::detach()
{
    if (_timer) {
        lv_timer_delete(_timer);
        _timer = nullptr;
    }
    _motion.reset();
    _apply = nullptr;
    _context = nullptr;
}

void MalletKnockController::stop()
{
    if (_timer) {
        lv_timer_pause(_timer);
    }
    const bool was_active = _motion.isActive() || _motion.angle() != MalletMotion::kRestAngle;
    _motion.reset();
    if (was_active) {
        applyCurrent();
    }
}

void MalletKnockController::trigger()
{
    if (!_apply) {
        return;
    }
    const bool was_active = _motion.isActive();
    if (_motion.trigger()) {
        applyCurrent();
    }
    if (!was_active) {
        _last_tick_ms = lv_tick_get();
        if (_timer) {
            lv_timer_reset(_timer);
            lv_timer_resume(_timer);
        }
    }
}

void MalletKnockController::applyCurrent()
{
    if (_apply) {
        _apply(_context, _motion.angle(), _motion.frameIndex());
    }
}

void MalletKnockController::onTimer(lv_timer_t *timer)
{
    auto *self = static_cast<MalletKnockController *>(lv_timer_get_user_data(timer));
    if (!self) {
        return;
    }
    const uint32_t dt = lv_tick_elaps(self->_last_tick_ms);
    self->_last_tick_ms = lv_tick_get();
    if (self->_motion.step(dt)) {
        self->applyCurrent();
    }
    if (!self->_motion.isActive() && self->_timer) {
        lv_timer_pause(self->_timer);
    }
}

lv_obj_t *createPillButton(
    lv_obj_t *parent,
    const char *text,
    uint32_t bg_hex,
    uint32_t text_hex,
    int w,
    int h,
    const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    expandTouchHeight(btn, h);
    lv_obj_set_style_bg_color(btn, color(bg_hex), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(color(bg_hex), 22), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, kRadiusBtn, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 14, 0);
    lv_obj_set_style_pad_ver(btn, 6, 0);
    lv_obj_set_style_transform_scale(btn, LV_SCALE_NONE, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, font ? font : fontMd(), 0);
    lv_obj_set_style_text_color(lbl, color(text_hex), 0);
    lv_obj_center(lbl);
    return btn;
}

TopBarRefs createAppTopBar(
    lv_obj_t *parent,
    const char *title,
    uint32_t accent_hex,
    lv_event_cb_t back_cb,
    void *user_data,
    const char *action_text,
    lv_event_cb_t action_cb)
{
    TopBarRefs refs{};
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), kTopBarHeight);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, kPadScreen, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    refs.bar = bar;

    lv_obj_t *back = createTopBarButton(bar, "‹ 返回", kTextPrimary, kBackButtonWidth, fontMd());
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    if (back_cb) {
        lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, user_data);
    }
    refs.back_btn = back;

    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_text(lbl, title ? title : "");
    lv_obj_set_style_text_font(lbl, fontMd(), 0);
    lv_obj_set_style_text_color(lbl, color(kTextPrimary), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl, kContentWidth - 2 * (kBackButtonWidth + 8));
    lv_obj_set_height(lbl, lv_font_get_line_height(fontMd()));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    refs.title_label = lbl;

    if (action_text) {
        const bool long_label = std::strlen(action_text) > 9;
        lv_obj_t *act = createTopBarButton(bar, action_text, accent_hex,
                                         long_label ? 108 : 96, long_label ? fontSm() : fontMd());
        lv_obj_align(act, LV_ALIGN_RIGHT_MID, 0, 0);
        if (action_cb) {
            lv_obj_add_event_cb(act, action_cb, LV_EVENT_CLICKED, user_data);
        }
        refs.action_btn = act;
        refs.action_label = lv_obj_get_child(act, 0);
    }

    return refs;
}

lv_obj_t *createBadge(lv_obj_t *parent, const char *text, uint32_t bg_hex, uint32_t text_hex)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, color(bg_hex), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 4, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(pill);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, fontSm(), 0);
    lv_obj_set_style_text_color(lbl, color(text_hex), 0);
    return pill;
}

}
