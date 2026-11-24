#pragma once

#include <cstdint>

namespace desk_buddy {

struct LuckyColor {
    const char *name;
    uint32_t rgb_hex;
};

struct DailyFortune {
    bool time_valid;
    int year;
    int month;
    int day;
    int slacking_stars;      // 1..5
    const char *level_title;
    LuckyColor lucky_color;
    const char *yi_text;
    const char *ji_text;
    const char *quote;
};

class FortuneEngine {
public:
    /** Valid dates produce stable results. Invalid dates use fallback_seed and return zeroed date fields. */
    static DailyFortune generate(int year, int month, int day, bool time_valid, uint32_t fallback_seed = 0);

    static int getYiPoolSize();
    static int getJiPoolSize();
    static int getQuotePoolSize();
    static int getColorPoolSize();
};

}
