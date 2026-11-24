#include "services/button_service.hpp"

#include <utility>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "services/power_service.hpp"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Buttons";
constexpr unsigned kSamplePeriodMs = 20;
constexpr unsigned kPowerPollMs = 100;
}

bool ButtonService::start(std::function<void(ButtonEvent)> on_button)
{
    const gpio_config_t config{
        .pin_bit_mask = (1ULL << GPIO_NUM_0) | (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&config) != ESP_OK) {
        return false;
    }
    _events = xQueueCreate(8, sizeof(ButtonEvent));
    if (!_events) {
        return false;
    }
    _on_button = std::move(on_button);
    auto *timer = lv_timer_create([](lv_timer_t *t) {
        static_cast<ButtonService *>(lv_timer_get_user_data(t))->dispatch();
    }, kSamplePeriodMs, this);
    if (timer && xTaskCreate([](void *arg) {
            static_cast<ButtonService *>(arg)->pollButtons();
        }, "db_buttons", 2048, this, 4, nullptr) == pdPASS) {
        return true;
    }
    if (timer) {
        lv_timer_delete(timer);
    }
    vQueueDelete(_events);
    _events = nullptr;
    return false;
}

void ButtonService::pollButtons()
{
    ButtonLogic boot;
    ButtonLogic key;
    auto next_sample = xTaskGetTickCount();
    uint64_t last_pwr_ms = 0;
    for (;;) {
        const uint64_t now_ms = esp_timer_get_time() / 1000;
        enqueue(ButtonId::BOOT, boot.update(gpio_get_level(GPIO_NUM_0) == 0, now_ms));
        enqueue(ButtonId::KEY, key.update(gpio_get_level(GPIO_NUM_18) == 0, now_ms));
        if (now_ms - last_pwr_ms >= kPowerPollMs) {
            last_pwr_ms = now_ms;
            enqueue(ButtonId::PWR, PowerService::instance().pollPowerKey());
        }
        xTaskDelayUntil(&next_sample, pdMS_TO_TICKS(kSamplePeriodMs));
    }
}

void ButtonService::enqueue(ButtonId id, ButtonPress press)
{
    if (press == ButtonPress::NONE) {
        return;
    }
    const ButtonEvent event{id, press};
    if (xQueueSend(_events, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Button event queue is full");
    }
}

void ButtonService::dispatch()
{
    ButtonEvent event{};
    while (xQueueReceive(_events, &event, 0) == pdTRUE) {
        _on_button(event);
    }
}

}
