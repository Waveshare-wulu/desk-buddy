#include "db_home_header.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include "db_fortune_engine.hpp"
#include "db_time_logic.hpp"
#include "esp_random.h"
#include "services/clock_service.hpp"
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "ui/assets/db_woodfish_art.h"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {

lv_obj_t *createDashboardCard(lv_obj_t *parent, int width, int height, int radius)
{
    auto *card = theme::createCard(parent, width, height, radius);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
    return card;
}

void makeCardInteractive(lv_obj_t *card, lv_event_cb_t press_cb, lv_event_cb_t click_cb, void *ud)
{
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_border_color(card, theme::color(theme::kBgCardPressed), LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, press_cb, LV_EVENT_PRESSED, ud);
    lv_obj_add_event_cb(card, click_cb, LV_EVENT_CLICKED, ud);
}

lv_obj_t *createProgressBar(lv_obj_t *parent, int width, int height)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, width, height);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, theme::color(theme::kBgTrack), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, theme::color(theme::kAccentMint), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    return bar;
}

void applyHomeMalletAngle(void *context, int32_t angle, size_t)
{
    auto *mallet = static_cast<lv_obj_t *>(context);
    if (mallet) {
        lv_image_set_rotation(mallet, angle);
    }
}

}

void HomeHeader::create(lv_obj_t *dashboard, lv_obj_t *home, Callbacks cbs)
{
    _cbs = std::move(cbs);
    _rng = RandomEngine([]() -> uint32_t { return esp_random(); });
    buildMinusOneScreen(dashboard);
    buildMainPageMotto(home);

    nextMotto();
    nextFood();
    refreshDailyContent();
    refreshClockAndStats();
}

