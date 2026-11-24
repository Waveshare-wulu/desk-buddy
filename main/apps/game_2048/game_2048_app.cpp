#include "game_2048_app.hpp"

#include <cstdio>
#include <cstdlib>
#include "esp_random.h"
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "ui/assets/db_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {

struct TileStyle {
    uint32_t bg;
    uint32_t fg;
};

TileStyle styleForTile(uint16_t val)
{
    switch (val) {
        case 0:    return {0xF3ECE2, 0xF3ECE2};
        case 2:    return {0xFFFDF9, 0x5A4A3F};
        case 4:    return {0xFCECD6, 0x5A4A3F};
        case 8:    return {0xFBB876, 0xFFFFFF};
        case 16:   return {0xF79056, 0xFFFFFF};
        case 32:   return {0xF56E78, 0xFFFFFF};
        case 64:   return {0xEB5463, 0xFFFFFF};
        case 128:  return {0xF6CA54, 0x5A410B};
        case 256:  return {0x8AD06B, 0xFFFFFF};
        case 512:  return {0x42BFA1, 0xFFFFFF};
        case 1024: return {0x52A9F5, 0xFFFFFF};
        case 2048: return {0x9378F7, 0xFFFFFF};
        default:   return {0xE86399, 0xFFFFFF};
    }
}

void animTileScale(void *obj, int32_t v)
{
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t *>(obj), v, 0);
}

void popTile(lv_obj_t *tile, int start_scale)
{
    lv_anim_delete(tile, animTileScale);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tile);
    lv_anim_set_exec_cb(&a, animTileScale);
    lv_anim_set_values(&a, start_scale, 256);
    lv_anim_set_duration(&a, 140);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

}

Game2048App::Game2048App()
    : DeskBuddyApp("deskbuddy.game_2048", i18n::tr(i18n::StrId::APP_GAME_2048), &db_icon_game_2048, 1)
    , _engine([]() -> uint32_t { return esp_random(); })
{
}

bool Game2048App::run(void)
{
    if (!_has_loaded_once) {
        Game2048State saved{};
        if (StorageService::instance().load2048State(saved)) {
            if (!_engine.loadState(saved)) {
                _engine.reset(saved.best_score);
            }
            _dismissed_win = _engine.getState().won_2048;
        }
        _has_loaded_once = true;
    }

    buildUi();
    renderBoard(nullptr);
    if (_engine.getState().game_over) {
        char sub[64];
        std::snprintf(sub, sizeof(sub), "最终得分 %lu", static_cast<unsigned long>(_engine.getState().score));
        showOverlay("本局已结束", sub, false);
    }
    return true;
}

bool Game2048App::pause(void)
{
    saveProgress();
    return DeskBuddyApp::pause();
}

bool Game2048App::close(void)
{
    saveProgress();
    _board_box = nullptr;
    _overlay = nullptr;
    return DeskBuddyApp::close();
}

void Game2048App::saveProgress()
{
    StorageService::instance().save2048State(_engine.getState());
}

