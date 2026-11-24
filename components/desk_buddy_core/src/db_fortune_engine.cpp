#include "db_fortune_engine.hpp"

namespace desk_buddy {

namespace {

constexpr LuckyColor kLuckyColors[] = {
    {"薄荷青", 0x2EE6C5},
    {"落日橘", 0xFF7A45},
    {"松石绿", 0x26C281},
    {"晴空蓝", 0x38BDF8},
    {"香芋紫", 0xA78BFA},
    {"琥珀黄", 0xFBBF24},
    {"珊瑚粉", 0xFB7185},
    {"青柠绿", 0xA3E635},
    {"冰川青", 0x22D3EE},
    {"蜜桃粉", 0xF472B6},
    {"鎏金色", 0xF59E0B},
    {"电光蓝", 0x60A5FA},
    {"赤霞红", 0xF87171},
    {"抹茶绿", 0x4ADE80},
    {"星云紫", 0xC084FC},
    {"奶霜白", 0xE2E8F0},
};

constexpr const char *kYiPool[] = {
    "准点下班",
    "带薪喝水",
    "点杯少糖奶茶",
    "午休小睡二十分钟",
    "把桌面理顺一点",
    "早点提交代码",
    "夸夸身边的搭子",
    "去窗边远眺一会",
    "吃一顿热乎午饭",
    "听两首轻松歌单",
    "顺手备份重要文件",
    "把大任务拆小再做",
    "果断拒绝无效会议",
    "敲十下电子木鱼",
    "下班去散步吹风",
    "穿最舒服的鞋子",
    "给眼睛放个小假",
    "写点简短清晰的注释",
    "今天提前收工",
    "对自己宽容一点",
    "试试没吃过的新店",
    "喝杯热茶提提神",
    "把待办划掉三项",
    "保持微笑不内耗",
    "早睡半小时",
    "整理浏览器标签页",
    "多走楼梯活动筋骨",
    "随手记录好点子",
    "用快捷键省力气",
    "按时吃早饭",
    "给自己买个小零食",
    "顺其自然不硬扛",
};

constexpr const char *kJiPool[] = {
    "主动揽活",
    "下班前改大架构",
    "空腹喝冰美式",
    "在群里秒回长消息",
    "熬夜刷短视频",
    "忘记按保存键",
    "跟需求文档较真",
    "连开三小时长会",
    "临近收工才查日志",
    "盲目自信不写测试",
    "一边吃饭一边回邮件",
    "久坐不动三小时",
    "替别人的锅买单",
    "过度精神内耗",
    "周五傍晚直接上线",
    "把咖啡当水喝",
    "同时开五十个网页",
    "情绪上头立刻回复",
    "为了小事反复纠结",
    "不看天气出门不带伞",
    "憋着问题硬撑半天",
    "忽略身体发出的困意",
    "临睡前想工作难题",
    "盲目承诺紧迫排期",
    "边走路边低头看屏",
    "跳过午饭硬扛下午",
    "和抬杠的人辩输赢",
    "忘记给设备充好电",
    "把简单事情复杂化",
    "强迫自己保持完美",
    "一口气吃太撑",
    "盯着倒计时干着急",
};

constexpr const char *kQuotePool[] = {
    "今天风向偏顺，适合低调把活干完再漂亮溜走。",
    "与其和自己较劲，不如先喝口水让大脑重启三秒。",
    "今天的隐藏成就：不动声色避开一个大坑。",
    "运势显示你今日手气在线，点外卖大概率不踩雷。",
    "放慢半拍反而更准，别被别人的节奏带偏啦。",
    "功德池水位充沛，遇到小插曲也能轻松化解。",
    "今日宜轻装上阵，把能推到明天的烦恼先放一放。",
    "灵感往往躲在走神的那两分钟里，允许自己喘口气。",
    "今天你的耐心值偏高，适合收尾一件拖了很久的小事。",
    "别怀疑，准点合上电脑那一刻你就是全场主角。",
    "小确幸正在路上，可能是一杯好喝的饮品或提前结束的会。",
    "今天不需要证明什么，安稳度过就已经赢过九成人。",
    "遇到卡壳先站起来活动两分钟，回来一眼就能看出破绽。",
    "今日能量守恒：少生一次气，多攒十分钟好心情。",
    "你的桌面搭子提醒你：坐姿端正，但心态可以躺平。",
    "好消息也许不大，但足够把今天点亮一点点。",
    "别把力气花在焦虑上，留给下班后的自由时间吧。",
    "今天适合做减法，删掉一件没必要的事立刻清爽。",
    "保持一点幽默感，连平平无奇的周三也能过出花样。",
    "你已经在稳步前进，不必每一步都跑得气喘吁吁。",
    "今日防抖模式已开启，小小波动根本晃不动你的好心态。",
    "先照顾好自己的胃，很多难题吃完饭就自动变小了。",
    "今天适合听从直觉，复杂的事交给简单的方法。",
    "如果觉得累了，就让电子木鱼替你念两声清净。",
    "星象说你今天自带护盾，琐碎麻烦通通弹开。",
    "别急着冲刺，把节奏调到最舒服的巡航速度。",
    "今日关键词是松弛感，越从容越容易把事情办漂亮。",
    "把手头最容易的一步先做完，剩下的就会顺理成章。",
    "今天的阳光和晚风都免费，记得抽空去领一份。",
    "摸鱼不是摆烂，是为了下一段冲刺留足电量。",
    "偶尔当一条快乐咸鱼，也比焦糊的卷王香得多。",
    "愿你今日代码零报错，消息少打扰，准点笑哈哈。",
    "把期待值放轻松，生活反而容易塞给你一颗糖。",
    "今天的好运藏在细节里，多留意身边的小惊喜。",
    "能用两句话说清的事，今天就别写八百字长文。",
    "别和困意对抗太久，小憩片刻效率直接翻倍。",
};

constexpr const char *kStarTitles[5] = {
    "低调潜行鱼",
    "稳健带薪鱼",
    "自在逍遥鱼",
    "锦鲤附体鱼",
    "天选摸鱼王",
};

constexpr int kColorCount = static_cast<int>(sizeof(kLuckyColors) / sizeof(kLuckyColors[0]));
constexpr int kYiCount = static_cast<int>(sizeof(kYiPool) / sizeof(kYiPool[0]));
constexpr int kJiCount = static_cast<int>(sizeof(kJiPool) / sizeof(kJiPool[0]));
constexpr int kQuoteCount = static_cast<int>(sizeof(kQuotePool) / sizeof(kQuotePool[0]));

uint32_t splitmix32(uint32_t x)
{
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}

}

DailyFortune FortuneEngine::generate(int year, int month, int day, bool time_valid, uint32_t fallback_seed)
{
    DailyFortune res{};
    res.time_valid = time_valid;
    res.year = time_valid ? year : 0;
    res.month = time_valid ? month : 0;
    res.day = time_valid ? day : 0;

    uint32_t base_key = 0;
    if (time_valid) {
        base_key = static_cast<uint32_t>(year * 10000 + month * 100 + day) ^ 0x5A3C96E1u;
    } else {
        base_key = (fallback_seed ? fallback_seed : 0x20260101u) ^ 0xA55A1357u;
    }

    const uint32_t h1 = splitmix32(base_key + 1u);
    const uint32_t h2 = splitmix32(base_key + 17u);
    const uint32_t h3 = splitmix32(base_key + 53u);
    const uint32_t h4 = splitmix32(base_key + 97u);
    const uint32_t h5 = splitmix32(base_key + 149u);

    static constexpr int kStarDist[10] = {1, 2, 3, 3, 4, 4, 4, 5, 5, 5};
    res.slacking_stars = kStarDist[h1 % 10u];
    res.level_title = kStarTitles[res.slacking_stars - 1];

    res.lucky_color = kLuckyColors[h2 % static_cast<uint32_t>(kColorCount)];
    res.yi_text = kYiPool[h3 % static_cast<uint32_t>(kYiCount)];
    res.ji_text = kJiPool[h4 % static_cast<uint32_t>(kJiCount)];
    res.quote = kQuotePool[h5 % static_cast<uint32_t>(kQuoteCount)];

    return res;
}

int FortuneEngine::getYiPoolSize() { return kYiCount; }
int FortuneEngine::getJiPoolSize() { return kJiCount; }
int FortuneEngine::getQuotePoolSize() { return kQuoteCount; }
int FortuneEngine::getColorPoolSize() { return kColorCount; }

}
