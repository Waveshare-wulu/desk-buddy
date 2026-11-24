#include "random_lab_app.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include "esp_random.h"
#include "services/display_service.hpp"
#include "services/motion_service.hpp"
#include "services/sound_service.hpp"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "ui/assets/db_icons.h"
#include "ui/assets/db_ui_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
struct ToolStyle {
    const char *title;
    const char *subtitle;
    uint32_t accent;
    uint32_t soft;
    uint32_t fg;
    const lv_image_dsc_t *icon;
};

constexpr ToolStyle kTools[] = {
    {"今天吃什么", "5类美食随心抽", theme::kAccentOrange, theme::kAccentOrangeSoft, theme::kAccentOrangeFg, &db_icon_rl_eat},
    {"答案之书", "默念问题找启示", theme::kAccentViolet, theme::kAccentVioletSoft, theme::kAccentVioletFg, &db_icon_rl_answers},
    {"抛硬币", "正反面交给天意", theme::kAccentAmber, theme::kAccentAmberSoft, theme::kAccentAmberFg, &db_icon_rl_coin},
    {"摇骰子", "1~3颗点数投掷", theme::kAccentMint, theme::kAccentMintSoft, theme::kAccentMintFg, &db_icon_rl_dice},
    {"随机数", "自定义区间抽取", theme::kAccentCyan, theme::kAccentCyanSoft, theme::kAccentCyanFg, &db_icon_rl_number},
};
constexpr const char *kFoodCatNames[] = {"全部", "中餐", "面粉", "快餐", "小吃"};
constexpr const char *kDicePrompt = "轻点托盘或摇一摇";
constexpr int kSide = theme::kPadScreen;
constexpr int kCardWidth = theme::kContentWidth;
constexpr int kGridWidth = theme::kColumnWidth;
constexpr int kGap = theme::kCardGap;
constexpr int kActionY = 288;
constexpr int kActionHeight = 68;

struct NumberPreset {
    int32_t min;
    int32_t max;
    const char *label;
};

constexpr NumberPreset kNumberPresets[] = {
    {1, 10, "1-10"}, {1, 100, "1-100"}, {1, 50, "1-50"}, {0, 999, "0-999"},
};
constexpr int kNumberSteps[] = {-10, -1, 1, 10};
constexpr int kCoinSize = 168;
constexpr int kCoinFramePeriodMs = 32;
constexpr uint32_t kCoinScales[] = {216, 132, 36, 88, 184, LV_SCALE_NONE};
constexpr int kCoinFramesPerTurn = sizeof(kCoinScales) / sizeof(kCoinScales[0]);
constexpr int kCoinEdgeFrame = 2;

constexpr uint16_t kDiePipMask[7] = {
    0,
    (1 << 4),
    (1 << 2) | (1 << 6),
    (1 << 2) | (1 << 4) | (1 << 6),
    (1 << 0) | (1 << 2) | (1 << 6) | (1 << 8),
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8),
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8),
};

lv_obj_t *makeSurface(lv_obj_t *parent, int width, int height, int radius, uint32_t bg)
{
    auto *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, theme::color(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t fg = theme::kTextPrimary)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, theme::color(fg), 0);
    return label;
}

lv_obj_t *makeCard(lv_obj_t *parent, int x, int y, int width, int height, int radius)
{
    auto *card = theme::createCard(parent, width, height, radius);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_pad_all(card, 0, 0);
    return card;
}

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int width, int height, uint32_t bg, uint32_t fg,
                     const lv_font_t *font, uint32_t border = theme::kBorderSubtle, int border_width = 0)
{
    auto *button = theme::createPillButton(parent, text, bg, fg, width, height, font);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_border_width(button, border_width, 0);
    lv_obj_set_style_border_color(button, theme::color(border), 0);
    return button;
}

void makeAction(lv_obj_t *parent, const char *text, int width, uint32_t accent,
                lv_event_cb_t callback, void *app)
{
    auto *button = makeButton(parent, text, width, kActionHeight, accent, theme::kTextOnAccent, theme::fontLg());
    lv_obj_set_pos(button, kSide + kCardWidth - width, kActionY);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, app);
}

