#pragma once

#include <cstdint>

namespace desk_buddy {

enum class ShakeSensitivity : uint8_t {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
};

class ShakeDetector {
public:
    explicit ShakeDetector(ShakeSensitivity sensitivity = ShakeSensitivity::MEDIUM);

    void setSensitivity(ShakeSensitivity sensitivity);
    ShakeSensitivity getSensitivity() const { return _sensitivity; }
    void setCooldownMs(uint32_t cooldown_ms) { _cooldown_ms = cooldown_ms; }
    void reset();

    /** Use a monotonic timestamp. Returns true only on the sample that triggers a shake. */
    bool update(float ax_mps2, float ay_mps2, float az_mps2, uint64_t now_ms);

    float lastDynamicMag() const { return _last_dyn_mag; }

private:
    void applySensitivityParams();

    ShakeSensitivity _sensitivity;
    float _threshold_mps2;
    float _rearm_mps2;
    uint32_t _window_ms;
    uint32_t _cooldown_ms;
    uint8_t _required_peaks;

    bool _initialized = false;
    float _gravity_x = 0.0f;
    float _gravity_y = 0.0f;
    float _gravity_z = 9.81f;
    float _last_dyn_mag = 0.0f;

    bool _above_threshold = false;
    uint8_t _peak_count = 0;
    uint64_t _first_peak_ms = 0;
    uint64_t _last_peak_ms = 0;
    uint64_t _last_trigger_ms = 0;
};

}
