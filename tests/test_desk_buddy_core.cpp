#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "db_button_logic.hpp"
#include "db_fortune_engine.hpp"
#include "db_game_2048_engine.hpp"
#include "db_mallet_motion.hpp"
#include "db_random_engine.hpp"
#include "db_relative_engine.hpp"
#include "db_shake_detector.hpp"
#include "db_time_logic.hpp"

using namespace desk_buddy;

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond, msg)                                                           \
    do {                                                                           \
        ++g_tests_run;                                                             \
        if (!(cond)) {                                                             \
            ++g_tests_failed;                                                      \
            std::cerr << "[FAIL] " << __func__ << ":" << __LINE__ << " - " << msg \
                      << std::endl;                                                \
        }                                                                          \
    } while (0)

static void testRandomEngine()
{
    RandomEngine rng;

    int counts[6] = {0};
    for (int i = 0; i < 6000; ++i) {
        int32_t v = rng.uniformInt(1, 6);
        CHECK(v >= 1 && v <= 6, "uniformInt out of range");
        counts[v - 1]++;
    }
    for (int i = 0; i < 6; ++i) {
        CHECK(counts[i] > 800 && counts[i] < 1200, "uniformInt distribution skewed");
    }

    int last_food = -1;
    const char *prev_name = nullptr;
    for (int i = 0; i < 20; ++i) {
        const auto &item = rng.pickFood(FoodCategory::ALL, &last_food);
        CHECK(item.name != nullptr && std::strlen(item.name) > 0, "empty food");
        if (prev_name != nullptr) {
            CHECK(std::strcmp(prev_name, item.name) != 0, "immediate food repeat");
        }
        prev_name = item.name;
    }

    CHECK(rng.getFoodCount(FoodCategory::NOODLES) >= 10, "noodles pool too small");
    const auto &noodle = rng.pickFood(FoodCategory::NOODLES, nullptr);
    CHECK(noodle.category == FoodCategory::NOODLES, "wrong food category");

    size_t base_all = rng.getFoodCount(FoodCategory::ALL);
    rng.setCustomFoods({"自制蛋炒饭", "烤鸭卷"});
    CHECK(rng.getFoodCount(FoodCategory::ALL) == base_all + 2, "custom food count mismatch");

    int last_ans = -1;
    const char *prev_ans = nullptr;
    for (int i = 0; i < 25; ++i) {
        const char *ans = rng.pickAnswer(&last_ans);
        CHECK(ans != nullptr && std::strlen(ans) > 0, "empty answer");
        if (prev_ans) {
            CHECK(std::strcmp(prev_ans, ans) != 0, "immediate answer repeat");
        }
        prev_ans = ans;
    }

    int heads = 0, tails = 0;
    for (int i = 0; i < 200; ++i) {
        auto c = rng.flipCoin();
        if (c.is_heads) ++heads;
        else ++tails;
    }
    CHECK(heads > 50 && tails > 50, "coin flip biased");

    auto d0 = rng.rollDice(0);
    CHECK(d0.count == 1 && d0.total == d0.values[0], "dice clamp min failed");
    auto d5 = rng.rollDice(5);
    CHECK(d5.count == 3 && d5.total == d5.values[0] + d5.values[1] + d5.values[2], "dice clamp max failed");

    auto bad = rng.pickNumberInRange(50, 10);
    CHECK(!bad.valid, "invalid min > max should fail");
    auto single = rng.pickNumberInRange(42, 42);
    CHECK(single.valid && single.value == 42, "single-point range failed");
    auto clamped = rng.pickNumberInRange(-500000, 500000);
    CHECK(clamped.valid && clamped.clamped_min == -99999 && clamped.clamped_max == 99999, "range clamp failed");
}

