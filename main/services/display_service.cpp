#include "services/display_service.hpp"

#include <algorithm>
#include <atomic>
#include <iterator>

#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_adapter.h"
#include "esp_memory_utils.h"
#include "esp_log.h"

namespace desk_buddy {

namespace {

constexpr const char *TAG = "DB:Display";
constexpr int kDrawLines = 30;
constexpr size_t kPixelBytes = BSP_LCD_BITS_PER_PIXEL / 8;
constexpr size_t kDmaBytes = BSP_LCD_H_RES * kDrawLines * kPixelBytes;
constexpr size_t kFallbackDrawBytes = BSP_LCD_H_RES * ((kDrawLines / 2) & ~1) * kPixelBytes;
constexpr size_t kAnimationDmaBytes = 170 * 170 * kPixelBytes;
constexpr size_t kFrameBytes = BSP_LCD_H_RES * BSP_LCD_V_RES * kPixelBytes;
constexpr size_t kDmaAlignment = 64;
// CO5300 datasheet section 6.4.1 requires a write clock period of at least 20 ns.
constexpr int kPanelClockHz = 40 * 1000 * 1000;
lv_draw_buf_t psram_buffer{};

struct TransferState {
    esp_lcd_panel_io_handle_t io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    lv_display_t *display = nullptr;
    lv_draw_buf_t *dma_buffer = nullptr;
    void *staging[2]{};
    void *animation_buffer = nullptr;
    lv_area_t animation_area{0, 0, -1, -1};
    std::atomic<int> pending_transfers{0};
};

TransferState transfer;

#ifdef DESK_BUDDY_DEVICE_SMOKE
unsigned panel_writes = 0;
#endif

void alignPanelArea(lv_area_t *area, void *)
{
    // Match the Waveshare BSP's CO5300 rounding to two-pixel boundaries.
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

// CO5300 register sequence from waveshare/esp32_s3_touch_amoled_2_16 (Apache-2.0).
const co5300_lcd_init_cmd_t kPanelInitCommands[] = {
    {0x11, "\x00", 0, 600},
    {0xFE, "\x20", 1, 0},
    {0x19, "\x10", 1, 0},
    {0x1C, "\xA0", 1, 0},
    {0xFE, "\x00", 1, 0},
    {0xC4, "\x80", 1, 0},
    {0x3A, "\x55", 1, 0},
    {0x35, "\x00", 1, 0},
    {0x53, "\x20", 1, 0},
    {0x51, "\x00", 1, 0},
    {0x63, "\xFF", 1, 0},
    {0x2A, "\x00\x00\x01\xDF", 4, 0},
    {0x2B, "\x00\x00\x01\xDF", 4, 0},
    {0x36, "\x00", 1, 0},
};

esp_err_t createPanel(esp_lcd_panel_handle_t &panel, esp_lcd_panel_io_handle_t &io)
{
    spi_bus_config_t bus{};
    bus.data0_io_num = BSP_LCD_DATA0;
    bus.data1_io_num = BSP_LCD_DATA1;
    bus.sclk_io_num = BSP_LCD_PCLK;
    bus.data2_io_num = BSP_LCD_DATA2;
    bus.data3_io_num = BSP_LCD_DATA3;
    bus.data4_io_num = -1;
    bus.data5_io_num = -1;
    bus.data6_io_num = -1;
    bus.data7_io_num = -1;
    bus.max_transfer_sz = kAnimationDmaBytes;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &bus, SPI_DMA_CH_AUTO), TAG, "QSPI bus");

    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = BSP_LCD_CS;
    io_config.dc_gpio_num = GPIO_NUM_NC;
    io_config.pclk_hz = kPanelClockHz;
    io_config.trans_queue_depth = 3;
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(BSP_LCD_SPI_NUM),
                                                 &io_config, &io), TAG, "panel IO");

    co5300_vendor_config_t vendor{};
    vendor.init_cmds = kPanelInitCommands;
    vendor.init_cmds_size = std::size(kPanelInitCommands);
    vendor.flags.use_qspi_interface = 1;
    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = BSP_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = BSP_LCD_BITS_PER_PIXEL;
    panel_config.vendor_config = &vendor;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(io, &panel_config, &panel), TAG, "CO5300 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset");
    return esp_lcd_panel_init(panel);
}

esp_err_t waitForQueuedPanelTransfers(esp_lcd_panel_io_handle_t io)
{
    // ESP-IDF drains queued color transfers even when command -1 skips command transmission.
    return esp_lcd_panel_io_tx_param(io, -1, nullptr, 0);
}

bool installPsramBuffer(lv_display_t *display)
{
    auto *memory = heap_caps_aligned_alloc(kDmaAlignment, kFrameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) {
        return false;
    }

    if (lv_draw_buf_init(&psram_buffer, BSP_LCD_H_RES, BSP_LCD_V_RES, LV_COLOR_FORMAT_RGB565,
                         LV_STRIDE_AUTO, memory, kFrameBytes) != LV_RESULT_OK) {
        heap_caps_free(memory);
        psram_buffer = {};
        return false;
    }

    lv_display_set_draw_buffers(display, &psram_buffer, nullptr);
    return true;
}

