#pragma once

#include <cstdint>

namespace desk_buddy {

enum class PomodoroStatus : uint8_t {
    IDLE = 0,
    RUNNING,
    PAUSED,
    FINISHED,
};

class PomodoroTimer {
public:
    explicit PomodoroTimer(uint32_t duration_sec = 25 * 60);

    void setDurationMinutes(uint32_t minutes);
    uint32_t getDurationSeconds() const { return _duration_sec; }

    void start(uint64_t now_ms);
    void pause(uint64_t now_ms);
    void reset();

    /** Update state for current monotonic time. Returns true on the transition to FINISHED. */
    bool tick(uint64_t now_ms);

    uint32_t remainingSeconds(uint64_t now_ms) const;
    int progressPercent(uint64_t now_ms) const;
    PomodoroStatus status() const { return _status; }
    uint32_t completedCount() const { return _completed_count; }

private:
    uint32_t _duration_sec;
    uint32_t _remaining_ms;
    uint64_t _last_resume_ms = 0;
    PomodoroStatus _status = PomodoroStatus::IDLE;
    uint32_t _completed_count = 0;
};

enum class WorkDayPhase : uint8_t {
    INVALID = 0,
    BEFORE_WORK,
    WORKING,
    OFF_WORK,
};

struct OffWorkStatus {
    bool time_valid;
    bool is_off_work;
    int32_t remaining_seconds;  // Seconds until the next shift boundary.
    int progress_percent;
    WorkDayPhase phase = WorkDayPhase::INVALID;
};

OffWorkStatus computeOffWorkCountdown(
    bool time_valid,
    int cur_hour,
    int cur_min,
    int cur_sec,
    int target_hour,
    int target_min,
    int start_hour = 9,
    int start_min = 0
);

}
