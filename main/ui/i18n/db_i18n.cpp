#include "db_i18n.hpp"
#include <cstddef>

namespace desk_buddy::i18n {

namespace {

Lang g_lang = Lang::ZH_CN;

constexpr const char *kStringsZh[static_cast<size_t>(StrId::MAX_COUNT)] = {
    "随机实验室",
    "电子木鱼",
    "今日运势",
    "2048",
    "亲戚计算器",
    "时间工具",
    "系统设置",

    "返回",
    "设置",
    "重置",
    "开始",
    "暂停",
    "继续",
    "撤销",
    "清空",
    "保存",
    "确认",
    "取消",

    "未校时 · 点击设置",
    "点击换一句",
    "设置",
    "仅供娱乐 · 开心摸鱼",
    "点击或摇一摇触发",
    "音效开",
    "已静音",
};

constexpr const char *kStringsEn[static_cast<size_t>(StrId::MAX_COUNT)] = {
    "Random Lab",
    "Wooden Fish",
    "Fortune",
    "2048",
    "Relatives",
    "Time Tools",
    "Settings",

    "Back",
    "Settings",
    "Reset",
    "Start",
    "Pause",
    "Resume",
    "Undo",
    "Clear",
    "Save",
    "Confirm",
    "Cancel",

    "Time Unset",
    "Tap Motto",
    "Setup",
    "For fun only",
    "Tap or Shake",
    "SFX On",
    "Muted",
};

}

void setLanguage(Lang lang)
{
    g_lang = lang;
}

Lang getLanguage()
{
    return g_lang;
}

const char *tr(StrId id)
{
    const auto idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(StrId::MAX_COUNT)) {
        return "";
    }
    return (g_lang == Lang::EN_US) ? kStringsEn[idx] : kStringsZh[idx];
}

}