lv_obj_t *makeSelector(lv_obj_t *parent, const char *text, int width, uint32_t accent, uint32_t fg,
                       lv_event_cb_t callback, void *app)
{
    auto *button = makeButton(parent, text, width, kActionHeight, theme::kBgCard, fg, theme::fontMd(), accent, 2);
    lv_obj_set_pos(button, kSide, kActionY);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, app);
    return lv_obj_get_child(button, 0);
}
}

RandomLabApp::RandomLabApp()
    : DeskBuddyApp("deskbuddy.random_lab", i18n::tr(i18n::StrId::APP_RANDOM_LAB), &db_icon_random_lab, 1)
    , _rng([]() -> uint32_t { return esp_random(); })
{
}

bool RandomLabApp::run(void)
{
    buildUi();
    return true;
}

bool RandomLabApp::resume(void)
{
    if (!_in_hub) {
        registerShake();
    }
    if (_coin_image) {
        reserveCoinTransfer();
    }
    return true;
}

bool RandomLabApp::pause(void)
{
    stopAnimTimer();
    if (_coin_image) {
        setDisplayAnimationRegion(nullptr);
    }
    return DeskBuddyApp::pause();
}

bool RandomLabApp::close(void)
{
    stopAnimTimer();
    clearCoinFaces();
    _in_hub = true;
    _content_box = nullptr;
    return DeskBuddyApp::close();
}

void RandomLabApp::registerShake()
{
    MotionService::instance().registerShakeListener(this, [this]() {
        triggerActiveTool();
    });
}

void RandomLabApp::stopAnimTimer()
{
    if (_anim_timer) {
        lv_timer_delete(_anim_timer);
        _anim_timer = nullptr;
    }
    if (_is_animating && _active_tab == Tab::COIN && _coin_image) {
        lv_image_set_src(_coin_image, _coin_faces[_coin_result_heads]);
        lv_image_set_scale_x(_coin_image, LV_SCALE_NONE);
    }
    _is_animating = false;
}

