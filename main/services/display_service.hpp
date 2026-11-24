#pragma once

#include "esp_err.h"
#include "lvgl.h"

namespace desk_buddy {

lv_display_t *startDisplay(lv_indev_t *&touch_input);

/** Send the brightness command after any queued panel pixel transfers finish. */
void setDisplayBrightness(uint8_t percent);
esp_err_t setDisplayPower(bool on);

/** Reserve a contiguous DMA buffer for local animation updates. Pass nullptr to release it. */
void setDisplayAnimationRegion(const lv_area_t *area);

#ifdef DESK_BUDDY_DEVICE_SMOKE
bool setDisplayBenchmarkBuffer(lv_display_t *display, bool use_psram);
bool waitForDisplayBenchmarkTransfer();
unsigned displayBenchmarkPanelWrites();
#endif

}
