#pragma once

#include "audio/voice_synth.hpp"
#include "game/audio_control.hpp"

#include <cstdint>
#include <vector>

namespace river_raid {

class FlightAudio {
public:
    FlightAudio();
    void reset() noexcept;
    void begin_batch() noexcept;
    void advance(const AudioControlTick& tick);
    void sustain();
    [[nodiscard]] AudioController& controller() noexcept;
    [[nodiscard]] const AudioControllerState& state() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] std::vector<float> take_samples() noexcept;

private:
    AudioController controller_;
    VoiceSynth synth_;
    std::uint64_t sample_remainder_{};
    std::vector<float> samples_;
};

} // namespace river_raid
