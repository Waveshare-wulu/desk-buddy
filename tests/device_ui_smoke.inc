constexpr const char *kSmokeTag = "DB:DeviceTest";
constexpr unsigned kSmokeRounds = 30;
constexpr auto kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

void countSmokeFlush(lv_event_t *event)
{
    ++*static_cast<unsigned *>(lv_event_get_user_data(event));
}

template <typename Action>
void runSmokeOnLvgl(Action action)
{
    StaticSemaphore_t semaphore_storage{};
    struct Request {
        Action action;
        SemaphoreHandle_t done;
    } request{action, xSemaphoreCreateBinaryStatic(&semaphore_storage)};

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    const auto scheduled = lv_async_call([](void *argument) {
        auto &pending = *static_cast<Request *>(argument);
        pending.action();
        xSemaphoreGive(pending.done);
    }, &request);
    esp_lv_adapter_unlock();
    ESP_ERROR_CHECK(scheduled == LV_RESULT_OK ? ESP_OK : ESP_ERR_NO_MEM);
    xSemaphoreTake(request.done, portMAX_DELAY);
    vSemaphoreDelete(request.done);
}

bool smokeTap(lv_obj_t *object, lv_event_code_t code = LV_EVENT_CLICKED)
{
    return object && lv_obj_send_event(object, code, nullptr) == LV_RESULT_OK;
}

bool smokeDispatch(core::System &system, QueueHandle_t queue, core::AppId id, bool start)
{
    const auto action = start ? SystemAction::START_APP : SystemAction::STOP_APP;
    SystemCommand command{};
    return xQueueReceive(queue, &command, pdMS_TO_TICKS(500)) == pdTRUE &&
           command.id == id && command.action == action && applyAppCommand(system, command);
}

lv_obj_t *findSmokePager(lv_obj_t *screen)
{
    for (uint32_t index = 0; index < lv_obj_get_child_count(screen); ++index) {
        auto *child = lv_obj_get_child(screen, index);
        if (lv_obj_check_type(child, &lv_tileview_class)) {
            return child;
        }
    }
    return nullptr;
}

lv_obj_t *findSmokeLauncher(lv_obj_t *pager, core::AppId id, unsigned page)
{
    auto *tile = lv_obj_get_child(pager, page);
    auto *grid = tile ? lv_obj_get_child(tile, 0) : nullptr;
    if (!grid) {
        return nullptr;
    }
    for (uint32_t index = 0; index < lv_obj_get_child_count(grid); ++index) {
        auto *cell = lv_obj_get_child(grid, index);
        if (reinterpret_cast<uintptr_t>(lv_obj_get_user_data(cell)) == id) {
            return cell;
        }
    }
    return nullptr;
}

lv_obj_t *openSmokeCoinStage(lv_obj_t *root)
{
    auto *content = lv_obj_get_child(root, 1);
    if (!content || lv_obj_get_child_count(content) != 5) {
        return nullptr;
    }
    smokeTap(lv_obj_get_child(content, 2));
    return lv_obj_get_child_count(content) == 3 ? lv_obj_get_child(content, 0) : nullptr;
}

bool returnToSmokeRandomHub(lv_obj_t *root)
{
    auto *bar = lv_obj_get_child(root, 0);
    auto *content = lv_obj_get_child(root, 1);
    return bar && content && smokeTap(lv_obj_get_child(bar, 0)) && lv_obj_get_child_count(content) == 5;
}

#include "device_display_bench.inc"

bool smokeShiftEditor(lv_obj_t *root, bool edit_end)
{
    auto *content = lv_obj_get_child(root, 1);
    const uint32_t children = lv_obj_get_child_count(root);
    const auto initial = StorageService::instance().getSettings();
    if (!smokeTap(lv_obj_get_child(content, edit_end ? 2 : 1)) ||
        lv_obj_get_child_count(root) != children + 1) {
        return false;
    }
    auto *editor = lv_obj_get_child(root, -1);
    auto *top = lv_obj_get_child(editor, 0);
    auto *tabs = lv_obj_get_child(editor, 1);
    auto *picker = lv_obj_get_child(editor, 2);
    auto *hour = lv_obj_get_child(picker, 2);
    auto *minute = lv_obj_get_child(picker, 3);
    if (!tabs || lv_obj_get_child_count(tabs) != 2 || !hour || !minute ||
        !lv_obj_check_type(hour, &lv_roller_class) || !lv_obj_check_type(minute, &lv_roller_class) ||
        lv_roller_get_selected(hour) != (edit_end ? initial.offwork_hour : initial.work_start_hour) ||
        lv_roller_get_selected(minute) != (edit_end ? initial.offwork_min : initial.work_start_min)) {
        return false;
    }

    auto *presets = lv_obj_get_child(editor, 3);
    if (!smokeTap(lv_obj_get_child(presets, edit_end ? 1 : 3)) || lv_roller_get_selected(minute) != 30) {
        return false;
    }
    smokeTap(lv_obj_get_child(top, edit_end ? 0 : 2));
    const auto &saved = StorageService::instance().getSettings();
    return lv_obj_get_child_count(root) == children &&
           (edit_end ? saved.offwork_hour == initial.offwork_hour && saved.offwork_min == initial.offwork_min
                     : saved.work_start_hour == 9 && saved.work_start_min == 30 &&
                       saved.offwork_hour == 18 && saved.offwork_min == 30);
}

