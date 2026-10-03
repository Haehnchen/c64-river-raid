#include "platform/sdl_audio.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct SdlSession {
    SdlSession() {
        if (!SDL_Init(SDL_INIT_AUDIO)) throw std::runtime_error(SDL_GetError());
    }
    ~SdlSession() { SDL_Quit(); }
};

void test_bounded_dummy_stream_and_gate() {
    using namespace river_raid;
    SdlAudioDevice device;
    require(!device.active() && device.queued_bytes() == 0,
            "Audio stream did not start paused and empty");

    const std::vector<float> samples(kPreviewAudioMaxQueuedFrames * 4, 0.125f);
    device.submit(samples);
    require(device.queued_bytes() == 0 && device.dropped_frames() == 0,
            "Inactive audio accepted PCM");

    device.set_active(true);
    device.submit(samples);
    require(device.queued_bytes() <= kPreviewAudioMaxQueuedFrames * sizeof(float),
            "Audio stream exceeded the 100 ms queue bound");
    require(device.dropped_frames() > 0, "Oversized PCM block was not bounded");
    device.clear();
    require(device.queued_bytes() == 0, "Explicit audio clear retained queued PCM");

    device.set_active(false);
    require(!device.active() && device.queued_bytes() == 0,
            "Inactive gate did not pause and clear queued PCM");
    const auto dropped = device.dropped_frames();
    device.submit(samples);
    require(device.queued_bytes() == 0 && device.dropped_frames() == dropped,
            "Audio was emitted while inactive");

    device.set_active(true);
    device.submit(std::vector<float>(480, 0.25f));
    const auto queued = device.queued_bytes();
    require(queued > 0 && queued <= 480 * sizeof(float),
            "Resumed dummy device did not receive the audio block");
    bool drained = false;
    for (int poll = 0; poll < 60; ++poll) {
        if (device.queued_bytes() == 0) {
            drained = true;
            break;
        }
        SDL_Delay(1);
    }
    require(drained, "Dummy device did not drain the resumed audio block within 60 ms");
}

} // namespace

int main() {
    try {
        SdlSession session;
        test_bounded_dummy_stream_and_gate();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
