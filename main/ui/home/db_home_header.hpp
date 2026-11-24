#pragma once

#include <functional>
#include "lvgl.h"
#include "db_random_engine.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

class HomeHeader {
public:
    struct Callbacks {
        std::function<void()> open_time;
        std::function<void()> open_fortune;
        std::function<void()> open_settings;
    };

    void create(lv_obj_t *dashboard, lv_obj_t *home, Callbacks cbs);
    void refreshClockAndStats();
    void refreshDailyContent();
    void nextMotto();
    void nextFood();

private:
    void buildMinusOneScreen(lv_obj_t *page0_main_obj);
    void buildMainPageMotto(lv_obj_t *page1_main_obj);
    bool acceptTap();

    static void onTouchPressed(lv_event_t *e);
    static void onTimeCardClicked(lv_event_t *e);
    static void onOffworkCardClicked(lv_event_t *e);
    static void onFishCardClicked(lv_event_t *e);
    static void onFortuneCardClicked(lv_event_t *e);
    static void onFoodCardClicked(lv_event_t *e);
    static void onMottoClicked(lv_event_t *e);

    Callbacks _cbs;
    RandomEngine _rng;
    int _last_motto_idx = -1;
    int _last_food_idx = -1;
    bool _time_valid = false;
    lv_point_t _press_pt{};

    lv_obj_t *_m1_date_lbl = nullptr;
    lv_obj_t *_m1_time_lbl = nullptr;
    lv_obj_t *_m1_clock_sub_lbl = nullptr;
    lv_obj_t *_m1_clock_bar = nullptr;
    lv_obj_t *_m1_merit_lbl = nullptr;
    lv_obj_t *_m1_mallet = nullptr;
    theme::MalletKnockController _mallet_knock{};
    lv_obj_t *_m1_off_title_lbl = nullptr;
    lv_obj_t *_m1_off_right_lbl = nullptr;
    lv_obj_t *_m1_off_bar = nullptr;
    lv_obj_t *_m1_fortune_stars_lbl = nullptr;
    lv_obj_t *_m1_fortune_level_lbl = nullptr;
    lv_obj_t *_m1_fortune_yi_lbl = nullptr;
    lv_obj_t *_m1_food_name_lbl = nullptr;

    lv_obj_t *_motto_label = nullptr;
};

}