void RandomLabApp::buildUi()
{
    auto *root = appRoot();
    theme::styleRootScreen(root);

    const auto top = theme::createAppTopBar(root, "", theme::kAccentVioletFg, onBackClicked, this);
    _back_btn = top.back_btn;
    _title_lbl = top.title_label;
    auto *shake_hint = theme::createBadge(top.bar, "支持摇一摇", theme::kBgCanvas, theme::kTextMuted);
    lv_obj_clear_flag(shake_hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(shake_hint, LV_ALIGN_RIGHT_MID, 0, 0);

    _content_box = makeSurface(root, 480, 366, 0, theme::kBgCanvas);
    lv_obj_set_y(_content_box, 54);
    showHub();
}

void RandomLabApp::updateTopBar()
{
    const auto &tool = kTools[static_cast<int>(_active_tab)];
    lv_label_set_text_static(lv_obj_get_child(_back_btn, 0), _in_hub ? "‹ 返回" : "‹ 列表");
    lv_label_set_text_static(_title_lbl, _in_hub ? i18n::tr(i18n::StrId::APP_RANDOM_LAB) : tool.title);
}

void RandomLabApp::showHub()
{
    if (!_in_hub) {
        MotionService::instance().unregisterShakeListener(this);
    }
    stopAnimTimer();
    clearCoinFaces();
    _in_hub = true;
    updateTopBar();
    lv_obj_clean(_content_box);
    buildHub(_content_box);
}

void RandomLabApp::showTool(Tab tab)
{
    _in_hub = false;
    _active_tab = tab;
    updateTopBar();
    lv_obj_clean(_content_box);
    switch (tab) {
        case Tab::EAT:     buildTabEat(_content_box); break;
        case Tab::ANSWERS: buildTabAnswers(_content_box); break;
        case Tab::COIN:    buildTabCoin(_content_box); break;
        case Tab::DICE:    buildTabDice(_content_box); break;
        case Tab::NUMBER:  buildTabNumber(_content_box); break;
    }
    registerShake();
}

void RandomLabApp::buildHub(lv_obj_t *parent)
{
    for (int i = 0; i < static_cast<int>(std::size(kTools)); ++i) {
        const auto &tool = kTools[i];
        const auto tab = static_cast<Tab>(i);
        const bool hero = tab == Tab::EAT;
        const int x = hero ? kSide : kSide + ((i - 1) % 2) * (kGridWidth + kGap);
        const int y = hero ? 2 : 120 + ((i - 1) / 2) * (116 + kGap);
        auto *card = makeCard(parent, x, y, hero ? kCardWidth : kGridWidth, hero ? 106 : 116, 24);
        lv_obj_set_style_border_color(card, theme::color(tool.accent), LV_STATE_PRESSED);
        lv_obj_set_user_data(card, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(card, onToolClicked, LV_EVENT_CLICKED, this);

        auto *icon = lv_image_create(card);
        lv_image_set_src(icon, tool.icon);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        if (hero) {
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 17, 0);
        } else {
            lv_obj_set_pos(icon, 17, 11);
        }

        auto *title = makeLabel(card, tool.title, hero ? theme::fontLg() : theme::fontMd());
        auto *subtitle = makeLabel(card, tool.subtitle, theme::fontSm(), theme::kTextSecondary);
        if (hero) {
            lv_obj_set_pos(title, 96, 22);
            lv_obj_set_pos(subtitle, 96, 60);
            lv_label_set_text_fmt(subtitle, "%s · 当前：%s", tool.subtitle, kFoodCatNames[static_cast<int>(_food_cat)]);
        } else {
            lv_obj_set_pos(title, 17, 61);
            lv_obj_set_pos(subtitle, 17, 88);
        }
    }
}

void RandomLabApp::buildTabEat(lv_obj_t *parent)
{
    auto *card = makeCard(parent, kSide, 4, kCardWidth, 270, 26);
    lv_obj_add_event_cb(card, onActionBtnClicked, LV_EVENT_CLICKED, this);

    auto *badge = theme::createBadge(card, "", theme::kAccentOrangeSoft, theme::kAccentOrangeFg);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(badge, 17, 16);
    _eat_cat_badge_lbl = lv_obj_get_child(badge, 0);

    _eat_result_lbl = makeLabel(card, "饿了就抽一个", theme::fontXl());
    lv_obj_set_width(_eat_result_lbl, 396);
    lv_obj_set_style_text_align(_eat_result_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_eat_result_lbl, LV_ALIGN_CENTER, 0, -27);

    _eat_tag_lbl = makeLabel(card, "点击或摇一摇抽取", theme::fontMd(), theme::kAccentAmberFg);
    lv_obj_align(_eat_tag_lbl, LV_ALIGN_BOTTOM_MID, 0, -61);

    _eat_cat_btn_lbl = makeSelector(parent, "", 172, theme::kAccentOrange, theme::kAccentOrangeFg, onEatCatClicked, this);
    updateEatCategory();
    makeAction(parent, "帮我决定", 260, theme::kAccentViolet, onActionBtnClicked, this);
}

void RandomLabApp::updateEatCategory()
{
    const char *category = kFoodCatNames[static_cast<int>(_food_cat)];
    lv_label_set_text_fmt(_eat_cat_badge_lbl, "分类：%s", category);
    lv_label_set_text_fmt(_eat_cat_btn_lbl, "%s · 切换", category);
}

void RandomLabApp::buildTabAnswers(lv_obj_t *parent)
{
    auto *card = makeCard(parent, kSide, 4, kCardWidth, 270, 28);
    lv_obj_set_style_border_color(card, theme::color(0xDDD4FF), 0);
    lv_obj_add_event_cb(card, onActionBtnClicked, LV_EVENT_CLICKED, this);

    auto *hint = makeLabel(card, "心中默念问题，轻点或摇一摇", theme::fontSm(), theme::kAccentAmberFg);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 20);

    _answer_lbl = makeLabel(card, "答案之书正在等你提问", theme::fontLg());
    lv_obj_set_width(_answer_lbl, 396);
    lv_obj_set_style_text_align(_answer_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(_answer_lbl);

    auto *footer = makeLabel(card, "轻点卡片翻页", theme::fontSm(), theme::kTextSecondary);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -16);
    makeAction(parent, "翻开答案之书", kCardWidth, theme::kAccentViolet, onActionBtnClicked, this);
}

