#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include "apps/db_base_app.hpp"
#include "db_time_logic.hpp"

namespace desk_buddy {

struct DateTime;

class TimeToolsApp : public DeskBuddyApp {
public:
    explicit TimeToolsApp(std::function<void()> on_shift_changed = {},
                          std::function<void()> on_open_time_calibration = {});
    ~TimeToolsApp() override = default;
    void setRestoreViewAfterCalibration(bool restore);

protected:
    bool run(void) override;
    bool onBackButtonPressed() override;
    bool pause(void) override;
    bool resume(void) override;
    bool close(void) override;

private:
    enum class Page : uint8_t {
        OFFWORK = 0,
        POMODORO,
        CLOCK,
    };

    void buildUi();
    void showHub();
    void showPage(Page page);
    void clearContent();
    void buildHub();
    void buildClock();
    void buildPomodoro();
    void buildOffwork();
    void showShiftEditor(unsigned endpoint);
    void hideShiftEditor();
    void refreshShiftEditor(lv_anim_enable_t animate);
    void refreshOffwork(const DateTime &now);
    void refreshPomodoro(uint64_t now_ms, bool date_valid);
    void refreshClock(const DateTime &now);
    void refreshView();
    void stopRefreshTimer();

    static void onBackClicked(lv_event_t *e);
    static void onTimeCalibrationClicked(lv_event_t *e);
    static void onHubCardClicked(lv_event_t *e);
    static void onRefreshTimer(lv_timer_t *t);
    static void onPomoStartPause(lv_event_t *e);
    static void onPomoReset(lv_event_t *e);
    static void onPomoStepMin(lv_event_t *e);
    static void onOpenShiftEditor(lv_event_t *e);
    static void onShiftEdit(lv_event_t *e);
    static void onShiftRoll(lv_event_t *e);
    static void onCancelShift(lv_event_t *e);
    static void onSaveShift(lv_event_t *e);

    Page _page = Page::OFFWORK;
    bool _in_hub = true;
    std::atomic<bool> _restore_view_after_calibration{false};
    std::function<void()> _on_shift_changed;
    std::function<void()> _on_open_time_calibration;
    PomodoroTimer _pomo;
    bool _pomo_loaded = false;
    int _completion_date = 0;
    uint32_t _completed_today = 0;

    lv_obj_t *_back_label = nullptr;
    lv_obj_t *_title_label = nullptr;
    lv_obj_t *_rtc_badge = nullptr;
    lv_obj_t *_content_box = nullptr;
    lv_timer_t *_refresh_timer = nullptr;

    struct {
        lv_obj_t *time = nullptr;
        lv_obj_t *date = nullptr;
        lv_obj_t *badge = nullptr;
    } _clock_view;

    struct {
        lv_obj_t *time = nullptr;
        lv_obj_t *badge = nullptr;
        lv_obj_t *arc = nullptr;
        lv_obj_t *state = nullptr;
        lv_obj_t *primary_label = nullptr;
        lv_obj_t *completed = nullptr;
        lv_obj_t *steps[2] = {};
    } _pomo_view;

    struct {
        lv_obj_t *time = nullptr;
        lv_obj_t *status = nullptr;
        lv_obj_t *badge = nullptr;
        lv_obj_t *bar = nullptr;
        lv_obj_t *endpoints[2] = {};
        lv_obj_t *scale[3] = {};
    } _offwork_view;

    lv_obj_t *_shift_editor = nullptr;
    lv_obj_t *_shift_tabs[2] = {};
    lv_obj_t *_shift_rollers[2] = {};
    lv_obj_t *_shift_summary = nullptr;
    lv_obj_t *_shift_save = nullptr;
    int _draft_minutes[2] = {};
    unsigned _shift_endpoint = 0;
};

}