static void testFortuneEngine()
{
    auto f1 = FortuneEngine::generate(2026, 11, 20, true);
    auto f2 = FortuneEngine::generate(2026, 11, 20, true);
    CHECK(f1.time_valid && f2.time_valid, "time_valid expected true");
    CHECK(f1.slacking_stars == f2.slacking_stars, "same day stars differ");
    CHECK(std::strcmp(f1.yi_text, f2.yi_text) == 0, "same day yi differs");
    CHECK(std::strcmp(f1.ji_text, f2.ji_text) == 0, "same day ji differs");
    CHECK(std::strcmp(f1.quote, f2.quote) == 0, "same day quote differs");
    CHECK(f1.lucky_color.rgb_hex == f2.lucky_color.rgb_hex, "same day color differs");

    int diff_days = 0;
    for (int d = 1; d <= 15; ++d) {
        auto fd = FortuneEngine::generate(2026, 11, d, true);
        CHECK(fd.slacking_stars >= 1 && fd.slacking_stars <= 5, "stars range invalid");
        if (std::strcmp(fd.quote, f1.quote) != 0) {
            ++diff_days;
        }
    }
    CHECK(diff_days >= 10, "daily fortune does not vary across days");

    auto unknown = FortuneEngine::generate(1970, 1, 1, false, 12345);
    CHECK(!unknown.time_valid && unknown.year == 0, "unknown time must be marked invalid");
    CHECK(unknown.quote != nullptr && unknown.yi_text != nullptr, "unknown time should still provide demo content");
}

