#include "app/flight_audio.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace river_raid;
    try {
        FlightAudio audio;
        require(audio.take_samples().empty(), "Unexpected startup audio");
        AudioControlTick tick;
        audio.controller().start_shot();
        audio.advance(tick);
        require(audio.state().shot_countdown == 31, "Shot event not consumed");
        const auto first = audio.take_samples();
        require(first.size() == 957, "Wrong first tick sample count");
        require(std::ranges::any_of(first, [](float value) { return value != 0; }),
                "Shot produces no samples");
        require(audio.take_samples().empty(), "Samples consumed twice");
        audio.reset();
        audio.controller().start_shot();
        audio.advance(tick);
        require(audio.take_samples() == first, "Restart does not reproduce audio");

        const auto sustained_state = audio.state();
        require(sustained_state.shot_countdown == 31,
                "Sustain test did not retain the active controller countdown");
        audio.begin_batch();
        audio.sustain();
        const auto sustained_first = audio.take_samples();
        require(!sustained_first.empty() &&
                    std::ranges::any_of(sustained_first, [](float value) { return value != 0; }),
                "Sustain did not render the retained voice output");
        require(audio.state() == sustained_state &&
                    audio.state().output == sustained_state.output &&
                    audio.state().shot_countdown == sustained_state.shot_countdown,
                "Sustain advanced controller registers or countdowns");

        audio.begin_batch();
        audio.sustain();
        const auto sustained_second = audio.take_samples();
        require(!sustained_second.empty() && sustained_second != sustained_first,
                "Sustain did not advance the PCM oscillator");
        require(audio.state() == sustained_state,
                "Repeated sustain changed controller state");

        audio.begin_batch();
        audio.sustain();
        audio.reset();
        require(audio.state() == AudioControllerState{} && audio.take_samples().empty() &&
                    !audio.pending(),
                "Reset retained sustained registers, countdowns, or PCM");

        audio.reset();
        std::size_t samples = 0;
        constexpr std::size_t ticks = 500;
        for (std::size_t i = 0; i < ticks; ++i) {
            audio.begin_batch();
            audio.advance(tick);
            samples += audio.take_samples().size();
        }
        require(samples == ticks * 48000ULL * 19656ULL / 985248ULL,
                "Fractional samples lost across batches");
        audio.advance(tick);
        audio.begin_batch();
        require(audio.take_samples().empty(), "Unconsumed batch retained");

        tick.lifecycle_muted = true;
        audio.advance(tick);
        const auto muted = audio.take_samples();
        require(!muted.empty() && std::ranges::all_of(muted, [](float v) { return v == 0; }),
                "Muted lifecycle produces sound");
        audio.reset();
        require(audio.state() == AudioControllerState{}, "Controller state survives reset");
        require(audio.take_samples().empty(), "Buffered audio survives reset");
        require(!audio.pending(), "Reset audio has a pending tail");
        tick.lifecycle_muted = false;
        tick.fuel = 0x3000;
        audio.advance(tick);
        require(audio.state().voice1_countdown != 0, "Warning phase not started");
        tick.flight_active = false;
        audio.advance(tick);
        require(!audio.pending(), "Inactive warning phase keeps terminal audio alive");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