void RandomLabApp::buildTabCoin(lv_obj_t *parent)
{
    auto *card = makeCard(parent, kSide, 4, kCardWidth, 224, 26);
    lv_obj_add_event_cb(card, onActionBtnClicked, LV_EVENT_CLICKED, this);

    auto *stage = makeSurface(card, kCoinSize, kCoinSize, 0, theme::kBgCard);
    lv_obj_center(stage);

    _coin_disc = makeSurface(stage, kCoinSize, kCoinSize, LV_RADIUS_CIRCLE, theme::kBgCard);
    lv_obj_set_style_border_width(_coin_disc, 4, 0);
    lv_obj_set_style_border_opa(_coin_disc, LV_OPA_COVER, 0);

    _coin_text = makeLabel(_coin_disc, "", theme::fontXl());
    lv_obj_center(_coin_text);

    drawCoinFace(false);
    _coin_faces[false] = lv_snapshot_take(stage, LV_COLOR_FORMAT_RGB565);
    drawCoinFace(true);
    _coin_faces[true] = lv_snapshot_take(stage, LV_COLOR_FORMAT_RGB565);
    if (_coin_faces[false] && _coin_faces[true]) {
        lv_obj_add_flag(_coin_disc, LV_OBJ_FLAG_HIDDEN);
        _coin_image = lv_image_create(stage);
        lv_obj_clear_flag(_coin_image, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(_coin_image, _coin_faces[_coin_result_heads]);
        lv_obj_center(_coin_image);
        reserveCoinTransfer();
    } else {
        clearCoinFaces();
        drawCoinFace(_coin_result_heads);
    }

    _coin_stat_lbl = makeLabel(parent, "", theme::fontMd(), theme::kTextSecondary);
    lv_label_set_text_fmt(_coin_stat_lbl, "正面 %d 次 · 反面 %d 次", _coin_heads, _coin_tails);
    lv_obj_align(_coin_stat_lbl, LV_ALIGN_TOP_MID, 0, 240);
    makeAction(parent, "抛硬币", kCardWidth, theme::kAccentAmber, onActionBtnClicked, this);
}

void RandomLabApp::drawCoinFace(bool heads)
{
    lv_label_set_text_static(_coin_text, heads ? "正" : "反");
    lv_obj_set_style_text_color(_coin_text, theme::color(heads ? 0x663906 : 0x104E78), 0);
    lv_obj_set_style_bg_color(_coin_disc, theme::color(heads ? 0xF5C873 : 0x9BBFE4), 0);
    lv_obj_set_style_border_color(_coin_disc, theme::color(heads ? 0xDBA343 : 0x68A1D1), 0);
}

void RandomLabApp::reserveCoinTransfer()
{
    auto *stage = lv_obj_get_parent(_coin_image);
    lv_obj_update_layout(stage);
    lv_area_t area{};
    lv_obj_get_coords(stage, &area);
    setDisplayAnimationRegion(&area);
}

void RandomLabApp::clearCoinFaces()
{
    if (_coin_image) {
        setDisplayAnimationRegion(nullptr);
        lv_obj_delete(_coin_image);
        _coin_image = nullptr;
    }
    for (auto *&face : _coin_faces) {
        if (face) {
            lv_image_cache_drop(face);
            lv_draw_buf_destroy(face);
            face = nullptr;
        }
    }
}

void RandomLabApp::buildTabDice(lv_obj_t *parent)
{
    auto *tray = makeCard(parent, kSide, 4, kCardWidth, 214, 26);
    lv_obj_set_style_bg_color(tray, theme::color(theme::kBgCardElevated), 0);
    lv_obj_set_flex_flow(tray, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tray, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(tray, onActionBtnClicked, LV_EVENT_CLICKED, this);

    for (int i = 0; i < 3; ++i) {
        auto *die = theme::createCard(tray, 116, 116, 22);
        lv_obj_set_style_border_width(die, 2, 0);
        lv_obj_set_style_pad_all(die, 15, 0);
        lv_obj_clear_flag(die, LV_OBJ_FLAG_CLICKABLE);
        _die_cards[i] = die;

        constexpr int kOffsets[] = {4, 32, 60};
        for (int pip = 0; pip < 9; ++pip) {
            auto *dot = makeSurface(die, 18, 18, LV_RADIUS_CIRCLE, theme::kTextPrimary);
            lv_obj_set_pos(dot, kOffsets[pip % 3], kOffsets[pip / 3]);
            _die_pips[i][pip] = dot;
        }
        renderDieFace(i, i + 1);
    }

    _dice_total_lbl = makeLabel(parent, kDicePrompt, theme::fontMd(), theme::kAccentMintFg);
    lv_obj_align(_dice_total_lbl, LV_ALIGN_TOP_MID, 0, 232);
    _dice_count_lbl = makeSelector(parent, "", 172, theme::kAccentViolet, theme::kAccentVioletFg, onDiceCountClicked, this);
    updateDiceCount();
    makeAction(parent, "摇骰子", 260, theme::kAccentViolet, onActionBtnClicked, this);
}

void RandomLabApp::updateDiceCount()
{
    lv_label_set_text_fmt(_dice_count_lbl, "%d 颗骰子", _dice_count);
    lv_label_set_text_static(_dice_total_lbl, kDicePrompt);
    for (int i = 0; i < 3; ++i) {
        if (i < _dice_count) {
            lv_obj_clear_flag(_die_cards[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_die_cards[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void RandomLabApp::buildTabNumber(lv_obj_t *parent)
{
    auto *card = makeCard(parent, kSide, 4, kCardWidth, 124, 24);
    lv_obj_add_event_cb(card, onActionBtnClicked, LV_EVENT_CLICKED, this);

    _num_val_lbl = makeLabel(card, "-", theme::fontHero(), theme::kAccentCyanFg);
    lv_obj_align(_num_val_lbl, LV_ALIGN_CENTER, 0, -12);
    _num_err_lbl = makeLabel(card, "", theme::fontSm(), theme::kTextSecondary);
    lv_obj_align(_num_err_lbl, LV_ALIGN_BOTTOM_MID, 0, -12);

    for (int i = 0; i < 2; ++i) {
        auto *focus = makeCard(parent, kSide + i * (kGridWidth + kGap), 138, kGridWidth, 68, 20);
        lv_obj_set_style_border_width(focus, 2, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(focus, theme::color(theme::kAccentViolet), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(focus, theme::color(theme::kAccentVioletSoft), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(focus, theme::color(theme::kAccentVioletFg), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(focus, theme::color(theme::kTextSecondary), 0);
        lv_obj_set_user_data(focus, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(focus, onNumFocusClicked, LV_EVENT_CLICKED, this);
        _num_focus_cards[i] = focus;

        auto *caption = lv_label_create(focus);
        lv_label_set_text_static(caption, i == 0 ? "最小值" : "最大值");
        lv_obj_set_style_text_font(caption, theme::fontMd(), 0);
        lv_obj_align(caption, LV_ALIGN_LEFT_MID, 17, 0);

        _num_bound_lbls[i] = makeLabel(focus, "", theme::fontLg());
        lv_obj_align(_num_bound_lbls[i], LV_ALIGN_RIGHT_MID, -17, 0);
    }
    updateNumberFocus();

    constexpr const char *kStepLabels[] = {"-10", "-1", "+1", "+10"};
    for (int i = 0; i < static_cast<int>(std::size(kNumberSteps)); ++i) {
        auto *step = makeButton(parent, kStepLabels[i], 103, 58, theme::kBgCardElevated, theme::kTextPrimary,
                                theme::fontMd(), theme::kBorderSubtle, 1);
        lv_obj_set_style_radius(step, 18, 0);
        lv_obj_set_pos(step, kSide + 1 + i * 113, 216);
        lv_obj_set_user_data(step, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(step, onNumStepEvent, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(step, onNumStepEvent, LV_EVENT_LONG_PRESSED_REPEAT, this);
        lv_obj_add_event_cb(step, onNumStepEvent, LV_EVENT_CLICKED, this);
    }

    makeSelector(parent, kNumberPresets[_num_preset_idx].label, 160, theme::kAccentCyan, theme::kAccentCyanFg,
                 onNumPresetClicked, this);
    makeAction(parent, "生成随机数", 272, theme::kAccentViolet, onActionBtnClicked, this);
    updateNumberLabels();
}

void RandomLabApp::updateNumberFocus()
{
    lv_obj_remove_state(_num_focus_cards[!_num_focus_max], LV_STATE_CHECKED);
    lv_obj_add_state(_num_focus_cards[_num_focus_max], LV_STATE_CHECKED);
}

void RandomLabApp::renderDieFace(int slot, int value)
{
    if (slot < 0 || slot >= 3) return;
    value = std::clamp(value, 1, 6);
    const uint16_t mask = kDiePipMask[value];
    const uint32_t pip_hex = (value == 1 || value == 4) ? 0xEF4444 : 0x1E293B;
    for (int p = 0; p < 9; ++p) {
        if (mask & (1u << p)) {
            lv_obj_clear_flag(_die_pips[slot][p], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(_die_pips[slot][p], theme::color(pip_hex), 0);
        } else {
            lv_obj_add_flag(_die_pips[slot][p], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void RandomLabApp::updateNumberLabels()
{
    lv_label_set_text_fmt(_num_bound_lbls[0], "%ld", static_cast<long>(_num_min));
    lv_label_set_text_fmt(_num_bound_lbls[1], "%ld", static_cast<long>(_num_max));

    const bool invalid = _num_min > _num_max;
    lv_label_set_text_static(_num_val_lbl, invalid ? "!" : "-");
    lv_obj_set_style_text_color(_num_val_lbl, theme::color(invalid ? theme::kAccentCoralFg : theme::kAccentCyanFg), 0);
    lv_obj_set_style_text_color(_num_err_lbl, theme::color(invalid ? theme::kAccentCoralFg : theme::kTextSecondary), 0);
    if (invalid) {
        lv_label_set_text_static(_num_err_lbl, "最小值不能大于最大值");
    } else {
        lv_label_set_text_fmt(_num_err_lbl, "范围 [%ld, %ld] · 点此抽取",
                              static_cast<long>(_num_min), static_cast<long>(_num_max));
    }
}

bool RandomLabApp::onUserButtonPressed()
{
    if (_in_hub) {
        return false;
    }
    triggerActiveTool();
    return true;
}

bool RandomLabApp::onBackButtonPressed()
{
    if (_in_hub) {
        return false;
    }
    showHub();
    return true;
}

void RandomLabApp::triggerActiveTool()
{
    if (_in_hub || _is_animating) {
        return;
    }
    switch (_active_tab) {
        case Tab::EAT:     startEatRoll(); break;
        case Tab::ANSWERS: startAnswerReveal(); break;
        case Tab::COIN:    startCoinFlip(); break;
        case Tab::DICE:    startDiceRoll(); break;
        case Tab::NUMBER:  triggerNumberPick(); break;
    }
}

void RandomLabApp::startEatRoll()
{
    SoundService::instance().play(SoundEffect::CLICK);
    _is_animating = true;
    _anim_step = 0;
    _anim_timer = lv_timer_create(onAnimTimer, 55, this);
}

void RandomLabApp::startAnswerReveal()
{
    SoundService::instance().play(SoundEffect::CLICK);
    _is_animating = true;
    _anim_step = 0;
    if (_answer_lbl) {
        lv_obj_set_style_text_opa(_answer_lbl, LV_OPA_30, 0);
        lv_label_set_text(_answer_lbl, "正在感应答案...");
    }
    _anim_timer = lv_timer_create(onAnimTimer, 70, this);
}

void RandomLabApp::startCoinFlip()
{
    SoundService::instance().play(SoundEffect::COIN);
    _pending_coin_heads = _rng.flipCoin().is_heads;
    if (!_coin_image) {
        finishCoinFlip();
        return;
    }
    _is_animating = true;
    _anim_step = 0;
    _coin_flip_steps = (_coin_result_heads == _pending_coin_heads ? 2 : 3) * kCoinFramesPerTurn;
    _anim_timer = lv_timer_create(onAnimTimer, kCoinFramePeriodMs, this);
    if (!_anim_timer) {
        finishCoinFlip();
    }
}

void RandomLabApp::advanceCoinFlip()
{
    if (_anim_step >= _coin_flip_steps) {
        finishCoinFlip();
        return;
    }

    const int frame = _anim_step - 1;
    const int phase = frame % kCoinFramesPerTurn;
    const int turns = frame / kCoinFramesPerTurn + (phase >= kCoinEdgeFrame);
    const bool heads = turns % 2 == 0 ? _coin_result_heads : !_coin_result_heads;
    if (lv_image_get_src(_coin_image) != _coin_faces[heads]) {
        lv_image_set_src(_coin_image, _coin_faces[heads]);
    }
    lv_image_set_scale_x(_coin_image, kCoinScales[phase]);
}

void RandomLabApp::finishCoinFlip()
{
    _coin_result_heads = _pending_coin_heads;
    if (_coin_result_heads) ++_coin_heads;
    else ++_coin_tails;
    if (!_coin_image) {
        drawCoinFace(_coin_result_heads);
    }
    lv_label_set_text_fmt(_coin_stat_lbl, "结果：%s · 正 %d / 反 %d",
                          _coin_result_heads ? "正面" : "反面", _coin_heads, _coin_tails);
    stopAnimTimer();
}

void RandomLabApp::startDiceRoll()
{
    SoundService::instance().play(SoundEffect::DICE);
    _is_animating = true;
    _anim_step = 0;
    _pending_dice = _rng.rollDice(_dice_count);
    if (_dice_total_lbl) {
        lv_label_set_text(_dice_total_lbl, "骰子滚动中...");
    }
    _anim_timer = lv_timer_create(onAnimTimer, 60, this);
}

void RandomLabApp::triggerNumberPick()
{
    const auto res = _rng.pickNumberInRange(_num_min, _num_max);
    if (!res.valid) {
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    lv_label_set_text_fmt(_num_val_lbl, "%ld", static_cast<long>(res.value));
}

void RandomLabApp::onAnimTimer(lv_timer_t *t)
{
    auto *self = static_cast<RandomLabApp *>(lv_timer_get_user_data(t));
    if (!self) return;

    ++self->_anim_step;

    switch (self->_active_tab) {
        case Tab::EAT: {
            const auto &item = self->_rng.pickFood(self->_food_cat, &self->_last_food_idx);
            if (self->_eat_result_lbl) {
                lv_label_set_text(self->_eat_result_lbl, item.name);
            }
            if (self->_eat_tag_lbl) {
                lv_label_set_text(self->_eat_tag_lbl, item.tag);
            }
            if (self->_anim_step >= 10) {
                SoundService::instance().play(SoundEffect::COMPLETE);
                self->stopAnimTimer();
            }
            break;
        }
        case Tab::ANSWERS: {
            if (self->_anim_step >= 5) {
                const char *ans = self->_rng.pickAnswer(&self->_last_answer_idx);
                if (self->_answer_lbl) {
                    lv_label_set_text(self->_answer_lbl, ans);
                    lv_obj_set_style_text_opa(self->_answer_lbl, LV_OPA_COVER, 0);
                }
                SoundService::instance().play(SoundEffect::COMPLETE);
                self->stopAnimTimer();
            }
            break;
        }
        case Tab::COIN:
            self->advanceCoinFlip();
            break;
        case Tab::DICE: {
            if (self->_anim_step < 9) {
                for (int i = 0; i < self->_dice_count; ++i) {
                    self->renderDieFace(i, self->_rng.uniformInt(1, 6));
                }
            } else {
                for (int i = 0; i < self->_pending_dice.count; ++i) {
                    self->renderDieFace(i, self->_pending_dice.values[i]);
                }
                if (self->_dice_total_lbl) {
                    char buf[64];
                    if (self->_pending_dice.count == 1) {
                        std::snprintf(buf, sizeof(buf), "点数：%d", self->_pending_dice.total);
                    } else if (self->_pending_dice.count == 2) {
                        std::snprintf(buf, sizeof(buf), "%d + %d = 总点数 %d",
                                      self->_pending_dice.values[0], self->_pending_dice.values[1],
                                      self->_pending_dice.total);
                    } else {
                        std::snprintf(buf, sizeof(buf), "%d + %d + %d = 总点数 %d",
                                      self->_pending_dice.values[0], self->_pending_dice.values[1],
                                      self->_pending_dice.values[2], self->_pending_dice.total);
                    }
                    lv_label_set_text(self->_dice_total_lbl, buf);
                }
                self->stopAnimTimer();
            }
            break;
        }
        default:
            self->stopAnimTimer();
            break;
    }
}

void RandomLabApp::onBackClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    if (self->onBackButtonPressed()) {
        SoundService::instance().play(SoundEffect::CLICK);
    } else {
        DeskBuddyApp::onBackBtnClicked(e);
    }
}

void RandomLabApp::onToolClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    auto *card = lv_event_get_current_target_obj(e);
    const auto tab = static_cast<Tab>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(card)));
    SoundService::instance().play(SoundEffect::CLICK);
    self->showTool(tab);
}

void RandomLabApp::onEatCatClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    if (self->_is_animating) {
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    const auto next = (static_cast<size_t>(self->_food_cat) + 1) % std::size(kFoodCatNames);
    self->_food_cat = static_cast<FoodCategory>(next);
    self->_last_food_idx = -1;
    self->updateEatCategory();
    lv_label_set_text_static(self->_eat_result_lbl, "饿了就抽一个");
    lv_label_set_text_static(self->_eat_tag_lbl, "点击或摇一摇抽取");
}

void RandomLabApp::onActionBtnClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    self->triggerActiveTool();
}

void RandomLabApp::onDiceCountClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    if (self->_is_animating) {
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    self->_dice_count = self->_dice_count % 3 + 1;
    self->updateDiceCount();
}

void RandomLabApp::onNumFocusClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    auto *card = lv_event_get_current_target_obj(e);
    self->_num_focus_max = lv_obj_get_user_data(card) != nullptr;
    self->updateNumberFocus();
    SoundService::instance().play(SoundEffect::CLICK);
}

void RandomLabApp::onNumStepEvent(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    const auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        self->_num_step_repeated = false;
        return;
    }
    if (code == LV_EVENT_CLICKED && self->_num_step_repeated) {
        return;
    }

    auto *button = lv_event_get_current_target_obj(e);
    const auto step = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button));
    auto &bound = self->_num_focus_max ? self->_num_max : self->_num_min;
    bound = std::clamp<int32_t>(bound + kNumberSteps[step], -99999, 99999);
    self->updateNumberLabels();
    if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
        self->_num_step_repeated = true;
    } else {
        SoundService::instance().play(SoundEffect::CLICK);
    }
}

void RandomLabApp::onNumPresetClicked(lv_event_t *e)
{
    auto *self = static_cast<RandomLabApp *>(lv_event_get_user_data(e));
    self->_num_preset_idx = (self->_num_preset_idx + 1) % std::size(kNumberPresets);
    const auto &preset = kNumberPresets[self->_num_preset_idx];
    self->_num_min = preset.min;
    self->_num_max = preset.max;
    lv_label_set_text_static(lv_obj_get_child(lv_event_get_current_target_obj(e), 0), preset.label);
    self->updateNumberLabels();
    SoundService::instance().play(SoundEffect::CLICK);
}

}
