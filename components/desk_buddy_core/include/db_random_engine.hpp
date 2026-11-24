#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace desk_buddy {

enum class FoodCategory : uint8_t {
    ALL = 0,
    CHINESE_MEAL,
    NOODLES,
    FAST_FOOD,
    SNACK,
};

struct FoodItem {
    const char *name;
    const char *tag;
    FoodCategory category;
};

struct CoinFlipResult {
    bool is_heads;
    const char *label;
};

struct DiceRollResult {
    int count;
    int values[3];
    int total;
};

struct RandomRangeResult {
    bool valid;
    int32_t value;
    int32_t clamped_min;
    int32_t clamped_max;
};

class RandomEngine {
public:
    using RawRngFn = std::function<uint32_t()>;

    explicit RandomEngine(RawRngFn rng_fn = nullptr);

    /** Unbiased uniform integer in [min_val, max_val]. Swaps if min_val > max_val. */
    int32_t uniformInt(int32_t min_val, int32_t max_val);

    /** Custom food references remain valid until setCustomFoods() or destruction. */
    const FoodItem &pickFood(FoodCategory category, int *last_index_io = nullptr);
    size_t getFoodCount(FoodCategory category) const;
    void setCustomFoods(std::vector<std::string> custom_names);

    /** Pass the previous index to avoid an immediate repeat. The selected index is written back. */
    const char *pickAnswer(int *last_index_io = nullptr);
    size_t getAnswerCount() const;

    CoinFlipResult flipCoin();

    /** Clamps dice_count to [1, 3]. Only the first count entries contain rolls. */
    DiceRollResult rollDice(int dice_count);

    /** Clamps both bounds to [-99999, 99999]. A reversed clamped range returns valid=false. */
    RandomRangeResult pickNumberInRange(int32_t min_val, int32_t max_val);

    const char *pickMotto(int *last_index_io = nullptr);
    size_t getMottoCount() const;

private:
    uint32_t nextU32();
    uint32_t uniformBound(uint32_t bound);

    RawRngFn _rng_fn;
    uint32_t _fallback_state;
    std::vector<std::string> _custom_food_strings;
    std::vector<FoodItem> _custom_food_items;
};

}