static void testGame2048Engine()
{
    Game2048Engine game;
    int init_tiles = 0;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (game.getState().board[r][c] != 0) ++init_tiles;
        }
    }
    CHECK(init_tiles == 2, "initial 2048 board must have 2 tiles");

    const uint16_t b1[4][4] = {
        {2, 2, 2, 2},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    };
    game.setBoardForTest(b1, 0);
    auto r1 = game.move(MoveDir::LEFT);
    CHECK(r1.moved, "move left should succeed");
    CHECK(r1.score_gained == 8, "score gained for 2+2, 2+2 must be 8");
    CHECK(game.getState().board[0][0] == 4 && game.getState().board[0][1] == 4, "merge result incorrect");
    CHECK(r1.merged_mask[0][0] && r1.merged_mask[0][1], "merged mask incorrect");
    CHECK(r1.spawned, "should spawn new tile after move");

    const uint16_t b2[4][4] = {
        {2, 2, 4, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    };
    game.setBoardForTest(b2, 0);
    auto r2 = game.move(MoveDir::LEFT);
    CHECK(r2.score_gained == 4, "double merge bug detected");
    CHECK(game.getState().board[0][0] == 4 && game.getState().board[0][1] == 4, "double merge board bug");

    const uint16_t b3[4][4] = {
        {1024, 1024, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
    };
    game.setBoardForTest(b3, 100);
    auto r3 = game.move(MoveDir::RIGHT);
    CHECK(r3.newly_won && game.getState().won_2048, "2048 win detection failed");
    CHECK(game.getState().score == 2148, "total score mismatch");

    const uint16_t full[4][4] = {
        {2, 4, 2, 4},
        {4, 2, 4, 2},
        {2, 4, 2, 4},
        {4, 2, 4, 2},
    };
    game.setBoardForTest(full, 500);
    CHECK(!game.canMove() && game.getState().game_over, "game over detection failed");
    auto r_no = game.move(MoveDir::UP);
    CHECK(!r_no.moved && !r_no.spawned, "should not move on locked board");

    const uint16_t mergeable[4][4] = {
        {2, 4, 2, 4},
        {4, 2, 4, 2},
        {2, 4, 2, 4},
        {4, 2, 4, 4},
    };
    game.setBoardForTest(mergeable, 500);
    CHECK(game.canMove() && !game.getState().game_over, "false game over on mergeable board");
}

static void testRelativeEngine()
{
    auto expectTitle = [](const std::vector<KinOp> &chain, SelfGender g, const char *expected) {
        auto r = RelativeEngine::evaluate(chain, g);
        CHECK(r.supported && r.primary_title == expected,
              std::string("Expected '") + expected + "' for " + RelativeEngine::formatPath(chain) +
              ", got '" + r.primary_title + "'");
    };

    expectTitle({KinOp::FATHER, KinOp::OLDER_SISTER}, SelfGender::MALE, "姑妈");
    expectTitle({KinOp::MOTHER, KinOp::OLDER_BROTHER}, SelfGender::MALE, "舅舅");
    expectTitle({KinOp::FATHER, KinOp::FATHER}, SelfGender::MALE, "爷爷");
    expectTitle({KinOp::FATHER, KinOp::MOTHER}, SelfGender::MALE, "奶奶");
    expectTitle({KinOp::MOTHER, KinOp::FATHER}, SelfGender::FEMALE, "外公");
    expectTitle({KinOp::MOTHER, KinOp::MOTHER}, SelfGender::FEMALE, "外婆");
    expectTitle({KinOp::FATHER, KinOp::OLDER_BROTHER}, SelfGender::MALE, "伯父");
    expectTitle({KinOp::FATHER, KinOp::YOUNGER_BROTHER}, SelfGender::MALE, "叔叔");
    expectTitle({KinOp::MOTHER, KinOp::OLDER_SISTER}, SelfGender::MALE, "姨妈");
    expectTitle({KinOp::MOTHER, KinOp::YOUNGER_SISTER}, SelfGender::MALE, "小姨");

    expectTitle({KinOp::WIFE, KinOp::FATHER}, SelfGender::MALE, "岳父");
    expectTitle({KinOp::WIFE, KinOp::MOTHER}, SelfGender::MALE, "岳母");
    expectTitle({KinOp::HUSBAND, KinOp::FATHER}, SelfGender::FEMALE, "公公");
    expectTitle({KinOp::HUSBAND, KinOp::MOTHER}, SelfGender::FEMALE, "婆婆");
    expectTitle({KinOp::OLDER_BROTHER, KinOp::WIFE}, SelfGender::MALE, "嫂子");
    expectTitle({KinOp::YOUNGER_BROTHER, KinOp::WIFE}, SelfGender::MALE, "弟媳");
    expectTitle({KinOp::OLDER_SISTER, KinOp::HUSBAND}, SelfGender::MALE, "姐夫");
    expectTitle({KinOp::OLDER_BROTHER, KinOp::SON}, SelfGender::MALE, "侄子");
    expectTitle({KinOp::OLDER_SISTER, KinOp::SON}, SelfGender::MALE, "外甥");

    expectTitle({KinOp::FATHER, KinOp::FATHER, KinOp::FATHER}, SelfGender::MALE, "曾祖父");
    expectTitle({KinOp::FATHER, KinOp::OLDER_BROTHER, KinOp::WIFE}, SelfGender::MALE, "伯母");
    expectTitle({KinOp::FATHER, KinOp::YOUNGER_BROTHER, KinOp::WIFE}, SelfGender::MALE, "婶婶");
    expectTitle({KinOp::MOTHER, KinOp::OLDER_BROTHER, KinOp::WIFE}, SelfGender::MALE, "舅妈");
    expectTitle({KinOp::FATHER, KinOp::OLDER_SISTER, KinOp::HUSBAND}, SelfGender::MALE, "姑父");
    expectTitle({KinOp::FATHER, KinOp::OLDER_BROTHER, KinOp::SON}, SelfGender::MALE, "堂哥 / 堂弟");
    expectTitle({KinOp::MOTHER, KinOp::OLDER_BROTHER, KinOp::DAUGHTER}, SelfGender::MALE, "表姐 / 表妹");
    expectTitle({KinOp::SON, KinOp::WIFE, KinOp::FATHER}, SelfGender::MALE, "亲家公");
    expectTitle({KinOp::WIFE, KinOp::OLDER_SISTER, KinOp::HUSBAND}, SelfGender::MALE, "连襟");
    expectTitle({KinOp::HUSBAND, KinOp::OLDER_BROTHER, KinOp::WIFE}, SelfGender::FEMALE, "大嫂");

    expectTitle({KinOp::FATHER, KinOp::WIFE, KinOp::OLDER_BROTHER}, SelfGender::MALE, "舅舅");
    expectTitle({KinOp::WIFE, KinOp::HUSBAND}, SelfGender::MALE, "自己");

    auto conflict = RelativeEngine::evaluate({KinOp::HUSBAND}, SelfGender::MALE);
    CHECK(!conflict.supported && conflict.invalid_chain, "male->husband should detect conflict");

    auto deep = RelativeEngine::evaluate(
        {KinOp::FATHER, KinOp::OLDER_BROTHER, KinOp::SON, KinOp::SON}, SelfGender::MALE);
    CHECK(!deep.supported && deep.primary_title == "暂不支持", "unsupported deep chain must return 暂不支持");
}

static void testShakeDetector()
{
    ShakeDetector det(ShakeSensitivity::MEDIUM);
    uint64_t t = 0;

    for (int i = 0; i < 100; ++i, t += 20) {
        CHECK(!det.update(0.0f, 0.0f, 9.81f, t), "stationary false trigger");
    }

    for (int i = 0; i < 40; ++i, t += 20) {
        float angle = i * 0.04f;
        CHECK(!det.update(9.81f * std::sin(angle), 0.5f, 9.81f * std::cos(angle), t), "gentle tilt false trigger");
    }

    det.reset();
    t = 1000;
    for (int i = 0; i < 10; ++i, t += 20) det.update(0.0f, 0.0f, 9.81f, t);
    CHECK(!det.update(25.0f, 0.0f, 9.81f, t), "single impulse triggered prematurely");
    t += 20;
    for (int i = 0; i < 40; ++i, t += 20) {
        CHECK(!det.update(0.0f, 0.0f, 9.81f, t), "single bump decay triggered");
    }

    det.reset();
    t = 2000;
    for (int i = 0; i < 10; ++i, t += 20) det.update(0.0f, 0.0f, 9.81f, t);

    int triggers = 0;
    if (det.update(24.0f, 5.0f, 9.81f, t)) ++triggers;
    t += 40;
    if (det.update(0.0f, 0.0f, 9.81f, t)) ++triggers;
    t += 40;
    if (det.update(0.0f, 0.0f, 9.81f, t)) ++triggers;
    t += 60;
    if (det.update(-24.0f, -5.0f, 9.81f, t)) ++triggers;
    CHECK(triggers == 1, "intentional double-peak shake failed to trigger");

    t += 100;
    CHECK(!det.update(28.0f, 0.0f, 9.81f, t), "cooldown failed to suppress");
    t += 100;
    CHECK(!det.update(-28.0f, 0.0f, 9.81f, t), "cooldown failed to suppress 2");
}

static void testTimeLogic()
{
    PomodoroTimer pomo(25 * 60);
    CHECK(pomo.status() == PomodoroStatus::IDLE, "initial status IDLE");
    CHECK(pomo.remainingSeconds(0) == 1500, "initial remaining 1500s");

    pomo.start(10000);
    CHECK(pomo.status() == PomodoroStatus::RUNNING, "should be RUNNING");
    CHECK(pomo.remainingSeconds(10000 + 60000) == 1440, "1 min elapsed remaining wrong");

    pomo.pause(10000 + 60000);
    CHECK(pomo.status() == PomodoroStatus::PAUSED, "should be PAUSED");
    CHECK(pomo.remainingSeconds(999999) == 1440, "paused timer drifted");

    pomo.start(500000);
    bool finished = pomo.tick(500000 + 1440000 + 500);
    CHECK(finished && pomo.status() == PomodoroStatus::FINISHED, "pomodoro completion missed");
    CHECK(pomo.completedCount() == 1, "completed count wrong");

    auto invalid = computeOffWorkCountdown(false, 15, 0, 0, 18, 0);
    CHECK(!invalid.time_valid && invalid.phase == WorkDayPhase::INVALID, "invalid time flag lost");

    const auto before = computeOffWorkCountdown(true, 9, 29, 59, 18, 30, 9, 30);
    CHECK(before.phase == WorkDayPhase::BEFORE_WORK && !before.is_off_work &&
          before.remaining_seconds == 1 && before.progress_percent == 0, "countdown to work start wrong");

    const auto start = computeOffWorkCountdown(true, 9, 30, 0, 18, 30, 9, 30);
    CHECK(start.phase == WorkDayPhase::WORKING && start.progress_percent == 0 &&
          start.remaining_seconds == 9 * 3600, "shift start boundary wrong");

    const auto custom = computeOffWorkCountdown(true, 13, 0, 0, 17, 45, 8, 15);
    CHECK(custom.phase == WorkDayPhase::WORKING && custom.progress_percent == 50 &&
          custom.remaining_seconds == 4 * 3600 + 45 * 60, "custom shift progress wrong");

    const auto last_second = computeOffWorkCountdown(true, 17, 59, 59, 18, 0);
    CHECK(last_second.phase == WorkDayPhase::WORKING && last_second.progress_percent == 99 &&
          last_second.remaining_seconds == 1, "last second of shift wrong");
    const auto end = computeOffWorkCountdown(true, 18, 0, 0, 18, 0);
    CHECK(end.phase == WorkDayPhase::OFF_WORK && end.progress_percent == 100, "shift end boundary wrong");

    for (int end_hour : {8, 9}) {
        const auto range = computeOffWorkCountdown(true, 15, 0, 0, end_hour, 0, 9, 0);
        CHECK(range.time_valid && range.phase == WorkDayPhase::INVALID && !range.is_off_work &&
              range.remaining_seconds == 0 && range.progress_percent == 0, "invalid shift was accepted");
    }

    auto mid = computeOffWorkCountdown(true, 13, 30, 0, 18, 0, 9, 0);
    CHECK(mid.time_valid && !mid.is_off_work && mid.remaining_seconds == 4 * 3600 + 30 * 60, "midday remaining wrong");
    CHECK(mid.progress_percent == 50, "midday progress should be 50%");

    auto done = computeOffWorkCountdown(true, 18, 5, 0, 18, 0, 9, 0);
    CHECK(done.time_valid && done.is_off_work && done.remaining_seconds == 0 && done.progress_percent == 100,
          "after off-work status wrong");
}

static void testButtonLogic()
{
    ButtonLogic button;
    CHECK(button.update(true, 100) == ButtonPress::NONE, "raw press emitted an event");
    CHECK(button.update(true, 124) == ButtonPress::NONE, "press before the debounce threshold emitted an event");
    button.update(false, 125);
    CHECK(button.update(false, 150) == ButtonPress::NONE, "press bounce emitted an event");

    button.update(true, 200);
    button.update(true, 225);
    button.update(false, 300);
    button.update(true, 310);
    button.update(false, 320);
    CHECK(button.update(false, 344) == ButtonPress::NONE, "release emitted before debounce");
    CHECK(button.update(false, 345) == ButtonPress::SHORT, "stable release did not emit a short press");
    CHECK(button.update(false, 400) == ButtonPress::NONE, "short press repeated after release");

    button.update(true, 500);
    button.update(true, 525);
    CHECK(button.update(true, 1999) == ButtonPress::NONE, "long press emitted before the threshold");
    CHECK(button.update(true, 2000) == ButtonPress::LONG, "long press did not emit while held at the threshold");
    CHECK(button.update(true, 2100) == ButtonPress::NONE, "long press repeated while held");
    button.update(false, 2150);
    CHECK(button.update(false, 2175) == ButtonPress::NONE, "long press emitted another event on release");

    button.update(true, 2500);
    button.update(true, 2525);
    button.update(false, 3999);
    CHECK(button.update(false, 4024) == ButtonPress::SHORT, "release debounce counted toward long press duration");

    ButtonLogic other;
    button.update(true, 5000);
    button.update(true, 5025);
    other.update(true, 5100);
    other.update(true, 5125);
    other.update(false, 5200);
    CHECK(other.update(false, 5225) == ButtonPress::SHORT, "overlapping long hold interrupted another key");
    CHECK(button.update(true, 6499) == ButtonPress::NONE, "another key advanced the first key timer");
    CHECK(button.update(true, 6500) == ButtonPress::LONG, "another key interrupted the first key timer");
}

static void testMalletMotion()
{
    MalletMotion motion;
    CHECK(!motion.isActive() && motion.angle() == MalletMotion::kRestAngle && motion.frameIndex() == 0,
          "initial mallet state invalid");

    CHECK(motion.trigger(), "waking from rest should advance immediately");
    CHECK(motion.isActive(), "mallet should be active after trigger");
    CHECK(std::abs(motion.angle() - MalletMotion::kRestAngle) <= 60, "initial step exceeded anti-tear bound");

    bool hit_strike = false;
    int32_t max_step = std::abs(motion.angle() - MalletMotion::kRestAngle);
    for (int i = 0; i < 40 && motion.isActive(); ++i) {
        const int32_t prev = motion.angle();
        motion.step(MalletMotion::kTickMs);
        max_step = std::max(max_step, std::abs(motion.angle() - prev));
        if (motion.angle() == MalletMotion::kStrikeAngle) {
            hit_strike = true;
            CHECK(motion.frameIndex() == MalletMotion::kFrameCount - 1, "strike frame index mismatch");
        }
    }
    CHECK(hit_strike, "single knock did not reach strike angle");
    CHECK(!motion.isActive() && motion.angle() == MalletMotion::kRestAngle, "single knock did not return to rest");
    CHECK(max_step <= 60, "single knock exceeded per-frame angle step limit");

    for (uint32_t interval : {100u, 75u, 50u, 24u}) {
        motion.reset();
        uint32_t next_tap = 0;
        int strikes = 0;
        int32_t rapid_max_step = 0;
        int32_t prev = motion.angle();
        bool was_at_strike = false;

        for (uint32_t t = 0; t <= 1000; t += MalletMotion::kTickMs) {
            while (next_tap <= t) {
                motion.trigger();
                next_tap += interval;
            }
            if (t > 0) {
                motion.step(MalletMotion::kTickMs);
            }
            rapid_max_step = std::max(rapid_max_step, std::abs(motion.angle() - prev));
            prev = motion.angle();
            const bool at_strike = motion.angle() == MalletMotion::kStrikeAngle;
            if (at_strike && !was_at_strike) {
                ++strikes;
            }
            was_at_strike = at_strike;
            CHECK(motion.frameIndex() < MalletMotion::kFrameCount, "frame index out of range");
        }
        CHECK(rapid_max_step <= 60, "rapid knocking exceeded per-frame angle step limit");
        CHECK(strikes >= 8, "rapid knocking stalled instead of oscillating");

        for (int i = 0; i < 40 && motion.isActive(); ++i) {
            const int32_t before = motion.angle();
            motion.step(MalletMotion::kTickMs);
            CHECK(std::abs(motion.angle() - before) <= 60, "settle step exceeded limit");
        }
        CHECK(!motion.isActive() && motion.angle() == MalletMotion::kRestAngle,
              "mallet failed to settle back to rest after rapid taps");
    }

    motion.reset();
    motion.trigger();
    const int32_t before_stall = motion.angle();
    motion.step(250);
    CHECK(std::abs(motion.angle() - before_stall) <= 60, "large dt stall caused unbounded angle jump");
}

static int runAllTests()
{
    g_tests_run = 0;
    g_tests_failed = 0;
    testRandomEngine();
    testFortuneEngine();
    testGame2048Engine();
    testRelativeEngine();
    testShakeDetector();
    testButtonLogic();
    testMalletMotion();
    testTimeLogic();

    std::printf("Desk Buddy Core Tests: %d/%d passed (%d failed).\n",
                g_tests_run - g_tests_failed, g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern "C" void app_main(void)
{
    int rc = runAllTests();
    std::printf("TEST_RUNNER_EXIT_CODE=%d\n", rc);
    std::fflush(stdout);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#else
int main()
{
    return runAllTests();
}
#endif
