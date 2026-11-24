#include "db_random_engine.hpp"

#include <algorithm>
#include <utility>

namespace desk_buddy {

namespace {

constexpr FoodItem kBuiltinFoods[] = {
    {"黄焖鸡米饭", "中餐", FoodCategory::CHINESE_MEAL},
    {"隆江猪脚饭", "中餐", FoodCategory::CHINESE_MEAL},
    {"鱼香肉丝盖饭", "中餐", FoodCategory::CHINESE_MEAL},
    {"宫保鸡丁饭", "中餐", FoodCategory::CHINESE_MEAL},
    {"番茄炒蛋盖饭", "中餐", FoodCategory::CHINESE_MEAL},
    {"辣椒炒肉", "湘菜", FoodCategory::CHINESE_MEAL},
    {"煲仔饭", "粤式", FoodCategory::CHINESE_MEAL},
    {"烧腊双拼饭", "粤式", FoodCategory::CHINESE_MEAL},
    {"水煮肉片", "川菜", FoodCategory::CHINESE_MEAL},
    {"麻婆豆腐拌饭", "川菜", FoodCategory::CHINESE_MEAL},
    {"酸菜鱼", "热菜", FoodCategory::CHINESE_MEAL},
    {"石锅拌饭", "热饭", FoodCategory::CHINESE_MEAL},
    {"咖喱鸡排饭", "简餐", FoodCategory::CHINESE_MEAL},
    {"卤肉饭", "台式", FoodCategory::CHINESE_MEAL},
    {"烤肉拌饭", "热饭", FoodCategory::CHINESE_MEAL},
    {"自选称重麻辣烫", "热汤", FoodCategory::CHINESE_MEAL},
    {"兰州牛肉面", "面食", FoodCategory::NOODLES},
    {"重庆小面", "面食", FoodCategory::NOODLES},
    {"红烧牛肉面", "面食", FoodCategory::NOODLES},
    {"武汉热干面", "拌面", FoodCategory::NOODLES},
    {"葱油拌面", "拌面", FoodCategory::NOODLES},
    {"老北京炸酱面", "面食", FoodCategory::NOODLES},
    {"柳州螺蛳粉", "嗦粉", FoodCategory::NOODLES},
    {"云南过桥米线", "米线", FoodCategory::NOODLES},
    {"桂林卤菜粉", "嗦粉", FoodCategory::NOODLES},
    {"鲜肉小馄饨", "面食", FoodCategory::NOODLES},
    {"手工水饺", "面食", FoodCategory::NOODLES},
    {"油泼扯面", "面食", FoodCategory::NOODLES},
    {"香辣鸡腿堡套餐", "快餐", FoodCategory::FAST_FOOD},
    {"照烧鸡腿饭", "快餐", FoodCategory::FAST_FOOD},
    {"牛肉卷配冰美式", "轻食", FoodCategory::FAST_FOOD},
    {"鸡胸肉能量碗", "轻食", FoodCategory::FAST_FOOD},
    {"金枪鱼三明治", "轻食", FoodCategory::FAST_FOOD},
    {"意式肉酱面", "西式", FoodCategory::FAST_FOOD},
    {"榴莲薄底披萨", "西式", FoodCategory::FAST_FOOD},
    {"鲜虾滑蛋饭", "简餐", FoodCategory::FAST_FOOD},
    {"日式肥牛饭", "简餐", FoodCategory::FAST_FOOD},
    {"寿喜锅单人餐", "简餐", FoodCategory::FAST_FOOD},
    {"肉夹馍配凉皮", "小吃", FoodCategory::SNACK},
    {"杂粮煎饼果子", "小吃", FoodCategory::SNACK},
    {"生煎包配牛肉汤", "小吃", FoodCategory::SNACK},
    {"关东煮全家桶", "小吃", FoodCategory::SNACK},
    {"烤冷面加烤肠", "小吃", FoodCategory::SNACK},
    {"脆皮炸鸡配可乐", "解馋", FoodCategory::SNACK},
    {"铁板鱿鱼配炒粉", "夜宵", FoodCategory::SNACK},
    {"章鱼小丸子", "小吃", FoodCategory::SNACK},
    {"锅贴配小米粥", "小吃", FoodCategory::SNACK},
    {"钵钵鸡配冰粉", "小吃", FoodCategory::SNACK},
};

constexpr const char *kAnswers[] = {
    "闭眼冲，别回头",
    "先喝口水再决定",
    "今天不适合卷",
    "直觉早就告诉你了",
    "拖一拖说不定就好了",
    "可以，但别抱太大期待",
    "换个思路会更顺",
    "大胆点，没人盯着你",
    "再等五分钟看看",
    "不如先吃顿好的",
    "答案是肯定的",
    "先别急着答应",
    "这事稳得像定海神针",
    "听起来不太靠谱",
    "交给明天的自己吧",
    "现在就是最好时机",
    "退一步海阔天空",
    "别问了，去干就完了",
    "小心有隐藏彩蛋",
    "顺其自然反而最香",
    "建议立刻执行",
    "先睡一觉再说",
    "你会感谢今天的克制",
    "试试看，成本很低",
    "把预期调低一半刚刚好",
    "别纠结，选第一个",
    "选第二个更惊喜",
    "先摸三分钟鱼冷静下",
    "找个搭子一起做",
    "低调推进，别声张",
    "放弃也是一种智慧",
    "好运正在派送中",
    "事情会比想象顺利",
    "再确认一遍细节",
    "别让别人替你做主",
    "今天宜躺平，不宜硬刚",
    "勇敢说不，一身轻松",
    "小小放纵一下无妨",
    "坚持一下就看到曙光",
    "这个主意有点东西",
    "先写个最简单的版本",
    "别被完美主义困住",
    "去晒晒太阳吧",
    "大概率会有意外收获",
    "先把手头小事清掉",
    "慢就是快，别慌",
    "别想太多，开心最重要",
    "值得冒一次小险",
    "暂缓行动，静观其变",
    "答案藏在你的下一杯奶茶里",
    "把复杂问题拆成两步",
    "今天适合果断一点",
    "不如抛个硬币决定",
    "别回头看，往前走",
    "这一局你占上风",
    "先保存进度再说",
    "听听反面意见也不亏",
    "放心，问题不大",
    "时机还差一点火候",
    "放手去做，结果不会差",
};

constexpr const char *kMottos[] = {
    "带薪呼吸，也是核心竞争力",
    "工作干不完，但鱼必须按时摸",
    "屏幕亮着，灵魂先放个小假",
    "今日目标：优雅地准点下班",
    "代码可以重构，心情不能崩盘",
    "适当摸鱼，有助于延长职业寿命",
    "先喝杯水，剩下的交给缘分",
    "别卷了，旁边工位都在偷笑",
    "小歇五分钟，灵感自己找上门",
    "电量够用，心态更要满格",
    "只要我不焦虑，DDL 就追不上我",
    "稳住节奏，世界不会因为慢一步塌掉",
    "今天也要做一个快乐的桌面生物",
    "把烦恼敲进木鱼，功德留给自己",
    "人生很多难题，吃顿好的就解了一半",
    "保持微笑，假装一切尽在掌握",
};

constexpr size_t kBuiltinFoodCount = sizeof(kBuiltinFoods) / sizeof(kBuiltinFoods[0]);
constexpr size_t kAnswerCount = sizeof(kAnswers) / sizeof(kAnswers[0]);
constexpr size_t kMottoCount = sizeof(kMottos) / sizeof(kMottos[0]);
constexpr int32_t kNumberMinClamp = -99999;
constexpr int32_t kNumberMaxClamp = 99999;

}

RandomEngine::RandomEngine(RawRngFn rng_fn)
    : _rng_fn(std::move(rng_fn))
    , _fallback_state(0xA341316Cu)
{
}

uint32_t RandomEngine::nextU32()
{
    if (_rng_fn) {
        return _rng_fn();
    }
    uint32_t x = _fallback_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _fallback_state = x ? x : 0x9E3779B9u;
    return _fallback_state;
}

uint32_t RandomEngine::uniformBound(uint32_t bound)
{
    if (bound <= 1) {
        return 0;
    }
    const uint32_t first_unbiased_sample = static_cast<uint32_t>(-bound) % bound;
    while (true) {
        const uint32_t sample = nextU32();
        if (sample >= first_unbiased_sample) {
            return sample % bound;
        }
    }
}

int32_t RandomEngine::uniformInt(int32_t min_val, int32_t max_val)
{
    if (min_val > max_val) {
        std::swap(min_val, max_val);
    }
    const uint64_t span = static_cast<uint64_t>(static_cast<int64_t>(max_val) - static_cast<int64_t>(min_val)) + 1ULL;
    if (span >= 0x100000000ULL) {
        return static_cast<int32_t>(nextU32());
    }
    const uint32_t offset = uniformBound(static_cast<uint32_t>(span));
    return static_cast<int32_t>(static_cast<int64_t>(min_val) + static_cast<int64_t>(offset));
}

void RandomEngine::setCustomFoods(std::vector<std::string> custom_names)
{
    _custom_food_strings = std::move(custom_names);
    _custom_food_items.clear();
    _custom_food_items.reserve(_custom_food_strings.size());
    for (const auto &s : _custom_food_strings) {
        if (!s.empty()) {
            _custom_food_items.push_back(FoodItem{s.c_str(), "自定义", FoodCategory::ALL});
        }
    }
}

size_t RandomEngine::getFoodCount(FoodCategory category) const
{
    if (category == FoodCategory::ALL) {
        return kBuiltinFoodCount + _custom_food_items.size();
    }
    size_t count = 0;
    for (const auto &item : kBuiltinFoods) {
        if (item.category == category) {
            ++count;
        }
    }
    return count;
}

const FoodItem &RandomEngine::pickFood(FoodCategory category, int *last_index_io)
{
    std::vector<const FoodItem *> candidates;
    candidates.reserve(kBuiltinFoodCount + _custom_food_items.size());

    for (const auto &item : kBuiltinFoods) {
        if (category == FoodCategory::ALL || item.category == category) {
            candidates.push_back(&item);
        }
    }
    if (category == FoodCategory::ALL) {
        for (const auto &item : _custom_food_items) {
            candidates.push_back(&item);
        }
    }

    if (candidates.empty()) {
        return kBuiltinFoods[0];
    }

    int idx = static_cast<int>(uniformBound(static_cast<uint32_t>(candidates.size())));
    if (last_index_io && candidates.size() > 1 && idx == *last_index_io) {
        idx = (idx + 1 + static_cast<int>(uniformBound(static_cast<uint32_t>(candidates.size() - 1)))) %
              static_cast<int>(candidates.size());
    }
    if (last_index_io) {
        *last_index_io = idx;
    }
    return *candidates[static_cast<size_t>(idx)];
}

size_t RandomEngine::getAnswerCount() const
{
    return kAnswerCount;
}

const char *RandomEngine::pickAnswer(int *last_index_io)
{
    int idx = static_cast<int>(uniformBound(static_cast<uint32_t>(kAnswerCount)));
    if (last_index_io && kAnswerCount > 1 && idx == *last_index_io) {
        idx = (idx + 1 + static_cast<int>(uniformBound(static_cast<uint32_t>(kAnswerCount - 1)))) %
              static_cast<int>(kAnswerCount);
    }
    if (last_index_io) {
        *last_index_io = idx;
    }
    return kAnswers[idx];
}

CoinFlipResult RandomEngine::flipCoin()
{
    const bool heads = (uniformBound(2) == 0);
    return CoinFlipResult{heads, heads ? "正面" : "反面"};
}

DiceRollResult RandomEngine::rollDice(int dice_count)
{
    DiceRollResult res{};
    res.count = std::clamp(dice_count, 1, 3);
    res.total = 0;
    for (int i = 0; i < res.count; ++i) {
        res.values[i] = uniformInt(1, 6);
        res.total += res.values[i];
    }
    return res;
}

RandomRangeResult RandomEngine::pickNumberInRange(int32_t min_val, int32_t max_val)
{
    RandomRangeResult res{};
    res.clamped_min = std::clamp(min_val, kNumberMinClamp, kNumberMaxClamp);
    res.clamped_max = std::clamp(max_val, kNumberMinClamp, kNumberMaxClamp);
    if (res.clamped_min > res.clamped_max) {
        res.valid = false;
        res.value = res.clamped_min;
        return res;
    }
    res.valid = true;
    res.value = uniformInt(res.clamped_min, res.clamped_max);
    return res;
}

size_t RandomEngine::getMottoCount() const
{
    return kMottoCount;
}

const char *RandomEngine::pickMotto(int *last_index_io)
{
    int idx = static_cast<int>(uniformBound(static_cast<uint32_t>(kMottoCount)));
    if (last_index_io && kMottoCount > 1 && idx == *last_index_io) {
        idx = (idx + 1) % static_cast<int>(kMottoCount);
    }
    if (last_index_io) {
        *last_index_io = idx;
    }
    return kMottos[idx];
}

}