bool smokeManualRtcEditor(lv_obj_t *root)
{
    auto *content = lv_obj_get_child(root, 1);
    const uint32_t children = lv_obj_get_child_count(root);
    if (!smokeTap(lv_obj_get_child(content, 1)) || lv_obj_get_child_count(root) != children + 1) {
        return false;
    }
    auto *editor = lv_obj_get_child(root, -1);
    auto *body = lv_obj_get_child(editor, 0);
    if (!body) {
        return false;
    }
    auto *minute = lv_obj_get_child(body, 4);
    auto *increment = lv_obj_get_child(body, 9);
    auto *label = minute ? lv_obj_get_child(minute, 1) : nullptr;
    auto *step_label = increment ? lv_obj_get_child(increment, 0) : nullptr;
    if (!label || !step_label || std::strcmp(lv_label_get_text(step_label), "+1") != 0) {
        return false;
    }
    const int start = std::atoi(lv_label_get_text(label));
    if (!smokeTap(increment, LV_EVENT_PRESSED) ||
        !smokeTap(increment, LV_EVENT_LONG_PRESSED_REPEAT) ||
        !smokeTap(increment, LV_EVENT_LONG_PRESSED_REPEAT) ||
        !smokeTap(increment) ||
        std::atoi(lv_label_get_text(label)) != (start + 2) % 60) {
        return false;
    }
    smokeTap(lv_obj_get_child(lv_obj_get_child(editor, 1), 0));
    return lv_obj_get_child_count(root) == children;
}

lv_obj_t *findSmokeVisibleLabel(lv_obj_t *root, const char *text)
{
    if (!root || lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) {
        return nullptr;
    }
    if (lv_obj_check_type(root, &lv_label_class) && std::strcmp(lv_label_get_text(root), text) == 0) {
        return root;
    }
    for (uint32_t index = 0; index < lv_obj_get_child_count(root); ++index) {
        if (auto *label = findSmokeVisibleLabel(lv_obj_get_child(root, index), text)) {
            return label;
        }
    }
    return nullptr;
}

bool smokePowerMenu(lv_obj_t *bar)
{
    auto *power = findSmokeVisibleLabel(bar, "电源");
    if (!power || !smokeTap(lv_obj_get_parent(power))) {
        return false;
    }
    auto *top = lv_layer_top();
    auto *cancel = findSmokeVisibleLabel(top, "取消");
    if (!cancel || !findSmokeVisibleLabel(top, "电源选项")) {
        return false;
    }
    smokeTap(lv_obj_get_parent(cancel));
    return !findSmokeVisibleLabel(top, "电源选项") && !findSmokeVisibleLabel(top, "取消");
}

bool smokeButtonGuide(lv_obj_t *bar, lv_obj_t *content)
{
    auto *keys = findSmokeVisibleLabel(bar, "按键");
    if (!keys || !smokeTap(lv_obj_get_parent(keys)) || !findSmokeVisibleLabel(bar, "实体按键") ||
        lv_obj_get_child_count(content) != 3 || !findSmokeVisibleLabel(bar, "‹ 关于")) {
        return false;
    }
    const char *positions[] = {"顶部左键", "顶部中键", "顶部右键"};
    for (unsigned i = 0; i < 3; ++i) {
        if (!findSmokeVisibleLabel(lv_obj_get_child(content, i), positions[i])) {
            return false;
        }
    }
    return smokeTap(lv_obj_get_child(bar, 0)) && findSmokeVisibleLabel(bar, "关于与重置") &&
           findSmokeVisibleLabel(bar, "按键") && lv_obj_get_child_count(content) == 6;
}

