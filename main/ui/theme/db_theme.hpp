#pragma once

#include <cstddef>
#include <cstdint>
#include "db_mallet_motion.hpp"
#include "lvgl.h"
#include "ui/assets/db_fonts.h"

namespace desk_buddy::theme {

constexpr uint32_t kBgCanvas       = 0xFAF6EE;
constexpr uint32_t kBgCard         = 0xFFFFFF;
constexpr uint32_t kBgCardElevated = 0xF3ECE1;
constexpr uint32_t kBgCardPressed  = 0xE8DEC9;
constexpr uint32_t kBgTrack        = 0xEFE6D8;
constexpr uint32_t kBorderSubtle   = 0xECE2D2;
constexpr uint32_t kBorderCoral    = 0xF6D5DA;
constexpr uint32_t kBorderAmber    = 0xF2DAB2;
constexpr uint32_t kShadowWarm     = 0x9A7B56;
constexpr uint32_t kTextPrimary    = 0x3A302A;
constexpr uint32_t kTextSecondary  = 0x86786D;
constexpr uint32_t kTextMuted      = 0xA5988B;
constexpr uint32_t kTextOnAccent   = 0xFFFFFF;

constexpr uint32_t kAccentViolet     = 0x8D79F2;
constexpr uint32_t kAccentVioletSoft = 0xF2EEFF;
constexpr uint32_t kAccentVioletFg   = 0x634BD6;

constexpr uint32_t kAccentAmber      = 0xF29838;
constexpr uint32_t kAccentAmberSoft  = 0xFFF2E0;
constexpr uint32_t kAccentAmberFg    = 0xB9610E;

constexpr uint32_t kAccentCoral      = 0xF26D7E;
constexpr uint32_t kAccentCoralSoft  = 0xFFEDF0;
constexpr uint32_t kAccentCoralFg    = 0xC73E52;

constexpr uint32_t kAccentOrange     = 0xF4823C;
constexpr uint32_t kAccentOrangeSoft = 0xFFF0E4;
constexpr uint32_t kAccentOrangeFg   = 0xC55412;

constexpr uint32_t kAccentMint       = 0x2EB897;
constexpr uint32_t kAccentMintSoft   = 0xE6F8F3;
constexpr uint32_t kAccentMintFg     = 0x177E65;

constexpr uint32_t kAccentCyan       = 0x46A0F2;
constexpr uint32_t kAccentCyanSoft   = 0xE9F4FF;
constexpr uint32_t kAccentCyanFg     = 0x1D6EC0;

constexpr uint32_t kAccentGreen      = 0x2EB897;
constexpr uint32_t kAccentSlate      = 0x7A7269;

constexpr int kRadiusCard   = 24;
constexpr int kRadiusPill   = 18;
constexpr int kRadiusBtn    = 16;
constexpr int kPadScreen    = 18;
constexpr int kContentWidth = 480 - 2 * kPadScreen;
constexpr int kCardGap      = 12;
constexpr int kColumnWidth  = (kContentWidth - kCardGap) / 2;
constexpr int kTopBarHeight = 52;
constexpr int kGestureClearance = 20;

inline const lv_font_t *fontSm()   { return &db_font_16; }
inline const lv_font_t *fontMd()   { return &db_font_20; }
inline const lv_font_t *fontLg()   { return &db_font_26; }
inline const lv_font_t *fontXl()   { return &db_font_36; }
inline const lv_font_t *fontHero() { return &db_font_hero_48; }

inline lv_color_t color(uint32_t hex) { return lv_color_hex(hex); }

void styleRootScreen(lv_obj_t *screen);

lv_obj_t *createCard(lv_obj_t *parent, int w, int h, int radius = kRadiusCard);

/** Extend short controls vertically while preserving separation between adjacent targets. */
void expandTouchHeight(lv_obj_t *control, int visible_height);

class MalletKnockController {
public:
    using ApplyAngleFn = void (*)(void *context, int32_t angle, size_t frame_index);

    void attach(ApplyAngleFn apply, void *context);
    void detach();
    void stop();
    void trigger();

private:
    static void onTimer(lv_timer_t *timer);
    void applyCurrent();

    MalletMotion _motion{};
    ApplyAngleFn _apply = nullptr;
    void *_context = nullptr;
    lv_timer_t *_timer = nullptr;
    uint32_t _last_tick_ms = 0;
};

lv_obj_t *createPillButton(
    lv_obj_t *parent,
    const char *text,
    uint32_t bg_hex,
    uint32_t text_hex = kTextPrimary,
    int w = LV_SIZE_CONTENT,
    int h = 44,
    const lv_font_t *font = nullptr
);

lv_obj_t *createTopBarButton(lv_obj_t *bar, const char *text, uint32_t fg,
                             int width = 96, const lv_font_t *font = nullptr);

struct TopBarRefs {
    lv_obj_t *bar;
    lv_obj_t *back_btn;
    lv_obj_t *title_label;
    lv_obj_t *action_btn;
    lv_obj_t *action_label;
};

/** When action_text is nullptr, action_btn and action_label are nullptr. */
TopBarRefs createAppTopBar(
    lv_obj_t *parent,
    const char *title,
    uint32_t accent_hex,
    lv_event_cb_t back_cb,
    void *user_data,
    const char *action_text = nullptr,
    lv_event_cb_t action_cb = nullptr
);

lv_obj_t *createBadge(lv_obj_t *parent, const char *text, uint32_t bg_hex, uint32_t text_hex);

}
