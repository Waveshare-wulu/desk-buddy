#pragma once

#include <cstdint>
#include "apps/db_base_app.hpp"
#include "db_random_engine.hpp"

namespace desk_buddy {

class RandomLabApp : public DeskBuddyApp {
public:
    RandomLabApp();
    ~RandomLabApp() override = default;

protected:
    bool run(void) override;
    bool onUserButtonPressed() override;
    bool onBackButtonPressed() override;
    bool pause(void) override;
    bool resume(void) override;
    bool close(void) override;

private:
    enum class Tab : uint8_t {
        EAT = 0,
        ANSWERS,
        COIN,
        DICE,
        NUMBER,
    };

    void buildUi();
    void updateTopBar();
    void showHub();
    void showTool(Tab tab);
    void buildHub(lv_obj_t *parent);
    void buildTabEat(lv_obj_t *parent);
    void buildTabAnswers(lv_obj_t *parent);
    void buildTabCoin(lv_obj_t *parent);
    void buildTabDice(lv_obj_t *parent);
    void buildTabNumber(lv_obj_t *parent);

    void triggerActiveTool();
    void startEatRoll();
    void startAnswerReveal();
    void startCoinFlip();
    void drawCoinFace(bool heads);
    void reserveCoinTransfer();
    void clearCoinFaces();
    void advanceCoinFlip();
    void finishCoinFlip();
    void startDiceRoll();
    void triggerNumberPick();

    void renderDieFace(int slot, int value);
    void updateEatCategory();
    void updateDiceCount();
    void updateNumberFocus();
    void updateNumberLabels();
    void stopAnimTimer();
    void registerShake();

    static void onBackClicked(lv_event_t *e);
    static void onToolClicked(lv_event_t *e);
    static void onEatCatClicked(lv_event_t *e);
    static void onActionBtnClicked(lv_event_t *e);
    static void onDiceCountClicked(lv_event_t *e);
    static void onNumFocusClicked(lv_event_t *e);
    static void onNumStepEvent(lv_event_t *e);
    static void onNumPresetClicked(lv_event_t *e);
    static void onAnimTimer(lv_timer_t *t);

    RandomEngine _rng;
    bool _in_hub = true;
    Tab _active_tab = Tab::EAT;
    lv_obj_t *_back_btn = nullptr;
    lv_obj_t *_title_lbl = nullptr;
    lv_obj_t *_content_box = nullptr;

    lv_timer_t *_anim_timer = nullptr;
    int _anim_step = 0;
    bool _is_animating = false;

    FoodCategory _food_cat = FoodCategory::ALL;
    int _last_food_idx = -1;
    lv_obj_t *_eat_cat_badge_lbl = nullptr;
    lv_obj_t *_eat_cat_btn_lbl = nullptr;
    lv_obj_t *_eat_tag_lbl = nullptr;
    lv_obj_t *_eat_result_lbl = nullptr;

    int _last_answer_idx = -1;
    lv_obj_t *_answer_lbl = nullptr;

    int _coin_heads = 0;
    int _coin_tails = 0;
    bool _coin_result_heads = true;
    bool _pending_coin_heads = true;
    int _coin_flip_steps = 0;
    lv_obj_t *_coin_disc = nullptr;
    lv_obj_t *_coin_text = nullptr;
    lv_obj_t *_coin_stat_lbl = nullptr;
    lv_obj_t *_coin_image = nullptr;
    lv_draw_buf_t *_coin_faces[2] = {};

    int _dice_count = 2;
    DiceRollResult _pending_dice{};
    lv_obj_t *_dice_count_lbl = nullptr;
    lv_obj_t *_die_cards[3] = {};
    lv_obj_t *_die_pips[3][9] = {};
    lv_obj_t *_dice_total_lbl = nullptr;

    int32_t _num_min = 1;
    int32_t _num_max = 100;
    bool _num_focus_max = true;
    int _num_preset_idx = 1;
    bool _num_step_repeated = false;
    lv_obj_t *_num_focus_cards[2] = {};
    lv_obj_t *_num_bound_lbls[2] = {};
    lv_obj_t *_num_val_lbl = nullptr;
    lv_obj_t *_num_err_lbl = nullptr;
};

}
