#include <array>
#include <cstdlib>
#include <memory>
#include <utility>

#include "brookesia/system_core/system.hpp"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "apps/daily_fortune/daily_fortune_app.hpp"
#include "apps/game_2048/game_2048_app.hpp"
#include "apps/random_lab/random_lab_app.hpp"
#include "apps/relative_calculator/relative_calc_app.hpp"
#include "apps/settings/settings_app.hpp"
#include "apps/time_tools/time_tools_app.hpp"
#include "apps/wooden_fish/wooden_fish_app.hpp"
#include "services/brookesia_storage.hpp"
#include "services/button_service.hpp"
#include "services/clock_service.hpp"
#include "services/wifi_time_service.hpp"
#include "services/display_service.hpp"
#include "services/motion_service.hpp"
#include "services/power_service.hpp"
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "ui/home/db_desktop.hpp"
#include "ui/home/db_power_overlay.hpp"
#include "ui/i18n/db_i18n.hpp"

#ifdef DESK_BUDDY_DEVICE_SMOKE
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#endif

using namespace desk_buddy;
namespace core = esp_brookesia::system::core;

namespace {

constexpr const char *TAG = "DeskBuddy";
constexpr uint32_t kShakeDispatchPeriodMs = 50;
constexpr size_t kWoodenFish = 1;
constexpr size_t kDailyFortune = 2;
constexpr size_t kTimeTools = 5;
constexpr size_t kSettings = 6;

enum class SystemAction : uint8_t {
    START_APP, STOP_APP, OPEN_SETTINGS, OPEN_TIME_CALIBRATION,
    SHUTDOWN, REBOOT, TOGGLE_SCREEN, OPEN_POWER_MENU, TOGGLE_MUTE, MARK_BUTTON_GUIDE_SEEN,
};

struct SystemCommand {
    SystemAction action;
    core::AppId id = core::INVALID_APP_ID;
};

struct SystemUi {
    Desktop desktop;
    PowerOverlay power_overlay;
    uint32_t tick_count = 0;
    int last_day_key = -1;
    bool shutting_down = false;
};

bool queueSystemCommand(QueueHandle_t queue, SystemCommand command)
{
    if (xQueueSend(queue, &command, 0) == pdTRUE) {
        return true;
    }
    ESP_LOGW(TAG, "System command queue is full");
    return false;
}

bool applyAppCommand(core::System &system, const SystemCommand &command)
{
    const auto active = system.get_active_app();
    const bool start = command.action == SystemAction::START_APP;
    if (start ? active.has_value() : !active || active->app_id != command.id) {
        return false;
    }
    const auto result = start ? system.start_app(command.id) : system.stop_app(command.id);
    if (!result) {
        ESP_LOGE(TAG, "App %lu %s failed: %s", static_cast<unsigned long>(command.id),
                 start ? "start" : "stop", result.error().c_str());
    }
    return result.has_value();
}

void clearShutdownState(SystemUi &ui)
{
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    ui.power_overlay.close();
    ui.shutting_down = false;
    lv_display_trigger_activity(nullptr);
    lv_obj_invalidate(lv_screen_active());
    esp_lv_adapter_unlock();
}

void runPowerCommand(core::System &system, SystemUi &ui, SystemAction action)
{
    auto &power = PowerService::instance();
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    if (action == SystemAction::SHUTDOWN && !power.getBatteryStatus().pmu_present) {
        ui.shutting_down = false;
        esp_lv_adapter_unlock();
        ESP_LOGE(TAG, "Cannot shut down without AXP2101");
        return;
    }

    ui.shutting_down = true;
    power.wakeUp();
    ui.power_overlay.showShutdown(action == SystemAction::REBOOT);
    lv_refr_now(nullptr);
    esp_lv_adapter_unlock();
    vTaskDelay(pdMS_TO_TICKS(350));

    WifiTimeService::instance().prepareForShutdown();
    if (const auto active = system.get_active_app()) {
        if (!applyAppCommand(system, {SystemAction::STOP_APP, active->app_id})) {
            clearShutdownState(ui);
            return;
        }
    }
    auto &storage = StorageService::instance();
    if (!storage.flushMeritIfNeeded(true)) {
        ESP_LOGE(TAG, "Could not save merit before shutdown");
        clearShutdownState(ui);
        return;
    }
    auto &sound = SoundService::instance();
    sound.prepareForShutdown();

    if (esp_lv_adapter_pause(-1) == ESP_OK) {
        const auto panel_result = setDisplayPower(false);
        if (panel_result != ESP_OK) {
            ESP_LOGW(TAG, "Could not turn off AMOLED: %s", esp_err_to_name(panel_result));
        }
        if (action == SystemAction::REBOOT) {
            power.reboot();
        }
        if (power.powerOff()) {
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        ESP_LOGE(TAG, "AXP2101 did not power off");
        setDisplayPower(true);
        setDisplayBrightness(power.getBrightness());
        esp_lv_adapter_resume();
    } else {
        ESP_LOGE(TAG, "Could not pause display before shutdown");
    }
    sound.setEnabled(storage.getSettings().sound_enabled);
    clearShutdownState(ui);
}

void dispatchButton(SystemUi &ui, QueueHandle_t queue, const auto &apps, const auto &ids, ButtonEvent event)
{
    if (ui.shutting_down) {
        return;
    }
    const bool short_press = event.press == ButtonPress::SHORT;
    auto &power = PowerService::instance();
    if (power.getScreenState() == ScreenPowerState::SLEEP) {
        if (event.id == ButtonId::PWR) {
            queueSystemCommand(queue, {short_press ? SystemAction::TOGGLE_SCREEN : SystemAction::OPEN_POWER_MENU});
        }
        return;
    }

    lv_display_trigger_activity(nullptr);
    if (ui.power_overlay.handleOnboardingButton(event)) {
        return;
    }
    if (ui.power_overlay.isMenuOpen()) {
        if (event.id == ButtonId::KEY) {
            return;
        }
        if (short_press) {
            ui.power_overlay.close();
            return;
        }
        if (event.id == ButtonId::PWR) {
            return;
        }
    }

    size_t active = 0;
    while (active < apps.size() && !apps[active]->isVisible()) {
        ++active;
    }
    if (active == kSettings && static_cast<SettingsApp &>(*apps[kSettings]).handleButtonGuide(event)) {
        return;
    }
    if (event.id == ButtonId::PWR) {
        queueSystemCommand(queue, {short_press ? SystemAction::TOGGLE_SCREEN : SystemAction::OPEN_POWER_MENU});
    } else if (event.id == ButtonId::BOOT) {
        if (!short_press) {
            queueSystemCommand(queue, {SystemAction::TOGGLE_MUTE});
        } else if (active == apps.size()) {
            ui.desktop.onBackButtonPressed();
        } else if (!apps[active]->onBackButtonPressed()) {
            queueSystemCommand(queue, {SystemAction::STOP_APP, ids[active]});
        }
    } else if (short_press) {
        if (active == apps.size()) {
            queueSystemCommand(queue, {SystemAction::START_APP, ids[kWoodenFish]});
        } else {
            apps[active]->onUserButtonPressed();
        }
    } else if (active != kSettings) {
        const auto active_id = active < apps.size() ? ids[active] : core::INVALID_APP_ID;
        queueSystemCommand(queue, {SystemAction::OPEN_SETTINGS, active_id});
    }
}

void toggleMute(SystemUi &ui, const auto &apps)
{
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    auto &storage = StorageService::instance();
    auto settings = storage.getSettings();
    settings.sound_enabled = !settings.sound_enabled;
    if (storage.updateSettings(settings)) {
        SoundService::instance().setEnabled(settings.sound_enabled);
        for (const auto &app : apps) {
            if (app->isVisible()) {
                app->onSoundEnabledChanged();
            }
        }
        ui.power_overlay.showMuteToast(settings.sound_enabled);
    } else {
        ESP_LOGE(TAG, "Could not save sound setting");
    }
    esp_lv_adapter_unlock();
}

#ifdef DESK_BUDDY_DEVICE_SMOKE
#include "../tests/device_ui_smoke.inc"
#endif

void onSystemTick(lv_timer_t *timer)
{
    auto &ui = *static_cast<SystemUi *>(lv_timer_get_user_data(timer));
    if (ui.shutting_down) {
        return;
    }
    auto &power = PowerService::instance();
    power.tickLvgl();

    ++ui.tick_count;
    if (ui.tick_count % 2 == 0) {
        const DateTime time = ClockService::instance().getNow();
        ui.desktop.refreshStatus(time, power.getBatteryStatus());
        ui.desktop.homeHeader().refreshClockAndStats();

        const int day_key = time.valid ? time.year * 10000 + time.month * 100 + time.day : -2;
        if (day_key != ui.last_day_key || ui.tick_count % 120 == 0) {
            ui.last_day_key = day_key;
            ui.desktop.homeHeader().refreshDailyContent();
        }
    }
    if (ui.tick_count % 30 == 0) {
        StorageService::instance().flushMeritIfNeeded(false);
    }
}

}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Desk Buddy Warm Cream UI");

