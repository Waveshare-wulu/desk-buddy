#include "daily_fortune_app.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include "db_fortune_engine.hpp"
#include "esp_random.h"
#include "services/clock_service.hpp"
#include "services/sound_service.hpp"
#include "ui/assets/db_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

DailyFortuneApp::DailyFortuneApp(std::function<void()> on_open_time_calibration)
    : DeskBuddyApp("deskbuddy.daily_fortune", i18n::tr(i18n::StrId::APP_DAILY_FORTUNE), &db_icon_daily_fortune, 1)
    , _on_open_time_calibration(std::move(on_open_time_calibration))
{
}

bool DailyFortuneApp::run(void)
{
    buildUi();
    renderFortune();
    return true;
}

bool DailyFortuneApp::resume(void)
{
    renderFortune();
    return true;
}

void DailyFortuneApp::buildUi()
{
    lv_obj_t *scr = appRoot();
    theme::styleRootScreen(scr);

    auto top = theme::createAppTopBar(
        scr,
        i18n::tr(i18n::StrId::APP_DAILY_FORTUNE),
        theme::kAccentCoralFg,
        DeskBuddyApp::onBackBtnClicked,
        this,
        "换演示签",
        onRerollDemo
    );
    _action_btn = top.action_btn;
    _daily_badge = theme::createBadge(top.bar, "每日一签", theme::kBgCanvas, theme::kTextMuted);
    lv_obj_clear_flag(_daily_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(_daily_badge, LV_ALIGN_RIGHT_MID, 0, 0);

    _warn_banner = lv_obj_create(scr);
    lv_obj_remove_style_all(_warn_banner);
    lv_obj_set_size(_warn_banner, theme::kContentWidth, 34);
    lv_obj_align(_warn_banner, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_opa(_warn_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_warn_banner, theme::color(theme::kAccentAmberSoft), 0);
    lv_obj_set_style_bg_color(_warn_banner, lv_color_darken(theme::color(theme::kAccentAmberSoft), 22), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(_warn_banner, 1, 0);
    lv_obj_set_style_border_color(_warn_banner, theme::color(0xF2D3A0), 0);
    lv_obj_set_style_radius(_warn_banner, 12, 0);
    lv_obj_set_style_pad_all(_warn_banner, 0, 0);
    lv_obj_clear_flag(_warn_banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_warn_banner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_warn_banner, onTimeCalibrationClicked, LV_EVENT_CLICKED, this);

    lv_obj_t *warn_lbl = lv_label_create(_warn_banner);
    lv_label_set_text(warn_lbl, "时间未校准 · 当前为演示签，轻点去校时 ›");
    lv_obj_set_style_text_font(warn_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(warn_lbl, theme::color(theme::kAccentAmberFg), 0);
    lv_obj_center(warn_lbl);

    auto *hero = _hero = theme::createCard(scr, theme::kContentWidth, 122);
    lv_obj_set_style_pad_hor(hero, 18, 0);
    lv_obj_set_style_pad_ver(hero, 9, 0);

    _date_lbl = lv_label_create(hero);
    lv_obj_set_style_text_font(_date_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_date_lbl, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(_date_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    _level_lbl = lv_label_create(hero);
    lv_obj_set_style_text_font(_level_lbl, theme::fontLg(), 0);
    lv_obj_set_style_text_color(_level_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_set_size(_level_lbl, 272, lv_font_get_line_height(theme::fontLg()));
    lv_label_set_long_mode(_level_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(_level_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    _stars_lbl = lv_label_create(hero);
    lv_obj_set_style_text_font(_stars_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_stars_lbl, theme::color(theme::kAccentAmber), 0);
    lv_obj_align(_stars_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    _color_card = theme::createCard(hero, 118, 94, 18);
    lv_obj_align(_color_card, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(_color_card, theme::color(theme::kBgCanvas), 0);
    lv_obj_set_style_shadow_width(_color_card, 0, 0);
    lv_obj_set_style_pad_all(_color_card, 6, 0);
    lv_obj_set_style_pad_bottom(_color_card, 2, 0);
    lv_obj_clear_flag(_color_card, LV_OBJ_FLAG_CLICKABLE);

    _color_swatch = lv_obj_create(_color_card);
    lv_obj_remove_style_all(_color_swatch);
    lv_obj_set_size(_color_swatch, LV_PCT(100), 42);
    lv_obj_set_style_bg_opa(_color_swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_color_swatch, 12, 0);
    lv_obj_set_style_border_width(_color_swatch, 1, 0);
    lv_obj_set_style_border_color(_color_swatch, theme::color(theme::kTextPrimary), 0);
    lv_obj_set_style_border_opa(_color_swatch, LV_OPA_10, 0);
    lv_obj_align(_color_swatch, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(_color_swatch, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    _color_name_lbl = lv_label_create(_color_card);
    lv_obj_set_style_text_font(_color_name_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_color_name_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_color_name_lbl, LV_ALIGN_BOTTOM_MID, 0, -lv_font_get_line_height(theme::fontSm()));

    auto *color_caption = lv_label_create(_color_card);
    lv_label_set_text_static(color_caption, "今日幸运色");
    lv_obj_set_style_text_font(color_caption, theme::fontSm(), 0);
    lv_obj_set_style_text_color(color_caption, theme::color(theme::kTextMuted), 0);
    lv_obj_align(color_caption, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto *mid_row = _yi_ji_row = lv_obj_create(scr);
    lv_obj_remove_style_all(_yi_ji_row);
    lv_obj_set_width(mid_row, theme::kContentWidth);
    lv_obj_set_style_bg_opa(mid_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mid_row, 0, 0);
    lv_obj_set_style_pad_all(mid_row, 0, 0);
    lv_obj_set_flex_flow(mid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mid_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(mid_row, LV_OBJ_FLAG_SCROLLABLE);

    auto makeYiJiCard = [&](const char *tag, uint32_t tag_fg, lv_obj_t **out_lbl) {
        lv_obj_t *c = theme::createCard(mid_row, theme::kColumnWidth, LV_PCT(100), 20);
        lv_obj_set_style_pad_hor(c, 16, 0);
        lv_obj_set_style_pad_ver(c, 12, 0);

        lv_obj_t *t = lv_label_create(c);
        lv_label_set_text_static(t, tag);
        lv_obj_set_style_text_font(t, theme::fontSm(), 0);
        lv_obj_set_style_text_color(t, theme::color(tag_fg), 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *v = lv_label_create(c);
        lv_obj_set_width(v, LV_PCT(100));
        lv_obj_set_style_text_font(v, theme::fontMd(), 0);
        lv_obj_set_style_text_color(v, theme::color(theme::kTextPrimary), 0);
        lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        *out_lbl = v;
    };

    makeYiJiCard("宜", theme::kAccentMintFg, &_yi_lbl);
    makeYiJiCard("忌", theme::kAccentCoralFg, &_ji_lbl);

    auto *qcard = _quote_card = theme::createCard(scr, theme::kContentWidth, 104, 20);
    lv_obj_set_style_pad_hor(qcard, 16, 0);
    lv_obj_set_style_pad_ver(qcard, 10, 0);

    lv_obj_t *qt = lv_label_create(qcard);
    lv_label_set_text(qt, "今日签文");
    lv_obj_set_style_text_font(qt, theme::fontSm(), 0);
    lv_obj_set_style_text_color(qt, theme::color(theme::kAccentCoralFg), 0);
    lv_obj_align(qt, LV_ALIGN_TOP_LEFT, 0, 0);

    _quote_lbl = lv_label_create(qcard);
    lv_obj_set_width(_quote_lbl, LV_PCT(100));
    lv_obj_set_style_text_font(_quote_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_quote_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_quote_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    lv_obj_t *foot = lv_label_create(qcard);
    lv_label_set_text(foot, "仅供日常摸鱼娱乐");
    lv_obj_set_style_text_font(foot, theme::fontSm(), 0);
    lv_obj_set_style_text_color(foot, theme::color(theme::kTextMuted), 0);
    lv_obj_align(foot, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void DailyFortuneApp::renderFortune()
{
    const DateTime dt = ClockService::instance().getNow();
    const DailyFortune f = FortuneEngine::generate(dt.year, dt.month, dt.day, dt.valid, _demo_seed);

    lv_obj_set_height(_hero, f.time_valid ? 122 : 108);
    lv_obj_set_height(_color_card, f.time_valid ? 94 : 88);
    lv_obj_set_height(_color_swatch, f.time_valid ? 42 : 38);
    lv_obj_align(_hero, LV_ALIGN_TOP_MID, 0, f.time_valid ? 58 : 98);
    lv_obj_set_height(_yi_ji_row, f.time_valid ? 110 : 98);
    lv_obj_align(_yi_ji_row, LV_ALIGN_TOP_MID, 0, f.time_valid ? 192 : 216);
    lv_obj_set_height(_quote_card, f.time_valid ? 104 : 94);
    lv_obj_align(_quote_card, LV_ALIGN_TOP_MID, 0, f.time_valid ? 314 : 324);

    if (f.time_valid) {
        lv_obj_add_flag(_warn_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_action_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_daily_badge, LV_OBJ_FLAG_HIDDEN);
        char dbuf[64];
        std::snprintf(dbuf, sizeof(dbuf), "%04d年%02d月%02d日 · %s",
                      f.year, f.month, f.day, ClockService::weekdayChinese(dt.weekday));
        lv_label_set_text(_date_lbl, dbuf);
    } else {
        lv_obj_clear_flag(_warn_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_action_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_daily_badge, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_date_lbl, "日期未知 · 演示签");
    }

    lv_label_set_text(_level_lbl, f.level_title);

    std::string stars = "摸鱼指数 ";
    for (int i = 0; i < 5; ++i) {
        stars += (i < f.slacking_stars) ? "★" : "☆";
    }
    lv_label_set_text(_stars_lbl, stars.c_str());

    lv_obj_set_style_bg_color(_color_swatch, theme::color(f.lucky_color.rgb_hex), 0);
    lv_label_set_text(_color_name_lbl, f.lucky_color.name);

    lv_label_set_text(_yi_lbl, f.yi_text);
    lv_label_set_text(_ji_lbl, f.ji_text);
    lv_label_set_text(_quote_lbl, f.quote);
}

void DailyFortuneApp::onTimeCalibrationClicked(lv_event_t *e)
{
    auto *self = static_cast<DailyFortuneApp *>(lv_event_get_user_data(e));
    if (!self || !self->_on_open_time_calibration) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_on_open_time_calibration();
}

void DailyFortuneApp::onRerollDemo(lv_event_t *e)
{
    auto *self = static_cast<DailyFortuneApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_demo_seed = esp_random();
    self->renderFortune();
}

}
