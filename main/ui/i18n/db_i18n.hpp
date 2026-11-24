#pragma once

#include <cstdint>

namespace desk_buddy::i18n {

enum class Lang : uint8_t {
    ZH_CN = 0,
    EN_US = 1,
};

enum class StrId : uint16_t {
    APP_RANDOM_LAB = 0,
    APP_WOODEN_FISH,
    APP_DAILY_FORTUNE,
    APP_GAME_2048,
    APP_RELATIVE_CALC,
    APP_TIME_TOOLS,
    APP_SETTINGS,

    BTN_BACK,
    BTN_SETTINGS,
    BTN_RESET,
    BTN_START,
    BTN_PAUSE,
    BTN_CONTINUE,
    BTN_UNDO,
    BTN_CLEAR,
    BTN_SAVE,
    BTN_CONFIRM,
    BTN_CANCEL,

    STATUS_TIME_UNCALIBRATED,
    HOME_TAP_REFRESH_MOTTO,
    HOME_SETTINGS_SHORT,
    FORTUNE_ENTERTAINMENT_ONLY,
    SHAKE_OR_TAP_HINT,
    SOUND_ON,
    SOUND_OFF,

    MAX_COUNT
};

void setLanguage(Lang lang);
Lang getLanguage();
const char *tr(StrId id);

}
