#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "apps/db_base_app.hpp"
#include "db_mallet_motion.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

class WoodenFishApp : public DeskBuddyApp {
public:
    WoodenFishApp();
    ~WoodenFishApp() override = default;

protected:
    bool run(void) override;
    bool onUserButtonPressed() override;
    void onSoundEnabledChanged() override;
    bool pause(void) override;
    bool close(void) override;

private:
    void buildUi();
    void buildMalletFrames(lv_obj_t *stage);
    void clearMalletFrames();
    void handleKnock();
    void updateCounters();
    void updateSoundLabel();
    void stopCheckpointTimer();

    static void applyMalletAngle(void *context, int32_t angle, size_t frame_index);
    static void onFishPressed(lv_event_t *e);
    static void onSoundToggle(lv_event_t *e);
    static void onModeToggle(lv_event_t *e);
    static void onResetSession(lv_event_t *e);
    static void onCheckpointTimer(lv_timer_t *t);

    uint32_t _session_count = 0;
    uint8_t _flavor_mode = 0;
    size_t _mallet_frame_idx = 0;

    lv_obj_t *_session_lbl = nullptr;
    lv_obj_t *_total_lbl = nullptr;
    lv_obj_t *_mallet = nullptr;
    lv_obj_t *_mallet_sprite = nullptr;
    std::array<lv_draw_buf_t *, MalletMotion::kFrameCount> _mallet_frames{};
    theme::MalletKnockController _mallet_knock{};
    lv_obj_t *_float_lbl = nullptr;
    lv_obj_t *_sound_btn_lbl = nullptr;
    lv_obj_t *_mode_btn_lbl = nullptr;
    lv_timer_t *_checkpoint_timer = nullptr;
};

}