bool IRAM_ATTR onColorTransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *context)
{
    auto &state = *static_cast<TransferState *>(context);
    return state.pending_transfers.fetch_sub(1, std::memory_order_acq_rel) == 1 &&
           esp_lv_adapter_display_notify_color_trans_done_from_isr(state.display);
}

esp_err_t writePanelRegion(esp_lcd_panel_handle_t panel, int x1, int y1, int x2, int y2, const void *pixels)
{
#ifdef DESK_BUDDY_DEVICE_SMOKE
    ++panel_writes;
#endif
    return esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, pixels);
}

void copyRotatedPanelRows(uint16_t *target, const uint16_t *source, int source_width,
                          int panel_width, int panel_rows)
{
    static_assert(sizeof(*source) == kPixelBytes);
    for (int panel_x = 0; panel_x < panel_width; ++panel_x) {
        const auto *source_row = source + panel_x * source_width;
        for (int panel_y = 0; panel_y < panel_rows; ++panel_y) {
            target[panel_y * panel_width + panel_x] = source_row[panel_rows - 1 - panel_y];
        }
    }
}

esp_err_t drawStagedRegion(lv_display_t *, esp_lcd_panel_handle_t panel, int x1, int y1, int x2, int y2,
                          const void *pixels, void *context)
{
    if (x1 < 0 || y1 < 0 || x2 > BSP_LCD_H_RES || y2 > BSP_LCD_V_RES ||
        x2 <= x1 || y2 <= y1 || ((x1 | y1 | x2 | y2) & 1) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    auto &state = *static_cast<TransferState *>(context);
    const auto *source = static_cast<const uint16_t *>(pixels);
    const int source_width = x2 - x1;
    const int panel_width = y2 - y1;
    const bool use_psram = esp_ptr_external_ram(pixels);
    const auto &region = state.animation_area;
    const bool use_animation_buffer = use_psram && state.animation_buffer &&
        x1 >= region.x1 && x2 - 1 <= region.x2 && y1 >= region.y1 && y2 - 1 <= region.y2;
    const int rows_per_slice = (!use_psram || use_animation_buffer) ? source_width :
        static_cast<int>(kDmaBytes / (panel_width * kPixelBytes)) & ~1;
    const int slices = (source_width + rows_per_slice - 1) / rows_per_slice;
    state.pending_transfers.store(slices, std::memory_order_release);

    for (int slice = 0, right = x2; right > x1; ++slice) {
        const int left = std::max(right - rows_per_slice, x1);
        auto *staging = use_animation_buffer ? state.animation_buffer : state.staging[use_psram ? slice & 1 : 1];
        copyRotatedPanelRows(static_cast<uint16_t *>(staging), source + (left - x1), source_width,
                             panel_width, right - left);

        // ESP-IDF drains queued DMA before the CO5300 window commands, so the next staging buffer is free.
        const auto result = writePanelRegion(panel, y1, BSP_LCD_H_RES - right, y2, BSP_LCD_H_RES - left, staging);
        if (result != ESP_OK) {
            waitForQueuedPanelTransfers(state.io);
            state.pending_transfers.store(0, std::memory_order_release);
            return result;
        }
        right = left;
    }
    return ESP_OK;
}

}

namespace {
esp_err_t writeDisplayBrightness(uint8_t percent)
{
    constexpr int kQspiWriteBrightness = 0x02 << 24 | 0x51 << 8;
    const uint8_t level = std::min<uint8_t>(percent, 100) * 255 / 100;
    return transfer.io ? esp_lcd_panel_io_tx_param(transfer.io, kQspiWriteBrightness, &level, 1)
                       : ESP_ERR_INVALID_STATE;
}
}

void setDisplayBrightness(uint8_t percent)
{
    if (writeDisplayBrightness(percent) != ESP_OK) {
        ESP_LOGW(TAG, "Unable to set panel brightness to %u%%", percent);
    }
}

esp_err_t setDisplayPower(bool on)
{
    ESP_RETURN_ON_FALSE(transfer.panel && transfer.io, ESP_ERR_INVALID_STATE, TAG, "missing panel");
    if (on) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_sleep(transfer.panel, false), TAG, "CO5300 Sleep Out");
        return esp_lcd_panel_disp_on_off(transfer.panel, true);
    }
    ESP_RETURN_ON_ERROR(waitForQueuedPanelTransfers(transfer.io), TAG, "pending pixel transfers");
    ESP_RETURN_ON_ERROR(writeDisplayBrightness(0), TAG, "CO5300 brightness");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(transfer.panel, false), TAG, "CO5300 display off");
    return esp_lcd_panel_disp_sleep(transfer.panel, true);
}

