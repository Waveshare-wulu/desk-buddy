#include "db_mallet_motion.hpp"

#include <algorithm>

namespace desk_buddy {

namespace {

constexpr uint32_t kMaxStepDtMs = 18;
constexpr int32_t kMaxStepAngle = 60;
constexpr int32_t kStrikeTenthsPer10Ms = 37;
constexpr int32_t kLiftTenthsPer10Ms = 32;
constexpr uint32_t kReboundDurationMs = 160;

int32_t clampedDelta(uint32_t dt_ms, int32_t tenths_per_10ms)
{
    const int32_t raw = static_cast<int32_t>((dt_ms * static_cast<uint32_t>(tenths_per_10ms) + 5u) / 10u);
    return std::clamp<int32_t>(raw, 1, kMaxStepAngle);
}

}

bool MalletMotion::trigger()
{
    if (_phase == Phase::IDLE) {
        _phase = Phase::STRIKE;
        _queued = false;
        return step(kTickMs);
    }
    if (_phase == Phase::STRIKE || _phase == Phase::LIFT) {
        _queued = true;
        return false;
    }

    _queued = false;
    _phase = _angle <= kMinLiftAngle ? Phase::LIFT : Phase::STRIKE;
    return false;
}

bool MalletMotion::step(uint32_t dt_ms)
{
    if (_phase == Phase::IDLE) {
        return false;
    }

    const uint32_t dt = std::clamp<uint32_t>(dt_ms, 1u, kMaxStepDtMs);
    const int32_t prev = _angle;

    switch (_phase) {
    case Phase::STRIKE:
        _angle = std::max(kStrikeAngle, _angle - clampedDelta(dt, kStrikeTenthsPer10Ms));
        if (_angle <= kStrikeAngle) {
            _angle = kStrikeAngle;
            if (_queued) {
                _queued = false;
                _phase = Phase::LIFT;
            } else {
                _phase = Phase::REBOUND;
                _rebound_start = kStrikeAngle;
                _rebound_elapsed_ms = 0;
            }
        }
        break;
    case Phase::LIFT:
        _angle = std::min(kMinLiftAngle, _angle + clampedDelta(dt, kLiftTenthsPer10Ms));
        if (_angle >= kMinLiftAngle) {
            _angle = kMinLiftAngle;
            _phase = Phase::STRIKE;
        }
        break;
    case Phase::REBOUND:
        _rebound_elapsed_ms += dt;
        if (_rebound_elapsed_ms >= kReboundDurationMs) {
            _angle = kRestAngle;
            _phase = Phase::IDLE;
        } else {
            const int64_t remaining = static_cast<int64_t>(kReboundDurationMs - _rebound_elapsed_ms);
            const int64_t span = static_cast<int64_t>(_rebound_start - kRestAngle);
            const int64_t denom = static_cast<int64_t>(kReboundDurationMs) * kReboundDurationMs;
            const int32_t target = kRestAngle + static_cast<int32_t>(span * remaining * remaining / denom);
            _angle = std::min(kRestAngle, std::min(target, _angle + kMaxStepAngle));
        }
        break;
    case Phase::IDLE:
        break;
    }

    return _angle != prev;
}

void MalletMotion::reset()
{
    _phase = Phase::IDLE;
    _angle = kRestAngle;
    _rebound_start = kStrikeAngle;
    _rebound_elapsed_ms = 0;
    _queued = false;
}

size_t MalletMotion::frameIndex() const
{
    const int32_t clamped = std::clamp(_angle, kStrikeAngle, kRestAngle);
    return static_cast<size_t>((-clamped + kFrameStep / 2) / kFrameStep);
}

}