void Game2048App::buildUi()
{
    lv_obj_t *scr = appRoot();
    theme::styleRootScreen(scr);

    theme::createAppTopBar(
        scr,
        "2048",
        theme::kAccentOrangeFg,
        DeskBuddyApp::onBackBtnClicked,
        this,
        "重开",
        onRestartClicked
    );

    constexpr int kBoardSize = 314;
    constexpr int kPad = 8;
    constexpr int kGap = 6;
    constexpr int kTileSize = (kBoardSize - 2 * kPad - 3 * kGap) / 4;
    constexpr int kBoardBorder = 2;

    lv_obj_t *score_row = lv_obj_create(scr);
    lv_obj_remove_style_all(score_row);
    lv_obj_set_size(score_row, kBoardSize, 40);
    lv_obj_align(score_row, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_opa(score_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(score_row, 0, 0);
    lv_obj_set_style_pad_all(score_row, 0, 0);
    lv_obj_set_flex_flow(score_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(score_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(score_row, LV_OBJ_FLAG_SCROLLABLE);

    auto makePillScore = [&](const char *name, uint32_t col, lv_obj_t **out_val) {
        lv_obj_t *p = theme::createCard(score_row, (kBoardSize - theme::kCardGap) / 2, 40, 14);
        lv_obj_set_style_pad_hor(p, 14, 0);
        lv_obj_set_style_pad_ver(p, 4, 0);
        lv_obj_t *n = lv_label_create(p);
        lv_label_set_text(n, name);
        lv_obj_set_style_text_font(n, theme::fontSm(), 0);
        lv_obj_set_style_text_color(n, theme::color(theme::kTextSecondary), 0);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *v = lv_label_create(p);
        lv_label_set_text(v, "0");
        lv_obj_set_style_text_font(v, theme::fontMd(), 0);
        lv_obj_set_style_text_color(v, theme::color(col), 0);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
        *out_val = v;
    };

    makePillScore("得分", theme::kAccentOrangeFg, &_score_lbl);
    makePillScore("最高", theme::kAccentMintFg, &_best_lbl);

    _board_box = lv_obj_create(scr);
    lv_obj_remove_style_all(_board_box);
    lv_obj_set_size(_board_box, kBoardSize, kBoardSize);
    lv_obj_align(_board_box, LV_ALIGN_TOP_MID, 0, 104);
    lv_obj_set_style_bg_opa(_board_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_board_box, theme::color(0xE6DDD0), 0);
    lv_obj_set_style_radius(_board_box, 22, 0);
    lv_obj_set_style_border_width(_board_box, kBoardBorder, 0);
    lv_obj_set_style_border_color(_board_box, theme::color(0xDDD1C0), 0);
    lv_obj_set_style_pad_all(_board_box, 0, 0);
    lv_obj_clear_flag(_board_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_board_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_board_box, onBoardTouch, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(_board_box, onBoardTouch, LV_EVENT_RELEASED, this);

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            lv_obj_t *t = lv_obj_create(_board_box);
            lv_obj_remove_style_all(t);
            lv_obj_set_size(t, kTileSize, kTileSize);
            lv_obj_set_pos(t, kPad - kBoardBorder + c * (kTileSize + kGap),
                           kPad - kBoardBorder + r * (kTileSize + kGap));
            lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(t, 14, 0);
            lv_obj_set_style_border_width(t, 0, 0);
            lv_obj_set_style_pad_all(t, 0, 0);
            lv_obj_set_style_transform_pivot_x(t, kTileSize / 2, 0);
            lv_obj_set_style_transform_pivot_y(t, kTileSize / 2, 0);
            lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t *l = lv_label_create(t);
            lv_label_set_text(l, "");
            lv_obj_center(l);

            _tile_objs[r][c] = t;
            _tile_lbls[r][c] = l;
        }
    }

    _overlay = lv_obj_create(_board_box);
    lv_obj_remove_style_all(_overlay);
    lv_obj_set_size(_overlay, kBoardSize, kBoardSize);
    lv_obj_center(_overlay);
    lv_obj_set_style_bg_color(_overlay, theme::color(0xFAF6EE), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
    lv_obj_set_style_radius(_overlay, 20, 0);
    lv_obj_set_style_border_width(_overlay, 0, 0);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

    _overlay_title = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_overlay_title, theme::fontLg(), 0);
    lv_obj_set_style_text_color(_overlay_title, theme::color(theme::kAccentOrangeFg), 0);
    lv_obj_align(_overlay_title, LV_ALIGN_CENTER, 0, -56);

    _overlay_sub = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_overlay_sub, theme::fontMd(), 0);
    lv_obj_set_style_text_color(_overlay_sub, theme::color(theme::kTextPrimary), 0);
    lv_obj_align(_overlay_sub, LV_ALIGN_CENTER, 0, -14);

    _overlay_continue_btn = theme::createPillButton(_overlay, "继续挑战", theme::kAccentMint, theme::kTextOnAccent, 200, 54, theme::fontMd());
    lv_obj_align(_overlay_continue_btn, LV_ALIGN_CENTER, 0, 32);
    lv_obj_add_event_cb(_overlay_continue_btn, onContinueClicked, LV_EVENT_CLICKED, this);

    lv_obj_t *again = theme::createPillButton(_overlay, "再来一局", theme::kAccentOrange, theme::kTextOnAccent, 200, 54, theme::fontMd());
    lv_obj_align(again, LV_ALIGN_CENTER, 0, 98);
    lv_obj_add_event_cb(again, onRestartClicked, LV_EVENT_CLICKED, this);

    hideOverlay();
}

void Game2048App::renderBoard(const MoveResult *last_move)
{
    const auto &st = _engine.getState();

    char buf[24];
    std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(st.score));
    if (_score_lbl) lv_label_set_text(_score_lbl, buf);
    std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(st.best_score));
    if (_best_lbl) lv_label_set_text(_best_lbl, buf);

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const uint16_t v = st.board[r][c];
            const TileStyle ts = styleForTile(v);
            lv_obj_set_style_bg_color(_tile_objs[r][c], theme::color(ts.bg), 0);

            if (v == 0) {
                lv_label_set_text(_tile_lbls[r][c], "");
            } else {
                std::snprintf(buf, sizeof(buf), "%u", v);
                lv_label_set_text(_tile_lbls[r][c], buf);
                lv_obj_set_style_text_color(_tile_lbls[r][c], theme::color(ts.fg), 0);
                const lv_font_t *f = (v >= 1024) ? theme::fontSm() : (v >= 128 ? theme::fontMd() : theme::fontLg());
                lv_obj_set_style_text_font(_tile_lbls[r][c], f, 0);
            }

            if (last_move) {
                if (last_move->spawned && last_move->spawn_row == r && last_move->spawn_col == c) {
                    popTile(_tile_objs[r][c], 150);
                } else if (last_move->merged_mask[r][c]) {
                    popTile(_tile_objs[r][c], 295);
                }
            }
        }
    }
}

