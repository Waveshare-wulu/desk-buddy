struct DisplayProbe {
    const char *mode;
    const char *scene = nullptr;
    int64_t start_us = 0;
    int64_t first_flush_us = 0;
    unsigned submits = 0;
    unsigned panel_writes_at_start = 0;
    unsigned pixels = 0;
    lv_area_t bounds{};
};

void sampleDisplayRefresh(lv_event_t *event)
{
    auto &probe = *static_cast<DisplayProbe *>(lv_event_get_user_data(event));
    if (!probe.scene) {
        return;
    }

    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        probe.start_us = esp_timer_get_time();
        probe.submits = 0;
        probe.pixels = 0;
        probe.panel_writes_at_start = displayBenchmarkPanelWrites();
        break;
    case LV_EVENT_FLUSH_START: {
        const auto &area = *static_cast<const lv_area_t *>(lv_event_get_param(event));
        if (probe.submits == 0) {
            probe.first_flush_us = esp_timer_get_time();
            probe.bounds = area;
        } else {
            probe.bounds.x1 = LV_MIN(probe.bounds.x1, area.x1);
            probe.bounds.y1 = LV_MIN(probe.bounds.y1, area.y1);
            probe.bounds.x2 = LV_MAX(probe.bounds.x2, area.x2);
            probe.bounds.y2 = LV_MAX(probe.bounds.y2, area.y2);
        }
        ++probe.submits;
        probe.pixels += lv_area_get_size(&area);
        break;
    }
    case LV_EVENT_REFR_READY:
        if (probe.submits != 0 && waitForDisplayBenchmarkTransfer()) {
            const auto end_us = esp_timer_get_time();
            ESP_LOGI("DB:DisplayBench", "%s %s submits=%u pixels=%u bounds=%ld,%ld,%ld,%ld first_to_done_us=%lld total_us=%lld panel_writes=%u",
                     probe.mode, probe.scene, probe.submits, probe.pixels,
                     static_cast<long>(probe.bounds.x1), static_cast<long>(probe.bounds.y1),
                     static_cast<long>(probe.bounds.x2), static_cast<long>(probe.bounds.y2),
                     static_cast<long long>(end_us - probe.first_flush_us),
                     static_cast<long long>(end_us - probe.start_us),
                     displayBenchmarkPanelWrites() - probe.panel_writes_at_start);
        }
        break;
    default:
        break;
    }
}

bool runDisplayBenchmarkMode(core::System &system, Desktop &desktop, lv_obj_t *pager,
                             QueueHandle_t queue, core::AppId random_lab, DisplayProbe &probe)
{
    for (unsigned index = 0; index < 10; ++index) {
        runSmokeOnLvgl([&]() {
            const unsigned page = index % 2;
            probe.scene = page == 0 ? "desktop0" : "desktop1";
            lv_tileview_set_tile_by_index(pager, page, 0, LV_ANIM_OFF);
        });
        vTaskDelay(pdMS_TO_TICKS(240));
    }

    bool launched = false;
    runSmokeOnLvgl([&]() {
        probe.scene = "app_open";
        launched = smokeTap(findSmokeLauncher(pager, random_lab, 1));
    });
    if (!launched || !smokeDispatch(system, queue, random_lab, true)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(240));

    lv_obj_t *root = nullptr;
    lv_obj_t *coin_stage = nullptr;
    runSmokeOnLvgl([&]() {
        probe.scene = "coin_tool";
        root = lv_obj_get_child(desktop.appLayer(), 0);
        if (root) {
            coin_stage = openSmokeCoinStage(root);
        }
    });
    vTaskDelay(pdMS_TO_TICKS(240));

    if (!coin_stage) {
        return false;
    }
    for (unsigned index = 0; index < 4; ++index) {
        runSmokeOnLvgl([&]() {
            probe.scene = "coin_flip";
            smokeTap(coin_stage);
        });
        vTaskDelay(pdMS_TO_TICKS(850));
    }

    bool hub = false;
    runSmokeOnLvgl([&]() {
        probe.scene = nullptr;
        hub = returnToSmokeRandomHub(root);
    });
    vTaskDelay(pdMS_TO_TICKS(240));
    if (!hub) {
        return false;
    }

    bool back = false;
    runSmokeOnLvgl([&]() {
        probe.scene = "app_close";
        auto *bar = lv_obj_get_child(root, 0);
        back = bar && smokeTap(lv_obj_get_child(bar, 0));
    });
    const bool stopped = back && smokeDispatch(system, queue, random_lab, false);
    vTaskDelay(pdMS_TO_TICKS(240));
    return stopped;
}

bool runDisplayBenchmark(core::System &system, Desktop &desktop, lv_display_t *display, lv_obj_t *pager,
                         QueueHandle_t queue, core::AppId random_lab)
{
    for (bool use_psram : {false, true}) {
        DisplayProbe probe{use_psram ? "psram" : "sram"};
        bool active = false;
        runSmokeOnLvgl([&]() {
            active = setDisplayBenchmarkBuffer(display, use_psram);
            if (active) {
                lv_display_add_event_cb(display, sampleDisplayRefresh, LV_EVENT_REFR_START, &probe);
                lv_display_add_event_cb(display, sampleDisplayRefresh, LV_EVENT_FLUSH_START, &probe);
                lv_display_add_event_cb(display, sampleDisplayRefresh, LV_EVENT_REFR_READY, &probe);
                probe.scene = "full_screen";
                lv_obj_invalidate(lv_display_get_screen_active(display));
                lv_refr_now(display);
                probe.scene = nullptr;
            }
        });
        if (!active) {
            ESP_LOGE(kSmokeTag, "FAIL display benchmark buffer=%s", probe.mode);
            return false;
        }

        const bool completed = runDisplayBenchmarkMode(system, desktop, pager, queue, random_lab, probe);
        runSmokeOnLvgl([&]() {
            lv_display_remove_event_cb_with_user_data(display, sampleDisplayRefresh, &probe);
        });
        if (!completed) {
            ESP_LOGE(kSmokeTag, "FAIL display benchmark scene=%s mode=%s", probe.scene, probe.mode);
            return false;
        }
    }
    return true;
}