bool smokeAppAction(lv_obj_t *root, size_t slot, unsigned round)
{
    switch (slot) {
    case 0: {
        auto *stage = openSmokeCoinStage(root);
        if (round != 1) {
            return smokeTap(stage);
        }
        auto &motion = MotionService::instance();
        if (!stage || !motion.isReady()) {
            return false;
        }
        motion.requestShakeOnNextSample();
        return true;
    }
    case 1:
        return smokeTap(lv_obj_get_child(root, 2), LV_EVENT_PRESSED);
    case 3: {
        auto *bar = lv_obj_get_child(root, 0);
        return bar && (round != 1 || smokeTap(lv_obj_get_child(bar, 2)));
    }
    case 4: {
        auto *row = lv_obj_get_child(root, 2);
        return row && smokeTap(lv_obj_get_child(row, 0));
    }
    case 5: {
        auto *content = lv_obj_get_child(root, 1);
        if (!content || lv_obj_get_child_count(content) != 3) {
            return false;
        }
        smokeTap(lv_obj_get_child(content, (round - 1) % 3));
        auto *bar = lv_obj_get_child(root, 0);
        auto *back = lv_obj_get_child(bar, 0);
        auto *back_label = lv_obj_get_child(back, 0);
        return back_label && std::strcmp(lv_label_get_text(back_label), "‹ 列表") == 0 &&
               ((round != 1 && round != 4) || smokeShiftEditor(root, round == 4));
    }
    case 6: {
        auto *bar = lv_obj_get_child(root, 0);
        auto *content = lv_obj_get_child(root, 1);
        if (!bar || !content || lv_obj_get_child_count(content) != 4) {
            return false;
        }
        if (round == 1) {
            const bool passed = smokePowerMenu(bar);
            ESP_LOGI(kSmokeTag, "settings power_menu=%s", passed ? "passed" : "failed");
            if (!passed) {
                return false;
            }
        }
        smokeTap(lv_obj_get_child(content, (round - 1) % 4));
        if (round == 4) {
            const bool passed = smokeButtonGuide(bar, content);
            ESP_LOGI(kSmokeTag, "settings button_guide=%s", passed ? "passed" : "failed");
            if (!passed) {
                return false;
            }
        }
        auto *back = lv_obj_get_child(bar, 0);
        auto *back_label = back ? lv_obj_get_child(back, 0) : nullptr;
        return back_label && std::strcmp(lv_label_get_text(back_label), "‹ 设置") == 0 &&
               (round != 3 || smokeManualRtcEditor(root));
    }
    default:
        return true;
    }
}

bool smokeRunCycle(core::System &system, Desktop &desktop, lv_display_t *display, lv_obj_t *pager,
                   QueueHandle_t queue, core::AppId id, size_t slot, unsigned round, unsigned &flushes)
{
    bool tapped = false;
    runSmokeOnLvgl([&]() {
        lv_display_trigger_activity(display);
        const unsigned page = slot == 6 ? 2 : 1;
        lv_tileview_set_tile_by_index(pager, page, 0, LV_ANIM_OFF);
        tapped = smokeTap(findSmokeLauncher(pager, id, page));
    });
    if (!tapped || !smokeDispatch(system, queue, id, true)) {
        return false;
    }

    auto *layer = desktop.appLayer();
    lv_obj_t *root = nullptr;
    uint32_t merit_before = 0;
    unsigned before = 0;
    bool acted = false;
    runSmokeOnLvgl([&]() {
        root = lv_obj_get_child(layer, 0);
        merit_before = StorageService::instance().getMeritTotal();
        acted = lv_obj_get_child_count(layer) == 1 && root && smokeAppAction(root, slot, round);
        before = flushes;
        if (root) {
            lv_obj_invalidate(root);
        }
    });

    if (slot == 1 && round == 1 && acted) {
        for (unsigned tap = 1; tap < 50; ++tap) {
            vTaskDelay(pdMS_TO_TICKS(24));
            runSmokeOnLvgl([&]() { smokeAppAction(root, slot, round); });
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(slot == 0 ? 1000 : 80));
    }

    bool verified = false;
    bool drew = false;
    bool back = false;
    unsigned before_back = 0;
    runSmokeOnLvgl([&]() {
        drew = flushes > before;
        verified = acted;
        if (slot == 0 && acted) {
            auto *stats = lv_obj_get_child(lv_obj_get_child(root, 1), 1);
            verified = stats && std::strncmp(lv_label_get_text(stats), "结果", sizeof("结果") - 1) == 0;
            if (round == 1) {
                ESP_LOGI(kSmokeTag, "random_lab shake_dispatch=%s", verified ? "passed" : "failed");
            }
            verified = returnToSmokeRandomHub(root) && verified;
        } else if ((slot == 5 || slot == 6) && acted) {
            auto *bar = lv_obj_get_child(root, 0);
            auto *content = lv_obj_get_child(root, 1);
            auto *back_button = lv_obj_get_child(bar, 0);
            auto *back_label = lv_obj_get_child(back_button, 0);
            verified = smokeTap(back_button) && back_label &&
                       std::strcmp(lv_label_get_text(back_label), "‹ 返回") == 0 &&
                       lv_obj_get_child_count(content) == (slot == 5 ? 3u : 4u);
        } else if (slot == 1) {
            const uint32_t count = StorageService::instance().getMeritTotal() - merit_before;
            verified = acted && count == (round == 1 ? 50u : 1u);
            if (round == 1) {
                ESP_LOGI(kSmokeTag, "wooden_fish taps=%lu expected=50", static_cast<unsigned long>(count));
            }
        }
        before_back = flushes;
        auto *bar = root ? lv_obj_get_child(root, 0) : nullptr;
        back = bar && smokeTap(lv_obj_get_child(bar, 0));
    });
    const bool stopped = back && smokeDispatch(system, queue, id, false);

    vTaskDelay(pdMS_TO_TICKS(80));
    bool cleared = false;
    runSmokeOnLvgl([&]() { cleared = lv_obj_get_child_count(layer) == 0 && flushes > before_back; });
    return acted && drew && verified && stopped && cleared;
}

