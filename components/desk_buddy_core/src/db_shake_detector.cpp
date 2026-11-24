#include "db_shake_detector.hpp"

#include <cmath>

namespace desk_buddy {

ShakeDetector::ShakeDetector(ShakeSensitivity sensitivity)
    : _sensitivity(sensitivity)
    , _threshold_mps2(13.5f)
    , _rearm_mps2(6.5f)
    , _window_ms(550)
    , _cooldown_ms(950)
    , _required_peaks(2)
{
    applySensitivityParams();
}

void ShakeDetector::applySensitivityParams()
{
    switch (_sensitivity) {
        case ShakeSensitivity::LOW:
            _threshold_mps2 = 17.5f;
            _rearm_mps2 = 8.0f;
            _window_ms = 500;
            _required_peaks = 2;
            break;
        case ShakeSensitivity::MEDIUM:
            _threshold_mps2 = 13.5f;
            _rearm_mps2 = 6.5f;
            _window_ms = 550;
            _required_peaks = 2;
            break;
        case ShakeSensitivity::HIGH:
            _threshold_mps2 = 10.2f;
            _rearm_mps2 = 5.0f;
            _window_ms = 600;
            _required_peaks = 2;
            break;
    }
}

void ShakeDetector::setSensitivity(ShakeSensitivity sensitivity)
{
    _sensitivity = sensitivity;
    applySensitivityParams();
}

void ShakeDetector::reset()
{
    _initialized = false;
    _gravity_x = 0.0f;
    _gravity_y = 0.0f;
    _gravity_z = 9.81f;
    _last_dyn_mag = 0.0f;
    _above_threshold = false;
    _peak_count = 0;
    _first_peak_ms = 0;
    _last_peak_ms = 0;
    _last_trigger_ms = 0;
}

bool ShakeDetector::update(float ax_mps2, float ay_mps2, float az_mps2, uint64_t now_ms)
{
    if (!_initialized) {
        _gravity_x = ax_mps2;
        _gravity_y = ay_mps2;
        _gravity_z = az_mps2;
        _initialized = true;
        _last_dyn_mag = 0.0f;
        return false;
    }

    constexpr float kGravityRetention = 0.85f;
    constexpr uint64_t kMinPeakSeparationMs = 60;
    _gravity_x = kGravityRetention * _gravity_x + (1.0f - kGravityRetention) * ax_mps2;
    _gravity_y = kGravityRetention * _gravity_y + (1.0f - kGravityRetention) * ay_mps2;
    _gravity_z = kGravityRetention * _gravity_z + (1.0f - kGravityRetention) * az_mps2;

    const float dx = ax_mps2 - _gravity_x;
    const float dy = ay_mps2 - _gravity_y;
    const float dz = az_mps2 - _gravity_z;
    const float dyn_mag = std::sqrt(dx * dx + dy * dy + dz * dz);
    _last_dyn_mag = dyn_mag;

    if (_last_trigger_ms != 0 && now_ms >= _last_trigger_ms && (now_ms - _last_trigger_ms) < _cooldown_ms) {
        _peak_count = 0;
        if (dyn_mag < _rearm_mps2) {
            _above_threshold = false;
        }
        return false;
    }

    if (_peak_count > 0 && now_ms >= _first_peak_ms && (now_ms - _first_peak_ms) > _window_ms) {
        _peak_count = 0;
    }

    if (!_above_threshold && dyn_mag >= _threshold_mps2) {
        if (_peak_count == 0 || (now_ms >= _last_peak_ms && now_ms - _last_peak_ms >= kMinPeakSeparationMs)) {
            _above_threshold = true;
            if (_peak_count == 0) {
                _first_peak_ms = now_ms;
            }
            _last_peak_ms = now_ms;
            ++_peak_count;

            if (_peak_count >= _required_peaks) {
                _peak_count = 0;
                _last_trigger_ms = now_ms;
                return true;
            }
        }
    } else if (_above_threshold && dyn_mag <= _rearm_mps2) {
        _above_threshold = false;
    }

    return false;
}

}
