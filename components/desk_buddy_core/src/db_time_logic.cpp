#include "db_time_logic.hpp"

#include <algorithm>

namespace desk_buddy {

PomodoroTimer::PomodoroTimer(uint32_t duration_sec)
    : _duration_sec(std::clamp<uint32_t>(duration_sec, 60u, 120u * 60u))
    , _remaining_ms(_duration_sec * 1000u)
{
}

void PomodoroTimer::setDurationMinutes(uint32_t minutes)
{
    minutes = std::clamp<uint32_t>(minutes, 1u, 120u);
    _duration_sec = minutes * 60u;
    if (_status == PomodoroStatus::IDLE || _status == PomodoroStatus::FINISHED) {
        _remaining_ms = _duration_sec * 1000u;
        _status = PomodoroStatus::IDLE;
    }
}

void PomodoroTimer::start(uint64_t now_ms)
{
    if (_status == PomodoroStatus::RUNNING) {
        return;
    }
    if (_status == PomodoroStatus::FINISHED || _remaining_ms == 0) {
        _remaining_ms = _duration_sec * 1000u;
    }
    _last_resume_ms = now_ms;
    _status = PomodoroStatus::RUNNING;
}

void PomodoroTimer::pause(uint64_t now_ms)
{
    if (_status != PomodoroStatus::RUNNING) {
        return;
    }
    tick(now_ms);
    if (_status == PomodoroStatus::RUNNING) {
        _status = PomodoroStatus::PAUSED;
    }
}

void PomodoroTimer::reset()
{
    _remaining_ms = _duration_sec * 1000u;
    _last_resume_ms = 0;
    _status = PomodoroStatus::IDLE;
}

bool PomodoroTimer::tick(uint64_t now_ms)
{
    if (_status != PomodoroStatus::RUNNING) {
        return false;
    }
    if (now_ms <= _last_resume_ms) {
        return false;
    }
    const uint64_t elapsed = now_ms - _last_resume_ms;
    _last_resume_ms = now_ms;

    if (elapsed >= _remaining_ms) {
        _remaining_ms = 0;
        _status = PomodoroStatus::FINISHED;
        ++_completed_count;
        return true;
    }
    _remaining_ms -= static_cast<uint32_t>(elapsed);
    return false;
}

uint32_t PomodoroTimer::remainingSeconds(uint64_t now_ms) const
{
    uint32_t rem_ms = _remaining_ms;
    if (_status == PomodoroStatus::RUNNING && now_ms > _last_resume_ms) {
        const uint64_t elapsed = now_ms - _last_resume_ms;
        rem_ms = (elapsed >= rem_ms) ? 0u : static_cast<uint32_t>(rem_ms - elapsed);
    }
    return (rem_ms + 999u) / 1000u;
}

int PomodoroTimer::progressPercent(uint64_t now_ms) const
{
    if (_duration_sec == 0) {
        return 0;
    }
    const uint32_t total_ms = _duration_sec * 1000u;
    uint32_t rem_ms = _remaining_ms;
    if (_status == PomodoroStatus::RUNNING && now_ms > _last_resume_ms) {
        const uint64_t elapsed = now_ms - _last_resume_ms;
        rem_ms = (elapsed >= rem_ms) ? 0u : static_cast<uint32_t>(rem_ms - elapsed);
    }
    const uint32_t done_ms = (rem_ms <= total_ms) ? (total_ms - rem_ms) : 0u;
    return static_cast<int>((static_cast<uint64_t>(done_ms) * 100ULL) / total_ms);
}

OffWorkStatus computeOffWorkCountdown(
    bool time_valid,
    int cur_hour,
    int cur_min,
    int cur_sec,
    int target_hour,
    int target_min,
    int start_hour,
    int start_min)
{
    OffWorkStatus res{};
    res.time_valid = time_valid;
    if (!time_valid) {
        return res;
    }

    const int32_t cur_total = std::clamp(cur_hour, 0, 23) * 3600 +
                              std::clamp(cur_min, 0, 59) * 60 +
                              std::clamp(cur_sec, 0, 59);
    const int32_t target_total = std::clamp(target_hour, 0, 23) * 3600 +
                                 std::clamp(target_min, 0, 59) * 60;
    const int32_t start_total = std::clamp(start_hour, 0, 23) * 3600 +
                                std::clamp(start_min, 0, 59) * 60;

    if (target_total <= start_total) {
        return res;
    }
    if (cur_total < start_total) {
        res.phase = WorkDayPhase::BEFORE_WORK;
        res.remaining_seconds = start_total - cur_total;
    } else if (cur_total >= target_total) {
        res.phase = WorkDayPhase::OFF_WORK;
        res.is_off_work = true;
        res.progress_percent = 100;
    } else {
        res.phase = WorkDayPhase::WORKING;
        res.remaining_seconds = target_total - cur_total;
        res.progress_percent = (cur_total - start_total) * 100 / (target_total - start_total);
    }
    return res;
}

}
