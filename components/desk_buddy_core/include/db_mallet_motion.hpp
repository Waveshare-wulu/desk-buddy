#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_buddy {

class MalletMotion {
public:
    static constexpr int32_t kRestAngle = 0;
    static constexpr int32_t kStrikeAngle = -220;
    static constexpr int32_t kMinLiftAngle = -100;
    static constexpr int32_t kFrameStep = 20;
    static constexpr size_t kFrameCount = 12;
    static constexpr uint32_t kTickMs = 15;

    /** Start or queue a knock; returns true when waking from rest changes the angle immediately. */
    bool trigger();
    /** Advance motion by dt_ms (clamped per frame); returns true if the angle changed. */
    bool step(uint32_t dt_ms);
    void reset();

    int32_t angle() const { return _angle; }
    size_t frameIndex() const;
    bool isActive() const { return _phase != Phase::IDLE; }

private:
    enum class Phase : uint8_t { IDLE, STRIKE, LIFT, REBOUND };

    Phase _phase = Phase::IDLE;
    int32_t _angle = kRestAngle;
    int32_t _rebound_start = kStrikeAngle;
    uint32_t _rebound_elapsed_ms = 0;
    bool _queued = false;
};

}
