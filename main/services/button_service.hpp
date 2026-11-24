#pragma once

#include <functional>

#include "db_button_logic.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

namespace desk_buddy {

enum class ButtonId : uint8_t { PWR, BOOT, KEY };

struct ButtonEvent {
    ButtonId id;
    ButtonPress press;
};

class ButtonService {
public:
    bool start(std::function<void(ButtonEvent)> on_button);

private:
    void pollButtons();
    void enqueue(ButtonId id, ButtonPress press);
    void dispatch();

    QueueHandle_t _events = nullptr;
    std::function<void(ButtonEvent)> _on_button;
};

}