void runDeviceSmoke(core::System &system, Desktop &desktop, lv_display_t *display, lv_indev_t *touch,
                    QueueHandle_t queue, const std::array<core::AppId, 7> &ids)
{
    if (esp_reset_reason() == ESP_RST_PANIC) {
        ESP_LOGE(kSmokeTag, "SKIP after panic");
        return;
    }
    unsigned flushes = 0;
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    lv_indev_enable(touch, false);
    auto *pager = findSmokePager(lv_display_get_screen_active(display));
    const bool onboarding_suppressed = !findSmokeVisibleLabel(lv_layer_top(), "顶部实体按键");
    lv_display_add_event_cb(display, countSmokeFlush, LV_EVENT_FLUSH_FINISH, &flushes);
    esp_lv_adapter_unlock();
    ESP_LOGI(kSmokeTag, "BEGIN rounds=%u apps=%u internal=%zu largest=%zu", kSmokeRounds,
             static_cast<unsigned>(ids.size()), heap_caps_get_free_size(kInternalCaps),
             heap_caps_get_largest_free_block(kInternalCaps));

    unsigned passed = 0;
    const bool sound_muted = !SoundService::instance().isEnabled();
    ESP_LOGI(kSmokeTag, "onboarding_suppressed=%d sound_muted=%d", onboarding_suppressed, sound_muted);
    bool ok = onboarding_suppressed && sound_muted && pager && runDisplayBenchmark(system, desktop, display, pager, queue, ids[0]);
    const bool measured = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
    ESP_LOGI(kSmokeTag, "ACTIVE internal=%zu largest=%zu", heap_caps_get_free_size(kInternalCaps),
             heap_caps_get_largest_free_block(kInternalCaps));
    for (unsigned round = 1; ok && round <= kSmokeRounds; ++round) {
        for (size_t slot = 0; slot < ids.size(); ++slot) {
            const auto id = ids[slot];
            ESP_LOGI(kSmokeTag, "OPEN round=%u app=%lu", round, static_cast<unsigned long>(id));
            if (!smokeRunCycle(system, desktop, display, pager, queue, id, slot, round, flushes)) {
                ESP_LOGE(kSmokeTag, "FAIL round=%u app=%lu", round, static_cast<unsigned long>(id));
                ok = false;
                break;
            }
            ++passed;
        }
        ESP_LOGI(kSmokeTag, "ROUND %u passed=%u internal=%zu largest=%zu", round, passed,
                 heap_caps_get_free_size(kInternalCaps), heap_caps_get_largest_free_block(kInternalCaps));
    }

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    lv_display_remove_event_cb_with_user_data(display, countSmokeFlush, &flushes);
    if (pager) {
        lv_tileview_set_tile_by_index(pager, 1, 0, LV_ANIM_OFF);
    }
    lv_indev_enable(touch, true);
    esp_lv_adapter_unlock();
    const auto minimum = heap_caps_get_minimum_free_size(kInternalCaps);
    if (measured) {
        heap_caps_monitor_local_minimum_free_size_stop();
    }
    ESP_LOGI(kSmokeTag, "END passed=%u expected=%u min_internal=%zu flushes=%u", passed,
             kSmokeRounds * static_cast<unsigned>(ids.size()), minimum, flushes);
}
