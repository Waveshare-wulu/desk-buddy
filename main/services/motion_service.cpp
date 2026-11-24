#include "motion_service.hpp"

#include <cstdlib>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef M_PI
#undef M_PI
#endif
#include "qmi8658.h"

namespace desk_buddy {

namespace {
constexpr const char *TAG = "DB:Motion";
constexpr uint32_t kPollPeriodMs = 20;
}

MotionService &MotionService::instance()
{
    static MotionService s_inst;
    return s_inst;
}

bool MotionService::init(i2c_master_bus_handle_t i2c_bus, ShakeSensitivity sensitivity)
{
    _detector.setSensitivity(sensitivity);
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }
    if (i2c_bus == nullptr) {
        ESP_LOGW(TAG, "I2C bus handle is NULL, IMU shake disabled");
        return false;
    }

    auto *dev = static_cast<qmi8658_dev_t *>(std::calloc(1, sizeof(qmi8658_dev_t)));
    if (!dev) {
        return false;
    }

    esp_err_t ret = qmi8658_init(dev, i2c_bus, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 init failed (%s), touch-only fallback active", esp_err_to_name(ret));
        std::free(dev);
        return false;
    }

    qmi8658_set_accel_range(dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(dev, QMI8658_ACCEL_ODR_125HZ);
    qmi8658_set_accel_unit_mps2(dev, true);

    _qmi_dev = dev;
    _hw_ready = true;

    BaseType_t ok = xTaskCreate(imuTaskEntry, "db_imu", 4096, this, 3, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to start IMU task");
        _hw_ready = false;
        return false;
    }

    ESP_LOGI(TAG, "QMI8658 shake service started (sensitivity=%d)", static_cast<int>(sensitivity));
    return true;
}

void MotionService::setSensitivity(ShakeSensitivity sensitivity)
{
    if (_mutex) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
    }
    _detector.setSensitivity(sensitivity);
    if (_mutex) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    }
}

void MotionService::registerShakeListener(void *owner, ShakeCallback cb)
{
    if (_mutex) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
    }
    _listener_owner = owner;
    _listener_cb = std::move(cb);
    _detector.reset();
    _shake_pending.store(false, std::memory_order_release);
    if (_mutex) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    }
}

void MotionService::unregisterShakeListener(void *owner)
{
    if (_mutex) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
    }
    if (_listener_owner == owner) {
        _listener_owner = nullptr;
        _listener_cb = nullptr;
        _shake_pending.store(false, std::memory_order_release);
#ifdef DESK_BUDDY_DEVICE_SMOKE
        _smoke_shake_requested.store(false, std::memory_order_relaxed);
#endif
    }
    if (_mutex) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    }
}

void MotionService::pollOnLvglThread()
{
    if (!_shake_pending.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    ShakeCallback cb;
    if (_mutex) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
    }
    cb = _listener_cb;
    if (_mutex) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    }
    if (cb) {
        cb();
    }
}

#ifdef DESK_BUDDY_DEVICE_SMOKE
void MotionService::requestShakeOnNextSample()
{
    _smoke_shake_requested.store(true, std::memory_order_relaxed);
}
#endif

void MotionService::imuTaskEntry(void *arg)
{
    static_cast<MotionService *>(arg)->imuTaskLoop();
}

void MotionService::imuTaskLoop()
{
    auto *dev = static_cast<qmi8658_dev_t *>(_qmi_dev);
    qmi8658_data_t data{};

    while (true) {
        bool has_listener = false;
        if (_mutex) {
            xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
        }
        has_listener = (_listener_owner != nullptr && static_cast<bool>(_listener_cb));
        if (_mutex) {
            xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
        }

        if (!has_listener) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        bool ready = false;
        if (qmi8658_is_data_ready(dev, &ready) == ESP_OK && ready) {
            if (qmi8658_read_sensor_data(dev, &data) == ESP_OK) {
                const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
                if (_mutex) {
                    xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), portMAX_DELAY);
                }
                const bool listening = _listener_owner != nullptr;
                const bool triggered = listening && _detector.update(data.accelX, data.accelY, data.accelZ, now_ms);
                const float dynamic_mag = _detector.lastDynamicMag();
#ifdef DESK_BUDDY_DEVICE_SMOKE
                const bool simulated = listening && _smoke_shake_requested.exchange(false, std::memory_order_relaxed);
                if (simulated) {
                    ESP_LOGI(TAG, "Smoke test received a QMI8658 sample");
                    _shake_pending.store(true, std::memory_order_release);
                }
#endif
                if (triggered) {
                    _shake_pending.store(true, std::memory_order_release);
                }
                if (_mutex) {
                    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
                }

                if (triggered) {
                    ESP_LOGI(TAG, "Intentional shake detected (dyn=%.2f m/s^2)", dynamic_mag);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kPollPeriodMs));
    }
}

}
