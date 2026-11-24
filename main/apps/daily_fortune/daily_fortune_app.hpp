#pragma once

#include <cstdint>
#include "apps/db_base_app.hpp"

namespace desk_buddy {

class DailyFortuneApp : public DeskBuddyApp {
public:
    DailyFortuneApp();
    ~DailyFortuneApp() override = default;

protected:
    bool run(void) override;
    bool resume(void) override;

private:
    void buildUi();
    void renderFortune();
    static void onRerollDemo(lv_event_t *e);

    uint32_t _demo_seed = 20261120u;
    lv_obj_t *_warn_banner = nullptr;
    lv_obj_t *_daily_badge = nullptr;
    lv_obj_t *_hero = nullptr;
    lv_obj_t *_yi_ji_row = nullptr;
    lv_obj_t *_quote_card = nullptr;
    lv_obj_t *_date_lbl = nullptr;
    lv_obj_t *_stars_lbl = nullptr;
    lv_obj_t *_level_lbl = nullptr;
    lv_obj_t *_color_card = nullptr;
    lv_obj_t *_color_swatch = nullptr;
    lv_obj_t *_color_name_lbl = nullptr;
    lv_obj_t *_yi_lbl = nullptr;
    lv_obj_t *_ji_lbl = nullptr;
    lv_obj_t *_quote_lbl = nullptr;
    lv_obj_t *_action_btn = nullptr;
};

}
