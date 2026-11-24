#include "db_button_logic.hpp"

namespace desk_buddy {

namespace {
constexpr uint64_t kDebounceMs = 25;
constexpr uint64_t kLongPressMs = 1500;
}

ButtonPress ButtonLogic::update(bool pressed, uint64_t now_ms)
{
    if (pressed != _raw_pressed) {
        _raw_pressed = pressed;
        _raw_changed_ms = now_ms;
    }

    const bool stable_transition = _raw_pressed != _stable_pressed && now_ms - _raw_changed_ms >= kDebounceMs;
    if (stable_transition && _raw_pressed) {
        _stable_pressed = true;
        _pressed_at_ms = _raw_changed_ms;
        _long_reported = false;
    }
    if (!_stable_pressed) {
        return ButtonPress::NONE;
    }

    const uint64_t held_until_ms = _raw_pressed ? now_ms : _raw_changed_ms;
    ButtonPress event = ButtonPress::NONE;
    if (!_long_reported && held_until_ms - _pressed_at_ms >= kLongPressMs) {
        _long_reported = true;
        event = ButtonPress::LONG;
    }
    if (stable_transition && !_raw_pressed) {
        _stable_pressed = false;
        if (!_long_reported) {
            return ButtonPress::SHORT;
        }
    }
    return event;
}

}