void setDisplayAnimationRegion(const lv_area_t *area)
{
    if (transfer.animation_buffer) {
        transfer.animation_area = {0, 0, -1, -1};
        const auto result = waitForQueuedPanelTransfers(transfer.io);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Unable to release animation DMA buffer: %s", esp_err_to_name(result));
            return;
        }
        heap_caps_free(transfer.animation_buffer);
        transfer.animation_buffer = nullptr;
    }
    if (!area || !transfer.display || !esp_ptr_external_ram(lv_display_get_buf_active(transfer.display)->data)) {
        return;
    }

    lv_area_t region = *area;
    alignPanelArea(&region, nullptr);
    const auto bytes = static_cast<size_t>(lv_area_get_size(&region)) * kPixelBytes;
    if (bytes == 0 || bytes > kAnimationDmaBytes) {
        return;
    }

    transfer.animation_buffer = heap_caps_aligned_alloc(kDmaAlignment, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!transfer.animation_buffer) {
        ESP_LOGW(TAG, "Unable to reserve %zu bytes for contiguous animation transfers", bytes);
        return;
    }
    transfer.animation_area = region;
}

lv_display_t *startDisplay(lv_indev_t *&touch_input)
{
    esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter.task_stack_size = 12 * 1024;
    if (esp_lv_adapter_init(&adapter) != ESP_OK) {
        return nullptr;
    }

    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t io = nullptr;
    if (createPanel(panel, io) != ESP_OK) {
        return nullptr;
    }
    if (esp_lv_adapter_set_default_display_idf_callback_registration_enabled(false) != ESP_OK) {
        return nullptr;
    }

    esp_lv_adapter_display_config_t profile = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
        panel, io, BSP_LCD_H_RES, BSP_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    profile.profile.buffer_height = kDrawLines;
    profile.profile.require_double_buffer = false;
    auto *display = esp_lv_adapter_register_display(&profile);
    auto *buffer = display ? lv_display_get_buf_active(display) : nullptr;
    if (!buffer || buffer->data_size < kDmaBytes || !esp_ptr_dma_capable(buffer->data)) {
        ESP_LOGE(TAG, "Unable to allocate the internal DMA buffer");
        return nullptr;
    }
    if (esp_lv_adapter_set_area_rounder_cb(display, alignPanelArea, nullptr) != ESP_OK) {
        return nullptr;
    }

    transfer.io = io;
    transfer.panel = panel;
    transfer.display = display;
    transfer.dma_buffer = buffer;
    const esp_lcd_panel_io_callbacks_t io_callbacks{.on_color_trans_done = onColorTransferDone};
    const esp_lv_adapter_draw_bitmap_callbacks_t draw_callbacks{.custom_draw_bitmap = drawStagedRegion};
    if (esp_lcd_panel_io_register_event_callbacks(io, &io_callbacks, &transfer) != ESP_OK ||
        esp_lv_adapter_set_draw_bitmap_callbacks(display, &draw_callbacks, &transfer) != ESP_OK) {
        return nullptr;
    }

    transfer.staging[0] = buffer->data;
    transfer.staging[1] = heap_caps_aligned_alloc(kDmaAlignment, kDmaBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    const bool use_psram = transfer.staging[1] && installPsramBuffer(display);
    if (!transfer.staging[1]) {
        buffer->data_size = kFallbackDrawBytes;
        transfer.staging[1] = buffer->data + kFallbackDrawBytes;
    }

    bsp_display_cfg_t touch_config{};
    touch_config.touch_flags.swap_xy = true;
    touch_config.touch_flags.mirror_y = true;
    esp_lcd_touch_handle_t touch = nullptr;
    if (bsp_touch_new(&touch_config, &touch) != ESP_OK) {
        return nullptr;
    }
    const esp_lv_adapter_touch_config_t input = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
    touch_input = esp_lv_adapter_register_touch(&input);
    if (!touch_input || esp_lv_adapter_start() != ESP_OK) {
        return nullptr;
    }
    if (use_psram) {
        ESP_LOGI(TAG, "AMOLED ready at %d MHz QSPI with a %zu-byte PSRAM draw buffer and two SRAM staging buffers (%d lines)",
                 kPanelClockHz / 1000000, kFrameBytes, kDrawLines);
    } else {
        ESP_LOGW(TAG, "PSRAM draw buffer unavailable; AMOLED ready with an internal draw buffer and staging");
    }
    return display;
}

#ifdef DESK_BUDDY_DEVICE_SMOKE
unsigned displayBenchmarkPanelWrites()
{
    return panel_writes;
}

bool waitForDisplayBenchmarkTransfer()
{
    return waitForQueuedPanelTransfers(transfer.io) == ESP_OK;
}

bool setDisplayBenchmarkBuffer(lv_display_t *display, bool use_psram)
{
    if (!waitForDisplayBenchmarkTransfer() || (use_psram && !psram_buffer.data)) {
        return false;
    }
    lv_display_set_draw_buffers(display, use_psram ? &psram_buffer : transfer.dma_buffer, nullptr);
    return true;
}
#endif

}
