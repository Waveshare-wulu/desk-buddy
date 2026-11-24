#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace desk_buddy {

enum class MoveDir : uint8_t {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
};

struct Game2048State {
    uint16_t board[4][4];
    uint32_t score;
    uint32_t best_score;
    bool won_2048;
    bool game_over;
};

struct MoveResult {
    bool moved;
    uint32_t score_gained;
    bool newly_won;
    bool spawned;
    int spawn_row;
    int spawn_col;
    bool merged_mask[4][4];
};

class Game2048Engine {
public:
    using RawRngFn = std::function<uint32_t()>;

    explicit Game2048Engine(RawRngFn rng_fn = nullptr);

    void reset(uint32_t keep_best_score = 0);
    bool loadState(const Game2048State &state);
    const Game2048State &getState() const { return _state; }

    /** Execute a move in the given direction. Spawns a new tile (2 or 4) iff the board changed. */
    MoveResult move(MoveDir dir);

    bool canMove() const;

    void setBoardForTest(const uint16_t board[4][4], uint32_t score = 0);

private:
    uint32_t nextU32();
    bool spawnTile(int *out_r = nullptr, int *out_c = nullptr);
    void updateGameOver();

    Game2048State _state{};
    RawRngFn _rng_fn;
    uint32_t _fallback_state;
};

}