    StorageService::instance().init();
    const auto &settings = StorageService::instance().getSettings();
    i18n::setLanguage(settings.language == 1 ? i18n::Lang::EN_US : i18n::Lang::ZH_CN);

    lv_indev_t *touch_input = nullptr;
    auto *display = startDisplay(touch_input);
    if (!display) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    auto *bus = bsp_i2c_get_handle();
    ClockService::instance().init(bus);
    WifiTimeService::instance().init();
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    auto &power = PowerService::instance();
    power.init(bus, settings.brightness, settings.dim_timeout_s, settings.sleep_timeout_s);
    esp_lv_adapter_unlock();
    const bool motion_ready = MotionService::instance().init(bus, static_cast<ShakeSensitivity>(settings.shake_sensitivity));
#ifdef DESK_BUDDY_DEVICE_SMOKE
    SoundService::instance().init(false, settings.volume);
#else
    SoundService::instance().init(settings.sound_enabled, settings.volume);
#endif

    if (!initBrookesiaStorage()) {
        ESP_LOGE(TAG, "Unable to initialize System Core storage");
        return;
    }

    core::System system;
    core::System::Config system_config{};
    system_config.system_type = "desk-buddy";
    system_config.install_registered_apps = false;
    system_config.install_package_apps = false;
    if (const auto init = system.init(std::move(system_config)); !init) {
        ESP_LOGE(TAG, "System Core initialization failed: %s", init.error().c_str());
        return;
    }
    if (const auto start = system.start(); !start) {
        ESP_LOGE(TAG, "System Core start failed: %s", start.error().c_str());
        return;
    }

