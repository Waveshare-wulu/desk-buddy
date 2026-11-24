#pragma once

#include <atomic>
#include <cstdint>

namespace desk_buddy {

enum class SoundEffect : uint8_t {
    CLICK = 0,
    WOOD_FISH,
    COIN,
    DICE,
    COMPLETE,
};

class SoundService {
public:
    static SoundService &instance();

    bool init(bool enabled, uint8_t volume);
    void setEnabled(bool enabled);
    void setVolume(uint8_t volume);
    void prepareForShutdown();
    bool isEnabled() const { return _enabled; }
    bool isHardwareReady() const { return _hw_ready; }

    /** Non-blocking request to play a short procedural sound effect. */
    void play(SoundEffect effect);

private:
    SoundService() = default;
    static void audioTaskEntry(void *arg);
    void audioTaskLoop();
    void playSynthesized(SoundEffect effect);

    bool _hw_ready = false;
    std::atomic<bool> _enabled{true};
    std::atomic<bool> _playing{false};
    std::atomic<uint8_t> _volume{65};
    void *_codec_handle = nullptr;
    void *_queue = nullptr;
};

}
