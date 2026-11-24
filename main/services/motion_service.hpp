#pragma once

#include <atomic>
#include <functional>
#include "driver/i2c_master.h"
#include "db_shake_detector.hpp"

namespace desk_buddy {

class MotionService {
public:
    using ShakeCallback = std::function<void()>;

    static MotionService &instance();

    bool init(i2c_master_bus_handle_t i2c_bus, ShakeSensitivity sensitivity);
    void setSensitivity(ShakeSensitivity sensitivity);
    bool isReady() const { return _hw_ready; }

    /** Replaces any active listener. Only the matching owner can unregister it. */
    void registerShakeListener(void *owner, ShakeCallback cb);
    void unregisterShakeListener(void *owner);

    /** Dispatches the pending callback synchronously on the calling LVGL task. */
    void pollOnLvglThread();

#ifdef DESK_BUDDY_DEVICE_SMOKE
    void requestShakeOnNextSample();
#endif

private:
    MotionService() = default;
    static void imuTaskEntry(void *arg);
    void imuTaskLoop();

    bool _hw_ready = false;
    void *_qmi_dev = nullptr;
    ShakeDetector _detector;

    void *_listener_owner = nullptr;
    ShakeCallback _listener_cb;
    void *_mutex = nullptr;
    std::atomic<bool> _shake_pending{false};
#ifdef DESK_BUDDY_DEVICE_SMOKE
    std::atomic<bool> _smoke_shake_requested{false};
#endif
};

}
