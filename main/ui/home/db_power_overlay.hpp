#pragma once

#include <array>
#include <functional>

#include "services/button_service.hpp"
#include "services/power_service.hpp"

namespace desk_buddy {

class ButtonGuide {
public:
    void build(lv_obj_t *parent, bool onboarding);
    void highlight(ButtonEvent event);
    void clear();

private:
    struct Card {
        lv_obj_t *body = nullptr;
        lv_obj_t *badge = nullptr;
        lv_obj_t *marker = nullptr;
        lv_timer_t *timer = nullptr;
        unsigned position = 0;
        bool onboarding = false;
    };

    static void style(Card &card, ButtonPress press);
    std::array<Card, 3> _cards{};
};

class PowerOverlay {
public:
    void bind(std::function<void(bool)> request_power, std::function<void()> confirm_guide);
    void showMenu(const BatteryStatus &battery);
    void showOnboarding();
    void showShutdown(bool reboot);
    void showMuteToast(bool sound_enabled);
    bool isMenuOpen() const { return _view == View::MENU; }
    bool handleOnboardingButton(ButtonEvent event);
    void close();

private:
    enum class View : uint8_t { NONE, MENU, ONBOARDING, SHUTDOWN };

    void setShutdownConfirmation(bool armed);
    void onShutdownClicked();

    View _view = View::NONE;
    lv_obj_t *_root = nullptr;
    lv_obj_t *_shutdown_button = nullptr;
    lv_obj_t *_shutdown_icon = nullptr;
    lv_obj_t *_shutdown_title = nullptr;
    lv_obj_t *_shutdown_subtitle = nullptr;
    lv_timer_t *_confirmation_timer = nullptr;
    lv_obj_t *_toast = nullptr;
    lv_timer_t *_toast_timer = nullptr;
    ButtonGuide _guide;
    std::function<void(bool)> _request_power;
    std::function<void()> _confirm_guide;
};

}
