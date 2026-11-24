#pragma once

#include <cstdint>

namespace desk_buddy {

enum class ButtonPress : uint8_t {
    NONE,
    SHORT,
    LONG,
};

class ButtonLogic {
public:
    ButtonPress update(bool pressed, uint64_t now_ms);

private:
    bool _raw_pressed = false;
    bool _stable_pressed = false;
    bool _long_reported = false;
    uint64_t _raw_changed_ms = 0;
    uint64_t _pressed_at_ms = 0;
};

}