    const auto queue = xQueueCreate(4, sizeof(SystemCommand));
    if (!queue) {
        ESP_LOGE(TAG, "Unable to allocate app command queue");
        return;
    }

    SystemUi ui;
    std::array<core::AppId, 7> ids{};
    const auto open_time_calibration = [queue](core::AppId source_id) {
        queueSystemCommand(queue, {SystemAction::OPEN_TIME_CALIBRATION, source_id});
    };
    const std::array<std::shared_ptr<DeskBuddyApp>, ids.size()> apps = {
        std::make_shared<RandomLabApp>(),
        std::make_shared<WoodenFishApp>(),
        std::make_shared<DailyFortuneApp>([&ids, open_time_calibration]() {
            open_time_calibration(ids[kDailyFortune]);
        }),
        std::make_shared<Game2048App>(),
        std::make_shared<RelativeCalcApp>(),
        std::make_shared<TimeToolsApp>([&ui]() {
            ui.desktop.homeHeader().refreshClockAndStats();
        }, [&ids, open_time_calibration]() {
            open_time_calibration(ids[kTimeTools]);
        }),
        std::make_shared<SettingsApp>([&ui]() {
            ui.desktop.homeHeader().refreshDailyContent();
            ui.desktop.homeHeader().refreshClockAndStats();
        }, [&ui]() {
            ui.power_overlay.showMenu(PowerService::instance().getBatteryStatus());
        }),
    };
    for (size_t index = 0; index < apps.size(); ++index) {
        const auto installed = system.install_app(apps[index]);
        if (!installed) {
            ESP_LOGE(TAG, "Unable to install %s: %s", apps[index]->name(), installed.error().c_str());
            vQueueDelete(queue);
            return;
        }
        ids[index] = *installed;
    }

    const auto launch = [queue](core::AppId id) { queueSystemCommand(queue, {SystemAction::START_APP, id}); };
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    ui.power_overlay.bind([&ui, queue](bool reboot) {
        if (!ui.shutting_down && queueSystemCommand(queue, {reboot ? SystemAction::REBOOT : SystemAction::SHUTDOWN})) {
            ui.shutting_down = true;
        }
    }, [queue]() { queueSystemCommand(queue, {SystemAction::MARK_BUTTON_GUIDE_SEEN}); });
    ui.desktop.create(lv_display_get_screen_active(display), touch_input, launch);
    for (size_t index = 0; index < apps.size(); ++index) {
        const auto id = ids[index];
        const auto &app = apps[index];
        app->attach(ui.desktop.appLayer(), [queue, id]() { queueSystemCommand(queue, {SystemAction::STOP_APP, id}); });
        ui.desktop.addApp(id, app->launcherPage(), app->name(), app->icon());
    }
    ui.desktop.showHome({
        .open_time = [launch, id = ids[kTimeTools]]() { launch(id); },
        .open_fortune = [launch, id = ids[kDailyFortune]]() { launch(id); },
        .open_time_calibration = [open_time_calibration]() { open_time_calibration(core::INVALID_APP_ID); },
    });
    ui.desktop.refreshStatus(ClockService::instance().getNow(), power.getBatteryStatus());
    lv_timer_create(onSystemTick, 500, &ui);
    if (motion_ready && !lv_timer_create([](lv_timer_t *timer) {
        if (!static_cast<SystemUi *>(lv_timer_get_user_data(timer))->shutting_down) {
            MotionService::instance().pollOnLvglThread();
        }
    }, kShakeDispatchPeriodMs, &ui)) {
        ESP_LOGE(TAG, "Unable to create the shake dispatch timer");
    }
    ButtonService buttons;
    if (!buttons.start([&ui, queue, &apps, &ids](ButtonEvent event) {
            dispatchButton(ui, queue, apps, ids, event);
        })) {
        ESP_LOGE(TAG, "Unable to start the physical button service");
    }
#ifndef DESK_BUDDY_DEVICE_SMOKE
    if (!StorageService::instance().isButtonGuideSeen()) {
        ui.power_overlay.showOnboarding();
    }
#endif
    lv_obj_invalidate(lv_display_get_screen_active(display));
    lv_refr_now(display);
    power.setBrightness(settings.brightness);
    ESP_ERROR_CHECK(setDisplayPower(true));
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Warm Cream desktop ready with %zu native apps", apps.size());

#ifdef DESK_BUDDY_DEVICE_SMOKE
    runDeviceSmoke(system, ui.desktop, display, touch_input, queue, ids);
#else
    if (settings.wifi_auto_sync) {
        WifiTimeService::instance().scheduleBootSync();
    }
#endif

