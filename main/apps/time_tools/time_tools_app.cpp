#include "time_tools_app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>
#include "esp_timer.h"
#include "services/clock_service.hpp"
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "ui/assets/db_icons.h"
#include "ui/assets/db_ui_icons.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
using namespace theme;

constexpr const char *kPageTitles[] = {"下班倒计时", "番茄钟", "数字时钟"};

constexpr struct {
    const char *hint;
    const char *action;
    const char *badge;
} kPomodoroText[] = {
    {"轻点圆环开始", "开始专注", "就绪"},
    {"专注计时中", "暂停专注", "计时中"},
    {"已暂停", "继续", "已暂停"},
    {"本轮完成", "再来一轮", "已完成"},
};

constexpr int kSelectEndpoint = 0;
constexpr int kQuickMinute = 2;
constexpr int kPresetShift = 6;
constexpr struct {
    const char *label;
    int start;
    int end;
} kShifts[] = {
    {"8:30-17:30", 510, 1050}, {"9:00-18:00", 540, 1080},
    {"9:30-18:30", 570, 1110}, {"10:00-19:00", 600, 1140}
};

lv_obj_t *addLabel(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font,
                   uint32_t foreground = theme::kTextPrimary)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, theme::color(foreground), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

void setTextIfChanged(lv_obj_t *label, const char *text)
{
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

lv_obj_t *addBadge(lv_obj_t *parent, const char *text, uint32_t bg, uint32_t fg)
{
    auto *badge = createBadge(parent, text, bg, fg);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    return badge;
}

void updateBadge(lv_obj_t *badge, const char *text, uint32_t bg, uint32_t fg)
{
    auto *label = lv_obj_get_child(badge, 0);
    setTextIfChanged(label, text);
    if (!lv_color_eq(lv_obj_get_style_bg_color(badge, LV_PART_MAIN), color(bg))) {
        lv_obj_set_style_bg_color(badge, color(bg), 0);
        lv_obj_set_style_text_color(label, color(fg), 0);
    }
}

void addCardAction(lv_obj_t *card, lv_event_cb_t handler, void *app, unsigned index, uint32_t accent)
{
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, color(kBgCanvas), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(card, color(accent), LV_STATE_PRESSED);
    lv_obj_set_user_data(card, reinterpret_cast<void *>(static_cast<uintptr_t>(index)));
    lv_obj_add_event_cb(card, handler, LV_EVENT_CLICKED, app);
}

lv_obj_t *addProgressBar(lv_obj_t *parent, int height, int bottom_offset)
{
    auto *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), height);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, bottom_offset);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, color(kBgTrack), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color(kAccentMint), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, height / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, height / 2, LV_PART_INDICATOR);
    return bar;
}

uint64_t nowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}
}

TimeToolsApp::TimeToolsApp(std::function<void()> on_shift_changed,
                           std::function<void()> on_open_time_calibration)
    : DeskBuddyApp("deskbuddy.time_tools", i18n::tr(i18n::StrId::APP_TIME_TOOLS), &db_icon_time_tools, 1)
    , _on_shift_changed(std::move(on_shift_changed))
    , _on_open_time_calibration(std::move(on_open_time_calibration))
    , _pomo(25 * 60)
{
}

void TimeToolsApp::setRestoreViewAfterCalibration(bool restore)
{
    _restore_view_after_calibration.store(restore);
}

bool TimeToolsApp::run(void)
{
    if (!_pomo_loaded) {
        _pomo.setDurationMinutes(StorageService::instance().getSettings().pomodoro_min);
        _pomo_loaded = true;
    }
    buildUi();
    if (_restore_view_after_calibration.exchange(false) && !_in_hub) {
        showPage(_page);
    } else {
        showHub();
    }
    _refresh_timer = lv_timer_create(onRefreshTimer, 250, this);
    return true;
}

bool TimeToolsApp::resume(void)
{
    if (!_refresh_timer) {
        _refresh_timer = lv_timer_create(onRefreshTimer, 250, this);
    }
    refreshView();
    return true;
}

bool TimeToolsApp::pause(void)
{
    stopRefreshTimer();
    return DeskBuddyApp::pause();
}

bool TimeToolsApp::close(void)
{
    stopRefreshTimer();
    _content_box = nullptr;
    _shift_editor = nullptr;
    return DeskBuddyApp::close();
}

void TimeToolsApp::stopRefreshTimer()
{
    if (_refresh_timer) {
        lv_timer_delete(_refresh_timer);
        _refresh_timer = nullptr;
    }
}

