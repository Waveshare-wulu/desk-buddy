#include "wooden_fish_app.hpp"

#include <cstdio>
#include "services/sound_service.hpp"
#include "services/storage_service.hpp"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "ui/assets/db_icons.h"
#include "ui/assets/db_woodfish_art.h"
#include "ui/i18n/db_i18n.hpp"
#include "ui/theme/db_theme.hpp"

namespace desk_buddy {

namespace {
constexpr const char *kFlavorTexts[4] = {
    "功德 +1",
    "Bug -1",
    "焦虑 -1",
    "心态 +1",
};
constexpr const char *kFlavorNames[4] = {
    "模式：功德 +1",
    "模式：Bug -1",
    "模式：焦虑 -1",
    "模式：心态 +1",
};

constexpr int kFloatRise = 20;
constexpr int kStageHeight = 226;
constexpr int kFishStageX = (theme::kContentWidth - 224) / 2 - 3;
constexpr int kFishStageY = (kStageHeight - 172) / 2 + 20;
constexpr int kMalletStageX = (theme::kContentWidth - 132) / 2 + 62;
constexpr int kMalletStageY = 3;
constexpr int kMalletPivotX = 123;
constexpr int kMalletPivotY = 13;
constexpr int kMalletBoxX = 220;
constexpr int kMalletBoxY = 6;
constexpr int kMalletBoxW = 142;
constexpr int kMalletBoxH = 90;

void animFloatLabel(void *obj, int32_t v)
{
    auto *label = static_cast<lv_obj_t *>(obj);
    lv_obj_set_style_translate_y(label, -kFloatRise * v / 255, 0);
    lv_obj_set_style_text_opa(label, static_cast<lv_opa_t>(v < 128 ? 255 : (255 - v) * 2), 0);
}

}

WoodenFishApp::WoodenFishApp()
    : DeskBuddyApp("deskbuddy.wooden_fish", i18n::tr(i18n::StrId::APP_WOODEN_FISH), &db_icon_wooden_fish, 1)
{
}

bool WoodenFishApp::run(void)
{
    _session_count = 0;
    buildUi();
    _checkpoint_timer = lv_timer_create(onCheckpointTimer, 15000, this);
    return true;
}

bool WoodenFishApp::pause(void)
{
    _mallet_knock.stop();
    StorageService::instance().flushMeritIfNeeded(true);
    return DeskBuddyApp::pause();
}

bool WoodenFishApp::close(void)
{
    stopCheckpointTimer();
    StorageService::instance().flushMeritIfNeeded(true);
    _mallet_knock.detach();
    if (_float_lbl) lv_anim_delete(_float_lbl, nullptr);
    clearMalletFrames();
    _mallet = nullptr;
    _float_lbl = nullptr;
    return DeskBuddyApp::close();
}

void WoodenFishApp::stopCheckpointTimer()
{
    if (_checkpoint_timer) {
        lv_timer_delete(_checkpoint_timer);
        _checkpoint_timer = nullptr;
    }
}

void WoodenFishApp::buildUi()
{
    lv_obj_t *scr = appRoot();
    theme::styleRootScreen(scr);

    auto top = theme::createAppTopBar(
        scr,
        i18n::tr(i18n::StrId::APP_WOODEN_FISH),
        theme::kAccentAmberFg,
        DeskBuddyApp::onBackBtnClicked,
        this,
        SoundService::instance().isEnabled() ? "音效开" : "已静音",
        onSoundToggle
    );
    lv_obj_set_width(top.action_btn, 104);
    _sound_btn_lbl = top.action_label;

    lv_obj_t *stats_row = lv_obj_create(scr);
    lv_obj_remove_style_all(stats_row);
    lv_obj_set_size(stats_row, theme::kContentWidth, 66);
    lv_obj_align(stats_row, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_opa(stats_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_row, 0, 0);
    lv_obj_set_style_pad_all(stats_row, 0, 0);
    lv_obj_set_flex_flow(stats_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(stats_row, LV_OBJ_FLAG_SCROLLABLE);

    auto makeStatCard = [&](const char *title, uint32_t val_color, lv_obj_t **out_lbl) {
        lv_obj_t *c = theme::createCard(stats_row, theme::kColumnWidth, 66, 20);
        lv_obj_set_style_pad_hor(c, 16, 0);
        lv_obj_set_style_pad_ver(c, 8, 0);

        lv_obj_t *t = lv_label_create(c);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_font(t, theme::fontSm(), 0);
        lv_obj_set_style_text_color(t, theme::color(theme::kTextSecondary), 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *v = lv_label_create(c);
        lv_label_set_text(v, "0");
        lv_obj_set_style_text_font(v, theme::fontLg(), 0);
        lv_obj_set_style_text_color(v, theme::color(val_color), 0);
        lv_obj_align(v, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        *out_lbl = v;
    };

    makeStatCard("本次敲击", theme::kAccentAmberFg, &_session_lbl);
    makeStatCard("累计功德", theme::kAccentMintFg, &_total_lbl);

    lv_obj_t *stage = lv_obj_create(scr);
    lv_obj_remove_style_all(stage);
    lv_obj_set_size(stage, theme::kContentWidth, kStageHeight);
    lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stage, 0, 0);
    lv_obj_set_style_pad_all(stage, 0, 0);
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stage, onFishPressed, LV_EVENT_PRESSED, this);

    auto *fish = lv_image_create(stage);
    lv_image_set_src(fish, &db_art_woodfish_lg);
    lv_obj_set_pos(fish, kFishStageX, kFishStageY);

    buildMalletFrames(stage);
    _mallet_knock.attach(applyMalletAngle, this);

    _float_lbl = lv_label_create(stage);
    lv_label_set_text_static(_float_lbl, kFlavorTexts[_flavor_mode]);
    lv_obj_set_style_text_font(_float_lbl, theme::fontLg(), 0);
    lv_obj_set_style_text_color(_float_lbl, theme::color(theme::kAccentAmberFg), 0);
    lv_obj_set_style_text_opa(_float_lbl, LV_OPA_TRANSP, 0);
    lv_obj_align(_float_lbl, LV_ALIGN_TOP_MID, -104, 24);
    lv_obj_clear_flag(_float_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *bot = lv_obj_create(scr);
    lv_obj_remove_style_all(bot);
    lv_obj_set_size(bot, theme::kContentWidth, 58);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, -theme::kGestureClearance);
    lv_obj_set_style_bg_opa(bot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bot, 0, 0);
    lv_obj_set_style_pad_all(bot, 0, 0);
    lv_obj_set_flex_flow(bot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bot, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mode_btn = theme::createPillButton(bot, kFlavorNames[_flavor_mode], theme::kAccentAmberSoft, theme::kAccentAmberFg, 276, 58, theme::fontMd());
    lv_obj_set_style_radius(mode_btn, 20, 0);
    lv_obj_set_style_border_width(mode_btn, 1, 0);
    lv_obj_set_style_border_color(mode_btn, theme::color(theme::kBorderAmber), 0);
    lv_obj_add_event_cb(mode_btn, onModeToggle, LV_EVENT_CLICKED, this);
    _mode_btn_lbl = lv_obj_get_child(mode_btn, 0);

    lv_obj_t *rst_btn = theme::createPillButton(bot, "清空本次", theme::kBgCard, theme::kTextSecondary, 156, 58, theme::fontMd());
    lv_obj_set_style_radius(rst_btn, 20, 0);
    lv_obj_set_style_border_width(rst_btn, 1, 0);
    lv_obj_set_style_border_color(rst_btn, theme::color(theme::kBorderSubtle), 0);
    lv_obj_add_event_cb(rst_btn, onResetSession, LV_EVENT_CLICKED, this);

    updateCounters();
}

bool WoodenFishApp::onUserButtonPressed()
{
    handleKnock();
    return true;
}

void WoodenFishApp::onSoundEnabledChanged()
{
    updateSoundLabel();
}

void WoodenFishApp::buildMalletFrames(lv_obj_t *stage)
{
    _mallet_frame_idx = 0;

    auto *mallet_box = lv_obj_create(stage);
    lv_obj_remove_style_all(mallet_box);
    lv_obj_set_pos(mallet_box, kMalletBoxX, kMalletBoxY);
    lv_obj_set_size(mallet_box, kMalletBoxW, kMalletBoxH);
    lv_obj_set_style_bg_color(mallet_box, theme::color(theme::kBgCanvas), 0);
    lv_obj_set_style_bg_opa(mallet_box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(mallet_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(mallet_box, LV_OBJ_FLAG_CLICKABLE);

    auto *fish_bg = lv_image_create(mallet_box);
    lv_image_set_src(fish_bg, &db_art_woodfish_lg);
    lv_obj_set_pos(fish_bg, kFishStageX - kMalletBoxX, kFishStageY - kMalletBoxY);

    auto *mallet_tmp = lv_image_create(mallet_box);
    lv_image_set_src(mallet_tmp, &db_art_mallet_lg);
    lv_obj_set_pos(mallet_tmp, kMalletStageX - kMalletBoxX, kMalletStageY - kMalletBoxY);
    lv_image_set_pivot(mallet_tmp, kMalletPivotX, kMalletPivotY);

    lv_obj_update_layout(stage);
    bool ready = true;
    for (size_t i = 0; i < _mallet_frames.size(); ++i) {
        lv_image_set_rotation(mallet_tmp, -static_cast<int32_t>(i) * MalletMotion::kFrameStep);
        _mallet_frames[i] = lv_snapshot_take(mallet_box, LV_COLOR_FORMAT_RGB565);
        if (!_mallet_frames[i]) {
            ready = false;
            break;
        }
    }

    if (ready) {
        lv_obj_delete(fish_bg);
        lv_obj_delete(mallet_tmp);
        _mallet_sprite = lv_image_create(mallet_box);
        lv_obj_set_pos(_mallet_sprite, 0, 0);
        lv_obj_clear_flag(_mallet_sprite, LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_src(_mallet_sprite, _mallet_frames[0]);
        return;
    }

    clearMalletFrames();
    lv_obj_delete(mallet_box);
    _mallet = lv_image_create(stage);
    lv_image_set_src(_mallet, &db_art_mallet_lg);
    lv_obj_set_pos(_mallet, kMalletStageX, kMalletStageY);
    lv_image_set_pivot(_mallet, kMalletPivotX, kMalletPivotY);
}

void WoodenFishApp::clearMalletFrames()
{
    if (_mallet_sprite) {
        lv_obj_delete(_mallet_sprite);
        _mallet_sprite = nullptr;
    }
    for (auto *&frame : _mallet_frames) {
        if (frame) {
            lv_image_cache_drop(frame);
            lv_draw_buf_destroy(frame);
            frame = nullptr;
        }
    }
    _mallet_frame_idx = 0;
}

void WoodenFishApp::applyMalletAngle(void *context, int32_t angle, size_t frame_index)
{
    auto *self = static_cast<WoodenFishApp *>(context);
    if (!self) {
        return;
    }
    if (self->_mallet_sprite && frame_index < self->_mallet_frames.size() && self->_mallet_frames[frame_index]) {
        if (frame_index != self->_mallet_frame_idx) {
            self->_mallet_frame_idx = frame_index;
            lv_image_set_src(self->_mallet_sprite, self->_mallet_frames[frame_index]);
        }
        return;
    }
    if (self->_mallet) {
        lv_image_set_rotation(self->_mallet, angle);
    }
}

void WoodenFishApp::handleKnock()
{
    ++_session_count;
    StorageService::instance().addMerit(1);
    updateCounters();
    SoundService::instance().play(SoundEffect::WOOD_FISH);

    _mallet_knock.trigger();

    lv_anim_t rise;
    lv_anim_init(&rise);
    lv_anim_set_var(&rise, _float_lbl);
    lv_anim_set_exec_cb(&rise, animFloatLabel);
    lv_anim_set_values(&rise, 0, 255);
    lv_anim_set_duration(&rise, 620);
    lv_anim_set_path_cb(&rise, lv_anim_path_ease_out);
    lv_anim_start(&rise);
}

void WoodenFishApp::updateCounters()
{
    char buf[32];
    if (_session_lbl) {
        std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(_session_count));
        lv_label_set_text(_session_lbl, buf);
    }
    if (_total_lbl) {
        std::snprintf(buf, sizeof(buf), "%lu",
                      static_cast<unsigned long>(StorageService::instance().getMeritTotal()));
        lv_label_set_text(_total_lbl, buf);
    }
}

void WoodenFishApp::updateSoundLabel()
{
    if (_sound_btn_lbl) {
        lv_label_set_text(_sound_btn_lbl, SoundService::instance().isEnabled() ? "音效开" : "已静音");
    }
}

void WoodenFishApp::onFishPressed(lv_event_t *e)
{
    auto *self = static_cast<WoodenFishApp *>(lv_event_get_user_data(e));
    if (self) {
        self->handleKnock();
    }
}

void WoodenFishApp::onSoundToggle(lv_event_t *e)
{
    auto *self = static_cast<WoodenFishApp *>(lv_event_get_user_data(e));
    if (!self) return;
    auto settings = StorageService::instance().getSettings();
    settings.sound_enabled = !settings.sound_enabled;
    StorageService::instance().updateSettings(settings);
    SoundService::instance().setEnabled(settings.sound_enabled);
    self->updateSoundLabel();
    if (settings.sound_enabled) {
        SoundService::instance().play(SoundEffect::CLICK);
    }
}

void WoodenFishApp::onModeToggle(lv_event_t *e)
{
    auto *self = static_cast<WoodenFishApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_flavor_mode = (self->_flavor_mode + 1) % 4;
    lv_label_set_text_static(self->_float_lbl, kFlavorTexts[self->_flavor_mode]);
    if (self->_mode_btn_lbl) {
        lv_label_set_text(self->_mode_btn_lbl, kFlavorNames[self->_flavor_mode]);
    }
}

void WoodenFishApp::onResetSession(lv_event_t *e)
{
    auto *self = static_cast<WoodenFishApp *>(lv_event_get_user_data(e));
    if (!self) return;
    SoundService::instance().play(SoundEffect::CLICK);
    self->_session_count = 0;
    self->updateCounters();
}

void WoodenFishApp::onCheckpointTimer(lv_timer_t *t)
{
    (void)t;
    StorageService::instance().flushMeritIfNeeded(false);
}

}