void Game2048App::applyMove(MoveDir dir)
{
    if (_overlay && !lv_obj_has_flag(_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    const uint32_t old_best = _engine.getState().best_score;
    MoveResult res = _engine.move(dir);
    if (!res.moved) {
        return;
    }

    SoundService::instance().play(res.score_gained > 0 ? SoundEffect::CLICK : SoundEffect::CLICK);
    renderBoard(&res);

    if (res.newly_won && !_dismissed_win) {
        SoundService::instance().play(SoundEffect::COMPLETE);
        saveProgress();
        showOverlay("恭喜合成 2048!", "继续冲刺更高分吧", true);
    } else if (_engine.getState().game_over) {
        saveProgress();
        char sub[64];
        std::snprintf(sub, sizeof(sub), "最终得分 %lu", static_cast<unsigned long>(_engine.getState().score));
        showOverlay("本局结束", sub, false);
    } else if (_engine.getState().best_score > old_best && (_engine.getState().score % 128 == 0)) {
        saveProgress();
    }
}

void Game2048App::showOverlay(const char *title, const char *sub, bool allow_continue)
{
    if (!_overlay) return;
    lv_label_set_text(_overlay_title, title);
    lv_label_set_text(_overlay_sub, sub);
    if (allow_continue) {
        lv_obj_clear_flag(_overlay_continue_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_overlay_continue_btn, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void Game2048App::hideOverlay()
{
    if (_overlay) {
        lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void Game2048App::onBoardTouch(lv_event_t *e)
{
    auto *self = static_cast<Game2048App *>(lv_event_get_user_data(e));
    if (!self) return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t pt{};
    lv_indev_get_point(indev, &pt);

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        self->_touch_start = pt;
        self->_touch_tracking = true;
    } else if (code == LV_EVENT_RELEASED && self->_touch_tracking) {
        self->_touch_tracking = false;
        const int dx = pt.x - self->_touch_start.x;
        const int dy = pt.y - self->_touch_start.y;
        const int adx = std::abs(dx);
        const int ady = std::abs(dy);
        constexpr int kSwipeThresholdPx = 28;
        if (adx < kSwipeThresholdPx && ady < kSwipeThresholdPx) {
            return;
        }
        if (adx > ady) {
            self->applyMove(dx > 0 ? MoveDir::RIGHT : MoveDir::LEFT);
        } else {
            self->applyMove(dy > 0 ? MoveDir::DOWN : MoveDir::UP);
        }
    }
}

void Game2048App::onRestartClicked(lv_event_t *e)
{
    auto *self = static_cast<Game2048App *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const uint32_t best = self->_engine.getState().best_score;
    self->_engine.reset(best);
    self->_dismissed_win = false;
    self->hideOverlay();
    self->renderBoard(nullptr);
    self->saveProgress();
}

void Game2048App::onContinueClicked(lv_event_t *e)
{
    auto *self = static_cast<Game2048App *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_dismissed_win = true;
    self->hideOverlay();
}

}