void HomeHeader::buildMinusOneScreen(lv_obj_t *page0_main_obj)
{
    lv_obj_t *root = lv_obj_create(page0_main_obj);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 412);
    lv_obj_align(root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *clock_card = createDashboardCard(root, 252, 148, 24);
    lv_obj_set_pos(clock_card, 16, 6);
    lv_obj_set_style_bg_color(clock_card, theme::color(0xFFFFFF), 0);
    lv_obj_set_style_border_color(clock_card, theme::color(0xF0DFC6), 0);
    lv_obj_set_style_pad_hor(clock_card, 18, 0);
    lv_obj_set_style_pad_ver(clock_card, 14, 0);
    makeCardInteractive(clock_card, onTouchPressed, onTimeCardClicked, this);

    _m1_date_lbl = lv_label_create(clock_card);
    lv_obj_set_style_text_font(_m1_date_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_date_lbl, theme::color(0x9F5E1B), 0);
    lv_obj_set_style_bg_color(_m1_date_lbl, theme::color(0xFFF0D8), 0);
    lv_obj_set_style_bg_opa(_m1_date_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(_m1_date_lbl, 12, 0);
    lv_obj_set_style_pad_ver(_m1_date_lbl, 3, 0);
    lv_obj_set_style_radius(_m1_date_lbl, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(_m1_date_lbl, LV_ALIGN_TOP_MID, 0, -3);

    _m1_time_lbl = lv_label_create(clock_card);
    lv_label_set_recolor(_m1_time_lbl, true);
    lv_obj_set_style_text_font(_m1_time_lbl, theme::fontHero(), 0);
    lv_obj_set_style_text_color(_m1_time_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_m1_time_lbl, LV_ALIGN_CENTER, 0, -1);

    _m1_clock_bar = createProgressBar(clock_card, 172, 6);
    lv_obj_align(_m1_clock_bar, LV_ALIGN_BOTTOM_MID, 0, -21);

    _m1_clock_sub_lbl = lv_label_create(clock_card);
    lv_obj_set_style_text_font(_m1_clock_sub_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_clock_sub_lbl, theme::color(theme::kAccentMintFg), 0);
    lv_obj_align(_m1_clock_sub_lbl, LV_ALIGN_BOTTOM_MID, 0, 2);

    lv_obj_t *fish_card = createDashboardCard(root, 184, 148, 24);
    lv_obj_set_pos(fish_card, 280, 6);
    lv_obj_set_style_bg_color(fish_card, theme::color(0xFFF6E8), 0);
    lv_obj_set_style_border_color(fish_card, theme::color(0xF2D7AC), 0);
    lv_obj_set_style_pad_hor(fish_card, 14, 0);
    lv_obj_set_style_pad_ver(fish_card, 12, 0);
    makeCardInteractive(fish_card, onTouchPressed, onFishCardClicked, this);

    lv_obj_t *fish_title = lv_label_create(fish_card);
    lv_label_set_text(fish_title, "直接敲");
    lv_obj_set_style_text_font(fish_title, theme::fontSm(), 0);
    lv_obj_set_style_text_color(fish_title, theme::color(theme::kAccentAmberFg), 0);
    lv_obj_align(fish_title, LV_ALIGN_TOP_LEFT, 0, 0);

    _m1_merit_lbl = lv_label_create(fish_card);
    lv_label_set_text(_m1_merit_lbl, "0");
    lv_obj_set_style_text_font(_m1_merit_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_merit_lbl, theme::color(0xC46C18), 0);
    lv_obj_align(_m1_merit_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    auto *fish = lv_image_create(fish_card);
    lv_image_set_src(fish, &db_art_woodfish_sm);
    lv_obj_align(fish, LV_ALIGN_CENTER, -4, 3);

    _m1_mallet = lv_image_create(fish_card);
    lv_image_set_src(_m1_mallet, &db_art_mallet_sm);
    lv_obj_align(_m1_mallet, LV_ALIGN_TOP_RIGHT, -10, 14);
    lv_image_set_pivot(_m1_mallet, 60, 6);
    _mallet_knock.attach(applyHomeMalletAngle, _m1_mallet);

    lv_obj_t *fish_hint = lv_label_create(fish_card);
    lv_label_set_text(fish_hint, "轻点功德 +1");
    lv_obj_set_style_text_font(fish_hint, theme::fontSm(), 0);
    lv_obj_set_style_text_color(fish_hint, theme::color(0xA6692A), 0);
    lv_obj_align(fish_hint, LV_ALIGN_BOTTOM_MID, 0, 2);

    lv_obj_t *off_card = createDashboardCard(root, 448, 78, 22);
    lv_obj_set_pos(off_card, 16, 166);
    lv_obj_set_style_pad_hor(off_card, 18, 0);
    lv_obj_set_style_pad_ver(off_card, 12, 0);
    makeCardInteractive(off_card, onTouchPressed, onOffworkCardClicked, this);

    _m1_off_title_lbl = lv_label_create(off_card);
    lv_label_set_text(_m1_off_title_lbl, "下班自由进度");
    lv_obj_set_style_text_font(_m1_off_title_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentMintFg), 0);
    lv_obj_align(_m1_off_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    _m1_off_right_lbl = lv_label_create(off_card);
    lv_label_set_text(_m1_off_right_lbl, "目标 18:00");
    lv_obj_set_style_text_font(_m1_off_right_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_off_right_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_m1_off_right_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    _m1_off_bar = createProgressBar(off_card, 410, 14);
    lv_obj_align(_m1_off_bar, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_t *fort_card = createDashboardCard(root, 218, 144, 24);
    lv_obj_set_pos(fort_card, 16, 256);
    lv_obj_set_style_bg_color(fort_card, theme::color(0xFFFFFF), 0);
    lv_obj_set_style_border_color(fort_card, theme::color(0xF6DADF), 0);
    lv_obj_set_style_pad_hor(fort_card, 16, 0);
    lv_obj_set_style_pad_ver(fort_card, 14, 0);
    makeCardInteractive(fort_card, onTouchPressed, onFortuneCardClicked, this);

    lv_obj_t *ft_title = lv_label_create(fort_card);
    lv_label_set_text(ft_title, "今日运势");
    lv_obj_set_style_text_font(ft_title, theme::fontSm(), 0);
    lv_obj_set_style_text_color(ft_title, theme::color(theme::kAccentCoralFg), 0);
    lv_obj_align(ft_title, LV_ALIGN_TOP_LEFT, 0, 0);

    _m1_fortune_stars_lbl = lv_label_create(fort_card);
    lv_obj_set_style_text_font(_m1_fortune_stars_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_fortune_stars_lbl, theme::color(theme::kAccentAmber), 0);
    lv_obj_align(_m1_fortune_stars_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    _m1_fortune_level_lbl = lv_label_create(fort_card);
    lv_obj_set_width(_m1_fortune_level_lbl, 184);
    lv_label_set_long_mode(_m1_fortune_level_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(_m1_fortune_level_lbl, theme::fontMd(), 0);
    lv_obj_set_style_text_color(_m1_fortune_level_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_m1_fortune_level_lbl, LV_ALIGN_LEFT_MID, 0, 2);

    _m1_fortune_yi_lbl = lv_label_create(fort_card);
    lv_obj_set_width(_m1_fortune_yi_lbl, 184);
    lv_label_set_long_mode(_m1_fortune_yi_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(_m1_fortune_yi_lbl, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_m1_fortune_yi_lbl, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(_m1_fortune_yi_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *food_card = createDashboardCard(root, 218, 144, 24);
    lv_obj_set_pos(food_card, 246, 256);
    lv_obj_set_style_bg_color(food_card, theme::color(0xFFFFFF), 0);
    lv_obj_set_style_border_color(food_card, theme::color(0xE3DCFA), 0);
    lv_obj_set_style_pad_hor(food_card, 16, 0);
    lv_obj_set_style_pad_ver(food_card, 14, 0);
    makeCardInteractive(food_card, onTouchPressed, onFoodCardClicked, this);

    lv_obj_t *fd_title = lv_label_create(food_card);
    lv_label_set_text(fd_title, "今天吃啥");
    lv_obj_set_style_text_font(fd_title, theme::fontSm(), 0);
    lv_obj_set_style_text_color(fd_title, theme::color(theme::kAccentVioletFg), 0);
    lv_obj_align(fd_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *fd_reroll = lv_label_create(food_card);
    lv_label_set_text(fd_reroll, "换一个");
    lv_obj_set_style_text_font(fd_reroll, theme::fontSm(), 0);
    lv_obj_set_style_text_color(fd_reroll, theme::color(theme::kAccentViolet), 0);
    lv_obj_align(fd_reroll, LV_ALIGN_TOP_RIGHT, 0, 0);

    _m1_food_name_lbl = lv_label_create(food_card);
    lv_obj_set_width(_m1_food_name_lbl, 184);
    lv_label_set_long_mode(_m1_food_name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(_m1_food_name_lbl, theme::fontMd(), 0);
    lv_obj_set_style_text_color(_m1_food_name_lbl, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_m1_food_name_lbl, LV_ALIGN_LEFT_MID, 0, 2);

    lv_obj_t *fd_hint = lv_label_create(food_card);
    lv_label_set_text(fd_hint, "轻点直接随机抽");
    lv_obj_set_style_text_font(fd_hint, theme::fontSm(), 0);
    lv_obj_set_style_text_color(fd_hint, theme::color(theme::kTextSecondary), 0);
    lv_obj_align(fd_hint, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void HomeHeader::buildMainPageMotto(lv_obj_t *page1_main_obj)
{
    _motto_label = lv_label_create(page1_main_obj);
    lv_obj_set_width(_motto_label, 420);
    lv_label_set_long_mode(_motto_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(_motto_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_motto_label, theme::fontSm(), 0);
    lv_obj_set_style_text_color(_motto_label, theme::color(theme::kTextMuted), 0);
    lv_obj_align(_motto_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(_motto_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_motto_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(_motto_label, onTouchPressed, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(_motto_label, onMottoClicked, LV_EVENT_CLICKED, this);
}

void HomeHeader::refreshClockAndStats()
{
    const DateTime dt = ClockService::instance().getNow();
    const auto &cfg = StorageService::instance().getSettings();
    _time_valid = dt.valid;

    char buf[80];
    if (_m1_time_lbl) {
        if (dt.valid) {
            std::snprintf(buf, sizeof(buf), "%02d#F29838 :#%02d", dt.hour, dt.minute);
        } else {
            std::snprintf(buf, sizeof(buf), "--#F29838 :#--");
        }
        lv_label_set_text(_m1_time_lbl, buf);
    }

    if (_m1_date_lbl) {
        if (dt.valid) {
            std::snprintf(buf, sizeof(buf), "%02d月%02d日 · %s",
                          dt.month, dt.day, ClockService::weekdayChinese(dt.weekday));
        } else {
            std::snprintf(buf, sizeof(buf), "时间未校准");
        }
        lv_label_set_text(_m1_date_lbl, buf);
    }

    const OffWorkStatus ow = computeOffWorkCountdown(
        dt.valid, dt.hour, dt.minute, dt.second, cfg.offwork_hour, cfg.offwork_min,
        cfg.work_start_hour, cfg.work_start_min);
    const bool before_work = ow.phase == WorkDayPhase::BEFORE_WORK;

    if (_m1_clock_bar) {
        lv_bar_set_value(_m1_clock_bar, ow.progress_percent, LV_ANIM_OFF);
    }

    if (_m1_clock_sub_lbl) {
        if (!ow.time_valid) {
            lv_label_set_text(_m1_clock_sub_lbl, "请前往设置校时");
            lv_obj_set_style_text_color(_m1_clock_sub_lbl, theme::color(theme::kAccentAmberFg), 0);
        } else if (ow.phase == WorkDayPhase::INVALID) {
            lv_label_set_text(_m1_clock_sub_lbl, "请调整上下班时间");
            lv_obj_set_style_text_color(_m1_clock_sub_lbl, theme::color(theme::kAccentCoralFg), 0);
        } else if (ow.phase == WorkDayPhase::OFF_WORK) {
            lv_label_set_text(_m1_clock_sub_lbl, "已经下班啦!");
            lv_obj_set_style_text_color(_m1_clock_sub_lbl, theme::color(theme::kAccentMintFg), 0);
        } else {
            const int32_t h = ow.remaining_seconds / 3600;
            const int32_t m = (ow.remaining_seconds % 3600) / 60;
            std::snprintf(buf, sizeof(buf), "%s %02ld:%02ld", before_work ? "距上班" : "距下班",
                          static_cast<long>(h), static_cast<long>(m));
            lv_label_set_text(_m1_clock_sub_lbl, buf);
            lv_obj_set_style_text_color(_m1_clock_sub_lbl,
                theme::color(before_work ? theme::kAccentCyanFg : theme::kAccentMintFg), 0);
        }
    }

    if (_m1_off_title_lbl && _m1_off_right_lbl && _m1_off_bar) {
        if (!ow.time_valid) {
            lv_label_set_text(_m1_off_title_lbl, "下班倒计时 · 待校准时间");
            lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentAmberFg), 0);
            lv_label_set_text(_m1_off_right_lbl, "去设置");
        } else if (ow.phase == WorkDayPhase::INVALID) {
            lv_label_set_text(_m1_off_title_lbl, "请调整上下班时间");
            lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentCoralFg), 0);
            lv_label_set_text(_m1_off_right_lbl, "去修改");
        } else if (before_work) {
            lv_label_set_text(_m1_off_title_lbl, "尚未到上班时间");
            lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentCyanFg), 0);
            lv_label_set_text_fmt(_m1_off_right_lbl, "上班 %02d:%02d", cfg.work_start_hour, cfg.work_start_min);
        } else if (ow.phase == WorkDayPhase::OFF_WORK) {
            lv_label_set_text(_m1_off_title_lbl, "今日已下班，尽情放松吧");
            lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentMintFg), 0);
            lv_label_set_text(_m1_off_right_lbl, "100%");
        } else {
            lv_label_set_text(_m1_off_title_lbl, "下班自由进度条");
            lv_obj_set_style_text_color(_m1_off_title_lbl, theme::color(theme::kAccentMintFg), 0);
            std::snprintf(buf, sizeof(buf), "%d%% · 目标 %02u:%02u",
                          ow.progress_percent, cfg.offwork_hour, cfg.offwork_min);
            lv_label_set_text(_m1_off_right_lbl, buf);
        }
        lv_bar_set_value(_m1_off_bar, ow.progress_percent, LV_ANIM_OFF);
    }

    if (_m1_merit_lbl) {
        std::snprintf(buf, sizeof(buf), "%lu",
                      static_cast<unsigned long>(StorageService::instance().getMeritTotal()));
        lv_label_set_text(_m1_merit_lbl, buf);
    }
}

void HomeHeader::refreshDailyContent()
{
    const DateTime dt = ClockService::instance().getNow();
    const DailyFortune f = FortuneEngine::generate(dt.year, dt.month, dt.day, dt.valid, 20261120u);

    if (_m1_fortune_stars_lbl) {
        std::string stars;
        for (int i = 0; i < 5; ++i) {
            stars += (i < f.slacking_stars) ? "★" : "☆";
        }
        lv_label_set_text(_m1_fortune_stars_lbl, stars.c_str());
    }
    if (_m1_fortune_level_lbl) {
        lv_label_set_text(_m1_fortune_level_lbl, f.level_title);
    }
    if (_m1_fortune_yi_lbl) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "宜：%s", f.yi_text);
        lv_label_set_text(_m1_fortune_yi_lbl, buf);
    }
}

void HomeHeader::nextMotto()
{
    if (!_motto_label) return;
    const char *motto = _rng.pickMotto(&_last_motto_idx);
    lv_label_set_text(_motto_label, motto);
}

void HomeHeader::nextFood()
{
    if (!_m1_food_name_lbl) return;
    const auto &item = _rng.pickFood(FoodCategory::ALL, &_last_food_idx);
    lv_label_set_text(_m1_food_name_lbl, item.name);
}

bool HomeHeader::acceptTap()
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return true;
    lv_point_t cur{};
    lv_indev_get_point(indev, &cur);
    return (std::abs(cur.x - _press_pt.x) < 22) && (std::abs(cur.y - _press_pt.y) < 22);
}

void HomeHeader::onTouchPressed(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    lv_indev_t *indev = lv_indev_active();
    if (self && indev) {
        lv_indev_get_point(indev, &self->_press_pt);
    }
}

void HomeHeader::onTimeCardClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (self->_cbs.open_time) {
        self->_cbs.open_time();
    }
}

void HomeHeader::onOffworkCardClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (!self->_time_valid && self->_cbs.open_settings) {
        self->_cbs.open_settings();
    } else if (self->_cbs.open_time) {
        self->_cbs.open_time();
    }
}

void HomeHeader::onFishCardClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;

    StorageService::instance().addMerit(1);
    SoundService::instance().play(SoundEffect::WOOD_FISH);

    if (self->_m1_merit_lbl) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lu",
                      static_cast<unsigned long>(StorageService::instance().getMeritTotal()));
        lv_label_set_text(self->_m1_merit_lbl, buf);
    }

    self->_mallet_knock.trigger();
}

void HomeHeader::onFortuneCardClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;
    SoundService::instance().play(SoundEffect::CLICK);
    if (self->_cbs.open_fortune) {
        self->_cbs.open_fortune();
    }
}

void HomeHeader::onFoodCardClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->nextFood();
}

void HomeHeader::onMottoClicked(lv_event_t *e)
{
    auto *self = static_cast<HomeHeader *>(lv_event_get_user_data(e));
    if (!self || !self->acceptTap()) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->nextMotto();
}

}