void TimeToolsApp::buildUi()
{
    auto *root = appRoot();
    styleRootScreen(root);
    const auto top = createAppTopBar(root, "", kAccentCyanFg, onBackClicked, this);
    _back_label = lv_obj_get_child(top.back_btn, 0);
    _title_label = top.title_label;
    _rtc_badge = createBadge(top.bar, "待校时 ›", kAccentAmberSoft, kAccentAmberFg);
    lv_obj_align(_rtc_badge, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(_rtc_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(_rtc_badge, lv_color_darken(color(kAccentAmberSoft), 22), LV_STATE_PRESSED);
    lv_obj_add_event_cb(_rtc_badge, onTimeCalibrationClicked, LV_EVENT_CLICKED, this);
    lv_obj_update_layout(_rtc_badge);
    expandTouchHeight(_rtc_badge, lv_obj_get_height(_rtc_badge));

    _content_box = lv_obj_create(root);
    lv_obj_remove_style_all(_content_box);
    lv_obj_set_size(_content_box, LV_PCT(100), 366);
    lv_obj_set_pos(_content_box, 0, kTopBarHeight + 2);
    lv_obj_clear_flag(_content_box, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
}

void TimeToolsApp::clearContent()
{
    lv_obj_clean(_content_box);
    _clock_view = {};
    _pomo_view = {};
    _offwork_view = {};
}

void TimeToolsApp::showHub()
{
    _in_hub = true;
    clearContent();
    lv_label_set_text_static(_back_label, "‹ 返回");
    lv_label_set_text(_title_label, i18n::tr(i18n::StrId::APP_TIME_TOOLS));
    buildHub();
    refreshView();
}

void TimeToolsApp::showPage(Page page)
{
    _in_hub = false;
    _page = page;
    clearContent();
    lv_label_set_text_static(_back_label, "‹ 列表");
    lv_label_set_text(_title_label, kPageTitles[static_cast<unsigned>(page)]);
    switch (page) {
        case Page::OFFWORK:  buildOffwork(); break;
        case Page::POMODORO: buildPomodoro(); break;
        case Page::CLOCK:    buildClock(); break;
    }
    refreshView();
}

void TimeToolsApp::buildHub()
{
    auto *offwork = createCard(_content_box, kContentWidth, 134);
    lv_obj_set_pos(offwork, kPadScreen, 4);
    lv_obj_set_style_pad_all(offwork, 16, 0);
    addCardAction(offwork, onHubCardClicked, this, static_cast<unsigned>(Page::OFFWORK), kAccentMint);

    auto *sunset = lv_image_create(offwork);
    lv_image_set_src(sunset, &db_icon_tt_offwork);
    lv_obj_set_pos(sunset, 0, 14);
    addLabel(offwork, kPageTitles[0], 60, 10, fontMd());
    _offwork_view.status = addLabel(offwork, "", 60, 43, fontSm(), kTextSecondary);
    lv_obj_set_size(_offwork_view.status, 200, lv_font_get_line_height(fontSm()));
    lv_obj_set_style_text_letter_space(_offwork_view.status, -1, 0);
    lv_label_set_long_mode(_offwork_view.status, LV_LABEL_LONG_MODE_DOTS);
    _offwork_view.time = addLabel(offwork, "", 0, 0, fontLg(), kAccentMintFg);
    lv_obj_align(_offwork_view.time, LV_ALIGN_TOP_RIGHT, 0, -2);
    _offwork_view.badge = addBadge(offwork, "", kAccentMintSoft, kAccentMintFg);
    lv_obj_align(_offwork_view.badge, LV_ALIGN_TOP_RIGHT, 0, 36);
    _offwork_view.bar = addProgressBar(offwork, 12, 0);

    auto *pomo = createCard(_content_box, kColumnWidth, 212);
    lv_obj_set_pos(pomo, kPadScreen, 150);
    lv_obj_set_style_pad_all(pomo, 17, 0);
    addCardAction(pomo, onHubCardClicked, this, static_cast<unsigned>(Page::POMODORO), kAccentCoral);
    lv_image_set_src(lv_image_create(pomo), &db_icon_tt_pomo);
    _pomo_view.badge = addBadge(pomo, "", kAccentCoralSoft, kAccentCoralFg);
    lv_obj_align(_pomo_view.badge, LV_ALIGN_TOP_RIGHT, 0, 8);
    addLabel(pomo, kPageTitles[1], 0, 73, fontMd(), kTextSecondary);
    _pomo_view.time = addLabel(pomo, "", 0, 101, fontXl());
    _pomo_view.completed = addLabel(pomo, "", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(_pomo_view.completed, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    auto *clock = createCard(_content_box, kColumnWidth, 212);
    lv_obj_set_pos(clock, kPadScreen + kColumnWidth + kCardGap, 150);
    lv_obj_set_style_pad_all(clock, 17, 0);
    addCardAction(clock, onHubCardClicked, this, static_cast<unsigned>(Page::CLOCK), kAccentCyan);
    lv_image_set_src(lv_image_create(clock), &db_icon_tt_clock);
    addLabel(clock, kPageTitles[2], 0, 73, fontMd(), kTextSecondary);
    _clock_view.time = addLabel(clock, "", 0, 101, fontXl());
    _clock_view.date = addLabel(clock, "", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(_clock_view.date, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void TimeToolsApp::buildClock()
{
    auto *card = createCard(_content_box, kContentWidth, 348, 28);
    lv_obj_set_pos(card, kPadScreen, 8);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_border_color(card, color(0xF0DFC6), 0);

    _clock_view.badge = addBadge(card, "", kAccentAmberSoft, kAccentAmberFg);
    lv_obj_align(_clock_view.badge, LV_ALIGN_TOP_MID, 0, 0);
    _clock_view.time = addLabel(card, "", 0, 0, fontHero());
    lv_obj_align(_clock_view.time, LV_ALIGN_CENTER, 0, -4);
    _clock_view.date = addLabel(card, "", 0, 0, fontMd(), kAccentCyanFg);
    lv_obj_align(_clock_view.date, LV_ALIGN_BOTTOM_MID, 0, -34);
    auto *hint = addLabel(card, "可在「系统设置 → 时间与校时」校准时间", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void TimeToolsApp::buildPomodoro()
{
    auto *dial = createCard(_content_box, 232, 232, LV_RADIUS_CIRCLE);
    lv_obj_align(dial, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_pad_all(dial, 0, 0);
    lv_obj_set_style_border_width(dial, 0, 0);
    lv_obj_set_style_bg_color(dial, color(kAccentCoralSoft), LV_STATE_PRESSED);
    lv_obj_add_flag(dial, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST));
    lv_obj_add_event_cb(dial, onPomoStartPause, LV_EVENT_CLICKED, this);

    _pomo_view.arc = lv_arc_create(dial);
    lv_obj_remove_style_all(_pomo_view.arc);
    lv_obj_set_size(_pomo_view.arc, LV_PCT(100), LV_PCT(100));
    lv_obj_center(_pomo_view.arc);
    lv_obj_clear_flag(_pomo_view.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(_pomo_view.arc, 270);
    lv_arc_set_bg_angles(_pomo_view.arc, 0, 360);
    lv_arc_set_range(_pomo_view.arc, 0, 100);
    lv_obj_set_style_arc_width(_pomo_view.arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_pomo_view.arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(_pomo_view.arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_pomo_view.arc, color(kBgTrack), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_pomo_view.arc, color(kAccentCoral), LV_PART_INDICATOR);
    _pomo_view.time = addLabel(dial, "", 0, 0, fontXl());
    lv_obj_align(_pomo_view.time, LV_ALIGN_CENTER, 0, -11);
    _pomo_view.state = addLabel(dial, "", 0, 0, fontSm(), kAccentAmberFg);
    lv_obj_align(_pomo_view.state, LV_ALIGN_CENTER, 0, 26);

    for (int i = 0; i < 2; ++i) {
        auto *step = createPillButton(_content_box, i ? "+5" : "-5", kBgCard, kTextPrimary, 80, 80, fontLg());
        lv_obj_align(step, i ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT, i ? -kPadScreen : kPadScreen, 84);
        lv_obj_set_style_radius(step, 22, 0);
        lv_obj_set_style_border_width(step, 1, 0);
        lv_obj_set_style_border_color(step, color(kBorderSubtle), 0);
        lv_obj_set_style_opa(step, LV_OPA_40, LV_STATE_DISABLED);
        lv_obj_align(lv_obj_get_child(step, 0), LV_ALIGN_CENTER, 0, -10);
        lv_obj_align(addLabel(step, "分钟", 0, 0, fontSm(), kTextSecondary), LV_ALIGN_CENTER, 0, 17);
        lv_obj_set_user_data(step, reinterpret_cast<void *>(static_cast<intptr_t>(i ? 5 : -5)));
        lv_obj_add_event_cb(step, onPomoStepMin, LV_EVENT_CLICKED, this);
        _pomo_view.steps[i] = step;
    }

    _pomo_view.completed = addLabel(_content_box, "", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(_pomo_view.completed, LV_ALIGN_TOP_MID, 0, 256);

    auto *reset = createPillButton(_content_box, "重置", kBgCard, kTextSecondary, 152, 64);
    lv_obj_set_pos(reset, kPadScreen, 294);
    lv_obj_set_style_radius(reset, 22, 0);
    lv_obj_set_style_border_width(reset, 1, 0);
    lv_obj_set_style_border_color(reset, color(kBorderSubtle), 0);
    lv_obj_add_event_cb(reset, onPomoReset, LV_EVENT_CLICKED, this);

    auto *primary = createPillButton(_content_box, "", kAccentCoral, kTextOnAccent, 280, 64, fontLg());
    lv_obj_align(primary, LV_ALIGN_TOP_RIGHT, -kPadScreen, 294);
    lv_obj_set_style_radius(primary, 22, 0);
    lv_obj_add_event_cb(primary, onPomoStartPause, LV_EVENT_CLICKED, this);
    _pomo_view.primary_label = lv_obj_get_child(primary, 0);
}

void TimeToolsApp::buildOffwork()
{
    auto *card = createCard(_content_box, kContentWidth, 234, 26);
    lv_obj_set_pos(card, kPadScreen, 6);
    lv_obj_set_style_pad_all(card, 20, 0);
    _offwork_view.status = addLabel(card, "", 0, 3, fontSm(), kAccentMintFg);
    _offwork_view.badge = addBadge(card, "", kAccentMintSoft, kAccentMintFg);
    lv_obj_align(_offwork_view.badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    _offwork_view.time = addLabel(card, "", 0, 0, fontHero());
    lv_obj_align(_offwork_view.time, LV_ALIGN_CENTER, 0, -5);
    _offwork_view.bar = addProgressBar(card, 18, -32);

    constexpr lv_align_t scale_align[] = {LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_RIGHT};
    for (unsigned i = 0; i < 3; ++i) {
        _offwork_view.scale[i] = addLabel(card, "", 0, 0, fontSm(), kTextSecondary);
        lv_obj_align(_offwork_view.scale[i], scale_align[i], 0, 0);
    }

    for (unsigned i = 0; i < 2; ++i) {
        const auto fg = i == 0 ? kAccentAmberFg : kAccentMintFg;
        auto *entry = createCard(_content_box, kColumnWidth, 108, 22);
        lv_obj_set_pos(entry, kPadScreen + i * (kColumnWidth + kCardGap), 252);
        lv_obj_set_style_pad_hor(entry, 17, 0);
        lv_obj_set_style_border_color(entry, color(i == 0 ? 0xF2DAB4 : 0xBCE7DA), 0);
        addLabel(entry, i == 0 ? "上班时间" : "下班时间", 0, 0, fontSm(), fg);
        auto *hint = addLabel(entry, "修改", 0, 0, fontSm(), fg);
        lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, 0, 0);
        _offwork_view.endpoints[i] = addLabel(entry, "", 0, 0, fontXl());
        lv_obj_align(_offwork_view.endpoints[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
        addCardAction(entry, onOpenShiftEditor, this, i, i == 0 ? kAccentAmber : kAccentMint);
    }
}

void TimeToolsApp::showShiftEditor(unsigned endpoint)
{
    using namespace theme;
    const auto &cfg = StorageService::instance().getSettings();
    _draft_minutes[0] = cfg.work_start_hour * 60 + cfg.work_start_min;
    _draft_minutes[1] = cfg.offwork_hour * 60 + cfg.offwork_min;
    _shift_endpoint = endpoint;

    _shift_editor = lv_obj_create(appRoot());
    lv_obj_remove_style_all(_shift_editor);
    lv_obj_set_size(_shift_editor, LV_PCT(100), 420);
    styleRootScreen(_shift_editor);
    lv_obj_set_style_radius(_shift_editor, 0, 0);
    const auto top = createAppTopBar(_shift_editor, "设置上下班", kAccentMintFg,
        onCancelShift, this, "保存", onSaveShift);
    lv_label_set_text(lv_obj_get_child(top.back_btn, 0), "‹ 取消");
    _shift_save = top.action_btn;

    auto addButton = [this](lv_obj_t *parent, const char *label, int w, int h,
                            const lv_font_t *font, uint32_t bg, int action) {
        auto *btn = createPillButton(parent, label, bg, kTextPrimary, w, h, font);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, color(kBorderSubtle), 0);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(action)));
        lv_obj_add_event_cb(btn, onShiftEdit, LV_EVENT_CLICKED, this);
        return btn;
    };
    auto *tabs = lv_obj_create(_shift_editor);
    lv_obj_remove_style_all(tabs);
    lv_obj_clear_flag(tabs, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(tabs, kContentWidth, 50);
    lv_obj_set_pos(tabs, kPadScreen, 56);
    for (unsigned i = 0; i < 2; ++i) {
        _shift_tabs[i] = addButton(tabs, "", kColumnWidth, 50, fontMd(), kBgCard, kSelectEndpoint + i);
        lv_obj_set_pos(_shift_tabs[i], i * (kColumnWidth + kCardGap), 0);
    }

    auto *picker = createCard(_shift_editor, kContentWidth, 198, 22);
    lv_obj_set_pos(picker, kPadScreen, 116);
    lv_obj_set_style_pad_all(picker, 12, 0);
    _shift_summary = addLabel(picker, "", 0, 0, fontSm(), kTextSecondary);
    auto *hint = addLabel(picker, "滚轮支持 1 分钟精度", 0, 0, fontSm(), kTextSecondary);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, 0, 0);
    for (unsigned i = 0; i < 2; ++i) {
        char options[60 * 3];
        const unsigned count = i == 0 ? 24 : 60;
        for (unsigned n = 0; n < count; ++n) {
            options[n * 3] = '0' + n / 10;
            options[n * 3 + 1] = '0' + n % 10;
            options[n * 3 + 2] = n + 1 < count ? '\n' : '\0';
        }
        auto *roller = lv_roller_create(picker);
        lv_obj_set_style_text_font(roller, fontXl(), LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, fontXl(), LV_PART_SELECTED);
        lv_obj_set_style_text_line_space(roller, 5, LV_PART_MAIN);
        lv_obj_set_style_bg_color(roller, color(kBgCanvas), LV_PART_MAIN);
        lv_obj_set_style_text_color(roller, color(kTextMuted), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
        lv_obj_set_style_radius(roller, 14, LV_PART_MAIN);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
        lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
        lv_roller_set_visible_row_count(roller, 3);
        lv_obj_set_size(roller, 98, 136);
        lv_obj_set_pos(roller, 116 * i, 31);
        lv_obj_add_event_cb(roller, onShiftRoll, LV_EVENT_VALUE_CHANGED, this);
        _shift_rollers[i] = roller;
    }
    addLabel(picker, ":", 101, 78, fontXl());
    constexpr const char *quick[] = {":00", ":30", "-5分", "+5分"};
    for (unsigned i = 0; i < std::size(quick); ++i) {
        auto *button = addButton(picker, quick[i], 82, 56, fontMd(), kBgCardElevated, kQuickMinute + i);
        lv_obj_set_pos(button, 246 + (i % 2) * 90, 41 + (i / 2) * 64);
    }

    auto *presets = createCard(_shift_editor, kContentWidth, 88, 20);
    lv_obj_set_pos(presets, kPadScreen, 322);
    lv_obj_set_style_pad_hor(presets, 10, 0);
    lv_obj_set_style_pad_ver(presets, 8, 0);
    addLabel(presets, "常用班制一键套用", 4, 0, fontSm(), kTextSecondary);
    for (unsigned i = 0; i < std::size(kShifts); ++i) {
        auto *button = addButton(presets, kShifts[i].label, 100, 44, fontSm(), kBgCanvas, kPresetShift + i);
        lv_obj_set_style_pad_hor(button, 0, 0);
        lv_obj_set_pos(button, i * 107, 24);
    }
    refreshShiftEditor(LV_ANIM_OFF);
}

void TimeToolsApp::hideShiftEditor()
{
    lv_obj_delete(_shift_editor);
    _shift_editor = nullptr;
    refreshView();
}

void TimeToolsApp::refreshShiftEditor(lv_anim_enable_t animate)
{
    using namespace theme;
    const int span = _draft_minutes[1] - _draft_minutes[0];
    for (unsigned i = 0; i < 2; ++i) {
        const bool active = _shift_endpoint == i;
        auto *label = lv_obj_get_child(_shift_tabs[i], 0);
        lv_label_set_text_fmt(label, "%s %02d:%02d", i == 0 ? "上班" : "下班",
            _draft_minutes[i] / 60, _draft_minutes[i] % 60);
        const auto soft = i == 0 ? kAccentAmberSoft : kAccentMintSoft;
        const auto accent = i == 0 ? kAccentAmber : kAccentMint;
        const auto fg = i == 0 ? kAccentAmberFg : kAccentMintFg;
        const auto fill = color(active ? soft : kBgCard);
        lv_obj_set_style_bg_color(_shift_tabs[i], fill, 0);
        lv_obj_set_style_bg_color(_shift_tabs[i], lv_color_darken(fill, 22), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(_shift_tabs[i], active ? 2 : 1, 0);
        lv_obj_set_style_border_color(_shift_tabs[i], color(active ? accent : kBorderSubtle), 0);
        lv_obj_set_style_text_color(label, color(active ? fg : kTextSecondary), 0);
        lv_obj_set_style_bg_color(_shift_rollers[i], color(_shift_endpoint == 0 ? kAccentAmberSoft : kAccentMintSoft), LV_PART_SELECTED);
        lv_obj_set_style_text_color(_shift_rollers[i], color(_shift_endpoint == 0 ? kAccentAmberFg : kAccentMintFg), LV_PART_SELECTED);
    }
    const int minutes = _draft_minutes[_shift_endpoint];
    if (lv_roller_get_selected(_shift_rollers[0]) != minutes / 60) {
        lv_roller_set_selected(_shift_rollers[0], minutes / 60, animate);
    }
    if (lv_roller_get_selected(_shift_rollers[1]) != minutes % 60) {
        lv_roller_set_selected(_shift_rollers[1], minutes % 60, animate);
    }

    if (span <= 0) {
        lv_label_set_text(_shift_summary, "下班时间需晚于上班时间");
    } else if (span % 60 == 0) {
        lv_label_set_text_fmt(_shift_summary, "今日班次 %d小时", span / 60);
    } else {
        lv_label_set_text_fmt(_shift_summary, "今日班次 %d小时%d分", span / 60, span % 60);
    }
    lv_obj_set_style_text_color(_shift_summary, color(span > 0 ? kTextSecondary : kAccentCoralFg), 0);
    const auto save_fill = color(span > 0 ? kAccentMintFg : kBgCardElevated);
    lv_obj_set_style_bg_color(_shift_save, save_fill, 0);
    lv_obj_set_style_bg_color(_shift_save, lv_color_darken(save_fill, 22), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(lv_obj_get_child(_shift_save, 0), color(span > 0 ? kTextOnAccent : kTextSecondary), 0);
}

void TimeToolsApp::refreshOffwork(const DateTime &now)
{
    const auto &cfg = StorageService::instance().getSettings();
    const auto status = computeOffWorkCountdown(now.valid, now.hour, now.minute, now.second,
        cfg.offwork_hour, cfg.offwork_min, cfg.work_start_hour, cfg.work_start_min);

    const char *summary = "距离自由还有";
    uint32_t fg = kAccentMintFg;
    uint32_t bg = kAccentMintSoft;
    char badge[24];
    std::snprintf(badge, sizeof(badge), "%d%%", status.progress_percent);
    switch (status.phase) {
        case WorkDayPhase::INVALID:
            summary = now.valid ? (_in_hub ? "请调整班次" : "请调整上下班时间") : "时间未校准";
            fg = kAccentCoralFg;
            bg = kAccentCoralSoft;
            std::snprintf(badge, sizeof(badge), "%s", now.valid ? "需调整" : "待校时");
            break;
        case WorkDayPhase::BEFORE_WORK:
            summary = "距离上班还有";
            fg = kAccentCyanFg;
            bg = kAccentCyanSoft;
            break;
        case WorkDayPhase::OFF_WORK:
            summary = "已到下班时间";
            fg = kAccentAmberFg;
            bg = kAccentAmberSoft;
            break;
        case WorkDayPhase::WORKING:
            break;
    }
    updateBadge(_offwork_view.badge, badge, bg, fg);

    char text[64];
    if (status.phase == WorkDayPhase::INVALID) {
        setTextIfChanged(_offwork_view.time, "--:--:--");
    } else {
        const int remaining = status.remaining_seconds;
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", remaining / 3600, remaining / 60 % 60, remaining % 60);
        setTextIfChanged(_offwork_view.time, text);
    }
    lv_bar_set_value(_offwork_view.bar, status.progress_percent, LV_ANIM_OFF);

    const int hours[] = {cfg.work_start_hour, cfg.offwork_hour};
    const int minutes[] = {cfg.work_start_min, cfg.offwork_min};
    if (_in_hub) {
        std::snprintf(text, sizeof(text), "%02d:%02d-%02d:%02d · %s",
                      hours[0], minutes[0], hours[1], minutes[1], summary);
        setTextIfChanged(_offwork_view.status, text);
        return;
    }
    setTextIfChanged(_offwork_view.status, summary);
    for (unsigned i = 0; i < 2; ++i) {
        std::snprintf(text, sizeof(text), "%02d:%02d", hours[i], minutes[i]);
        setTextIfChanged(_offwork_view.endpoints[i], text);
        std::snprintf(text, sizeof(text), "%s %02d:%02d", i == 0 ? "上班" : "下班", hours[i], minutes[i]);
        setTextIfChanged(_offwork_view.scale[i * 2], text);
    }
    if (now.valid) {
        std::snprintf(text, sizeof(text), "当前 %02d:%02d", now.hour, now.minute);
        setTextIfChanged(_offwork_view.scale[1], text);
    } else {
        setTextIfChanged(_offwork_view.scale[1], "当前 --:--");
    }
}

void TimeToolsApp::refreshPomodoro(uint64_t now_ms, bool date_valid)
{
    const auto remaining = _pomo.remainingSeconds(now_ms);
    char text[80];
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(remaining / 60),
                  static_cast<unsigned>(remaining % 60));
    setTextIfChanged(_pomo_view.time, text);

    const auto status = _pomo.status();
    const auto &labels = kPomodoroText[static_cast<unsigned>(status)];
    const auto completed = static_cast<unsigned>(_pomo.completedCount());
    if (_in_hub) {
        setTextIfChanged(lv_obj_get_child(_pomo_view.badge, 0), labels.badge);
        std::snprintf(text, sizeof(text), date_valid ? "今日已完成 %u 个" : "本次已完成 %u 个",
                      date_valid ? static_cast<unsigned>(_completed_today) : completed);
    } else {
        lv_arc_set_value(_pomo_view.arc, 100 - _pomo.progressPercent(now_ms));
        setTextIfChanged(_pomo_view.state, labels.hint);
        setTextIfChanged(_pomo_view.primary_label, labels.action);
        for (auto *step : _pomo_view.steps) {
            if (status == PomodoroStatus::RUNNING) {
                lv_obj_add_state(step, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(step, LV_STATE_DISABLED);
            }
        }
        std::snprintf(text, sizeof(text), "本次已完成 %u 个番茄 · 支持后台计时", completed);
    }
    setTextIfChanged(_pomo_view.completed, text);
}

void TimeToolsApp::refreshClock(const DateTime &now)
{
    char text[64];
    if (_in_hub) {
        std::snprintf(text, sizeof(text), "%02d:%02d", now.hour, now.minute);
    } else {
        std::snprintf(text, sizeof(text), "%02d:%02d:%02d", now.hour, now.minute, now.second);
    }
    setTextIfChanged(_clock_view.time, text);

    if (now.valid) {
        const auto *weekday = ClockService::weekdayChinese(now.weekday);
        if (_in_hub) {
            std::snprintf(text, sizeof(text), "%d月%d日 · %s", now.month, now.day, weekday);
        } else {
            std::snprintf(text, sizeof(text), "%04d年%02d月%02d日 · %s", now.year, now.month, now.day, weekday);
        }
        setTextIfChanged(_clock_view.date, text);
    } else {
        setTextIfChanged(_clock_view.date, _in_hub ? "时间未校准" : "日期未校准（显示开机计时）");
    }

    if (!_in_hub) {
        const char *status = ClockService::instance().isRtcPresent() ? "板载 RTC 已同步" : "系统时钟运行中";
        updateBadge(_clock_view.badge, now.valid ? status : "待校时",
                    now.valid ? kAccentMintSoft : kAccentAmberSoft, now.valid ? kAccentMintFg : kAccentAmberFg);
    }
}

void TimeToolsApp::refreshView()
{
    const auto now_ms = nowMs();
    const auto &clock = ClockService::instance();
    const auto now = clock.getNow();
    if (now.valid) {
        const int date = now.year * 10000 + now.month * 100 + now.day;
        if (date != _completion_date) {
            _completion_date = date;
            _completed_today = 0;
        }
    }
    if (_pomo.tick(now_ms)) {
        if (now.valid) {
            ++_completed_today;
        }
        SoundService::instance().play(SoundEffect::COMPLETE);
    }

    lv_obj_set_flag(_rtc_badge, LV_OBJ_FLAG_HIDDEN, now.valid);
    if ((_in_hub || _page == Page::OFFWORK) && !_shift_editor) {
        refreshOffwork(now);
    }
    if (_in_hub || _page == Page::POMODORO) {
        refreshPomodoro(now_ms, now.valid);
    }
    if (_in_hub || _page == Page::CLOCK) {
        refreshClock(now);
    }
}

bool TimeToolsApp::onBackButtonPressed()
{
    if (_in_hub) {
        return false;
    }
    showHub();
    return true;
}

void TimeToolsApp::onBackClicked(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    if (self->onBackButtonPressed()) {
        SoundService::instance().play(SoundEffect::CLICK);
    } else {
        DeskBuddyApp::onBackBtnClicked(e);
    }
}

void TimeToolsApp::onTimeCalibrationClicked(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    if (!self || !self->_on_open_time_calibration) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_on_open_time_calibration();
}

void TimeToolsApp::onHubCardClicked(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    auto *card = lv_event_get_target_obj(e);
    const auto page = static_cast<Page>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(card)));
    SoundService::instance().play(SoundEffect::CLICK);
    self->showPage(page);
}

void TimeToolsApp::onRefreshTimer(lv_timer_t *t)
{
    auto *self = static_cast<TimeToolsApp *>(lv_timer_get_user_data(t));
    if (self) {
        self->refreshView();
    }
}

void TimeToolsApp::onPomoStartPause(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    const uint64_t t_ms = nowMs();
    if (self->_pomo.status() == PomodoroStatus::RUNNING) {
        if (self->_pomo.remainingSeconds(t_ms) > 0) {
            self->_pomo.pause(t_ms);
        }
    } else {
        self->_pomo.start(t_ms);
    }
    self->refreshView();
}

void TimeToolsApp::onPomoReset(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_pomo.reset();
    self->refreshView();
}

void TimeToolsApp::onPomoStepMin(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) return;
    if (self->_pomo.status() == PomodoroStatus::RUNNING) {
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    const int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    const int current = static_cast<int>(self->_pomo.getDurationSeconds() / 60);
    const int duration = std::clamp(current + delta, 5, 60);
    if (duration == current) {
        return;
    }
    auto &storage = StorageService::instance();
    auto settings = storage.getSettings();
    settings.pomodoro_min = static_cast<uint8_t>(duration);
    if (!storage.updateSettings(settings)) {
        return;
    }
    self->_pomo.setDurationMinutes(static_cast<uint32_t>(duration));
    self->_pomo.reset();
    self->refreshView();
}

void TimeToolsApp::onOpenShiftEditor(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    const auto endpoint = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
    SoundService::instance().play(SoundEffect::CLICK);
    self->showShiftEditor(endpoint);
}

void TimeToolsApp::onShiftEdit(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    const int action = reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
    SoundService::instance().play(SoundEffect::CLICK);
    if (action < kQuickMinute) {
        self->_shift_endpoint = action - kSelectEndpoint;
        self->refreshShiftEditor(LV_ANIM_OFF);
        return;
    }
    int &minutes = self->_draft_minutes[self->_shift_endpoint];
    if (action < kQuickMinute + 2) {
        minutes = minutes / 60 * 60 + (action - kQuickMinute) * 30;
    } else if (action < kPresetShift) {
        const int step = action == kQuickMinute + 2 ? -5 : 5;
        minutes = minutes / 60 * 60 + (minutes % 60 + step + 60) % 60;
    } else {
        const auto &preset = kShifts[action - kPresetShift];
        self->_draft_minutes[0] = preset.start;
        self->_draft_minutes[1] = preset.end;
    }
    self->refreshShiftEditor(LV_ANIM_ON);
}

void TimeToolsApp::onShiftRoll(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    self->_draft_minutes[self->_shift_endpoint] = lv_roller_get_selected(self->_shift_rollers[0]) * 60 +
                                                  lv_roller_get_selected(self->_shift_rollers[1]);
    self->refreshShiftEditor(LV_ANIM_OFF);
}

void TimeToolsApp::onCancelShift(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    self->hideShiftEditor();
}

void TimeToolsApp::onSaveShift(lv_event_t *e)
{
    auto *self = static_cast<TimeToolsApp *>(lv_event_get_user_data(e));
    if (self->_draft_minutes[1] <= self->_draft_minutes[0]) {
        lv_label_set_text(self->_shift_summary, "请先调整上班和下班时间");
        return;
    }

    auto &storage = StorageService::instance();
    auto settings = storage.getSettings();
    settings.work_start_hour = self->_draft_minutes[0] / 60;
    settings.work_start_min = self->_draft_minutes[0] % 60;
    settings.offwork_hour = self->_draft_minutes[1] / 60;
    settings.offwork_min = self->_draft_minutes[1] % 60;
    if (!storage.updateSettings(settings)) {
        lv_label_set_text(self->_shift_summary, "保存失败，请重试");
        lv_obj_set_style_text_color(self->_shift_summary, theme::color(theme::kAccentCoralFg), 0);
        return;
    }
    SoundService::instance().play(SoundEffect::CLICK);
    self->hideShiftEditor();
    if (self->_on_shift_changed) {
        self->_on_shift_changed();
    }
}

}