    const auto settings_id = ids[kSettings];
    auto &settings_app = static_cast<SettingsApp &>(*apps[kSettings]);
    auto &time_tools_app = static_cast<TimeToolsApp &>(*apps[kTimeTools]);
    const auto reopen_after_calibration = [&](core::AppId id) {
        if (id == core::INVALID_APP_ID) {
            return;
        }
        const bool restore_time_tools = id == ids[kTimeTools];
        if (restore_time_tools) {
            time_tools_app.setRestoreViewAfterCalibration(true);
        }
        if (!applyAppCommand(system, {SystemAction::START_APP, id}) && restore_time_tools) {
            time_tools_app.setRestoreViewAfterCalibration(false);
        }
    };
    core::AppId return_app_id = core::INVALID_APP_ID;
    SystemCommand command{};
    for (;;) {
        if (xQueueReceive(queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (command.action) {
            case SystemAction::START_APP:
                if (applyAppCommand(system, command)) {
                    return_app_id = core::INVALID_APP_ID;
                }
                break;
            case SystemAction::STOP_APP:
                if (applyAppCommand(system, command) && command.id == settings_id) {
                    reopen_after_calibration(std::exchange(return_app_id, core::INVALID_APP_ID));
                }
                break;
            case SystemAction::OPEN_SETTINGS:
            case SystemAction::OPEN_TIME_CALIBRATION: {
                const bool direct_time = command.action == SystemAction::OPEN_TIME_CALIBRATION;
                const auto active = system.get_active_app();
                const auto active_id = active ? active->app_id : core::INVALID_APP_ID;
                const bool source_changed = active_id != command.id;
                if (active_id == settings_id || (source_changed && (direct_time || active))) {
                    break;
                }
                if (active && !applyAppCommand(system, {SystemAction::STOP_APP, active_id})) {
                    break;
                }
                return_app_id = core::INVALID_APP_ID;
                settings_app.setLaunchDirectTime(direct_time);
                if (applyAppCommand(system, {SystemAction::START_APP, settings_id})) {
                    if (direct_time) {
                        return_app_id = active_id;
                    }
                } else if (direct_time) {
                    settings_app.setLaunchDirectTime(false);
                    reopen_after_calibration(active_id);
                }
                break;
            }
            case SystemAction::SHUTDOWN:
            case SystemAction::REBOOT:
                return_app_id = core::INVALID_APP_ID;
                runPowerCommand(system, ui, command.action);
                break;
            case SystemAction::TOGGLE_SCREEN:
                ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
                if (!ui.shutting_down) {
                    PowerService::instance().toggleScreen();
                }
                esp_lv_adapter_unlock();
                break;
            case SystemAction::OPEN_POWER_MENU: {
                ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
                if (!ui.shutting_down) {
                    auto &power = PowerService::instance();
                    power.wakeUp();
                    ui.power_overlay.showMenu(power.getBatteryStatus());
                }
                esp_lv_adapter_unlock();
                break;
            }
            case SystemAction::TOGGLE_MUTE:
                toggleMute(ui, apps);
                break;
            case SystemAction::MARK_BUTTON_GUIDE_SEEN:
                if (StorageService::instance().markButtonGuideSeen()) {
                    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
                    ui.power_overlay.close();
                    esp_lv_adapter_unlock();
                } else {
                    ESP_LOGE(TAG, "Could not save button guide acknowledgment");
                }
                break;
        }
    }
}
