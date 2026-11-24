#pragma once

#include <cstdint>
#include "apps/db_base_app.hpp"
#include "db_game_2048_engine.hpp"

namespace desk_buddy {

class Game2048App : public DeskBuddyApp {
public:
    Game2048App();
    ~Game2048App() override = default;

protected:
    bool run(void) override;
    bool pause(void) override;
    bool close(void) override;

private:
    void buildUi();
    void renderBoard(const MoveResult *last_move = nullptr);
    void applyMove(MoveDir dir);
    void saveProgress();
    void showOverlay(const char *title, const char *sub, bool allow_continue);
    void hideOverlay();

    static void onBoardTouch(lv_event_t *e);
    static void onRestartClicked(lv_event_t *e);
    static void onContinueClicked(lv_event_t *e);

    Game2048Engine _engine;
    bool _has_loaded_once = false;
    bool _dismissed_win = false;

    lv_point_t _touch_start{};
    bool _touch_tracking = false;

    lv_obj_t *_score_lbl = nullptr;
    lv_obj_t *_best_lbl = nullptr;
    lv_obj_t *_board_box = nullptr;
    lv_obj_t *_tile_objs[4][4] = {};
    lv_obj_t *_tile_lbls[4][4] = {};
    lv_obj_t *_overlay = nullptr;
    lv_obj_t *_overlay_title = nullptr;
    lv_obj_t *_overlay_sub = nullptr;
    lv_obj_t *_overlay_continue_btn = nullptr;
};

}
