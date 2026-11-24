#include "db_game_2048_engine.hpp"

#include <cstring>
#include <utility>

namespace desk_buddy {

Game2048Engine::Game2048Engine(RawRngFn rng_fn)
    : _rng_fn(std::move(rng_fn))
    , _fallback_state(0x13579BDFu)
{
    reset(0);
}

uint32_t Game2048Engine::nextU32()
{
    if (_rng_fn) {
        return _rng_fn();
    }
    uint32_t x = _fallback_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _fallback_state = x ? x : 0x2468ACE1u;
    return _fallback_state;
}

void Game2048Engine::reset(uint32_t keep_best_score)
{
    std::memset(&_state, 0, sizeof(_state));
    _state.best_score = keep_best_score;
    spawnTile();
    spawnTile();
    updateGameOver();
}

bool Game2048Engine::loadState(const Game2048State &state)
{
    bool has_any = false;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            uint16_t v = state.board[r][c];
            if (v != 0) {
                if ((v & (v - 1)) != 0 || v < 2) {
                    return false;
                }
                has_any = true;
            }
        }
    }
    if (!has_any) {
        return false;
    }
    _state = state;
    if (_state.best_score < _state.score) {
        _state.best_score = _state.score;
    }
    updateGameOver();
    return true;
}

void Game2048Engine::setBoardForTest(const uint16_t board[4][4], uint32_t score)
{
    std::memcpy(_state.board, board, sizeof(_state.board));
    _state.score = score;
    if (_state.best_score < score) {
        _state.best_score = score;
    }
    _state.won_2048 = false;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (_state.board[r][c] >= 2048) {
                _state.won_2048 = true;
            }
        }
    }
    updateGameOver();
}

bool Game2048Engine::spawnTile(int *out_r, int *out_c)
{
    int empty_r[16];
    int empty_c[16];
    int count = 0;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (_state.board[r][c] == 0) {
                empty_r[count] = r;
                empty_c[count] = c;
                ++count;
            }
        }
    }
    if (count == 0) {
        return false;
    }
    const int pick = static_cast<int>(nextU32() % static_cast<uint32_t>(count));
    const uint16_t value = ((nextU32() % 10u) == 0u) ? 4 : 2;
    _state.board[empty_r[pick]][empty_c[pick]] = value;
    if (out_r) {
        *out_r = empty_r[pick];
    }
    if (out_c) {
        *out_c = empty_c[pick];
    }
    return true;
}

bool Game2048Engine::canMove() const
{
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const uint16_t v = _state.board[r][c];
            if (v == 0) {
                return true;
            }
            if (r + 1 < 4 && _state.board[r + 1][c] == v) {
                return true;
            }
            if (c + 1 < 4 && _state.board[r][c + 1] == v) {
                return true;
            }
        }
    }
    return false;
}

void Game2048Engine::updateGameOver()
{
    _state.game_over = !canMove();
}

MoveResult Game2048Engine::move(MoveDir dir)
{
    MoveResult res{};
    res.spawn_row = -1;
    res.spawn_col = -1;

    uint16_t old_board[4][4];
    std::memcpy(old_board, _state.board, sizeof(old_board));

    const bool was_won = _state.won_2048;

    auto processLine = [&](uint16_t line[4], bool merged[4]) {
        uint16_t compact[4] = {0, 0, 0, 0};
        int n = 0;
        for (int i = 0; i < 4; ++i) {
            if (line[i] != 0) {
                compact[n++] = line[i];
            }
        }

        uint16_t out[4] = {0, 0, 0, 0};
        int out_idx = 0;
        for (int input_idx = 0; input_idx < n;) {
            if (input_idx + 1 < n && compact[input_idx] == compact[input_idx + 1]) {
                const uint16_t merged_val = static_cast<uint16_t>(compact[input_idx] * 2);
                out[out_idx] = merged_val;
                merged[out_idx] = true;
                res.score_gained += merged_val;
                if (merged_val >= 2048) {
                    _state.won_2048 = true;
                }
                input_idx += 2;
            } else {
                out[out_idx] = compact[input_idx++];
            }
            ++out_idx;
        }

        for (int i = 0; i < 4; ++i) {
            line[i] = out[i];
        }
    };

    for (int idx = 0; idx < 4; ++idx) {
        uint16_t line[4];
        bool merged[4] = {false, false, false, false};

        for (int k = 0; k < 4; ++k) {
            switch (dir) {
                case MoveDir::LEFT:  line[k] = _state.board[idx][k]; break;
                case MoveDir::RIGHT: line[k] = _state.board[idx][3 - k]; break;
                case MoveDir::UP:    line[k] = _state.board[k][idx]; break;
                case MoveDir::DOWN:  line[k] = _state.board[3 - k][idx]; break;
            }
        }

        processLine(line, merged);

        for (int k = 0; k < 4; ++k) {
            switch (dir) {
                case MoveDir::LEFT:
                    _state.board[idx][k] = line[k];
                    res.merged_mask[idx][k] = merged[k];
                    break;
                case MoveDir::RIGHT:
                    _state.board[idx][3 - k] = line[k];
                    res.merged_mask[idx][3 - k] = merged[k];
                    break;
                case MoveDir::UP:
                    _state.board[k][idx] = line[k];
                    res.merged_mask[k][idx] = merged[k];
                    break;
                case MoveDir::DOWN:
                    _state.board[3 - k][idx] = line[k];
                    res.merged_mask[3 - k][idx] = merged[k];
                    break;
            }
        }
    }

    res.moved = (std::memcmp(old_board, _state.board, sizeof(old_board)) != 0);
    if (res.moved) {
        _state.score += res.score_gained;
        if (_state.score > _state.best_score) {
            _state.best_score = _state.score;
        }
        res.newly_won = (!was_won && _state.won_2048);
        res.spawned = spawnTile(&res.spawn_row, &res.spawn_col);
        updateGameOver();
    }

    return res;
}

}
