#pragma once

#include "game/audio_control.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace river_raid {

// Triangle/noise and envelope approximation; not a cycle-exact SID.
class VoiceSynth {
public:
    VoiceSynth(std::uint32_t sample_rate, std::uint32_t chip_clock);
    void set_output(const AudioControlOutput& output) noexcept;
    void render(std::span<float> samples) noexcept;
    void reset() noexcept;

private:
    enum class EnvelopeStage { Attack, Decay, Sustain, Release };
    struct Voice {
        AudioVoiceOutput output{};
        double phase{};
        std::uint32_t noise{0x7ffff8};
        std::uint16_t rate_counter{};
        std::uint8_t envelope_level{};
        std::uint8_t exponential_counter{};
        std::uint8_t exponential_period{1};
        bool hold_zero{true};
        EnvelopeStage envelope{EnvelopeStage::Release};
    };
    static void advance_envelope(Voice& voice, std::uint32_t cycles) noexcept;
    double sample_voice(Voice& voice, std::uint32_t cycles) noexcept;

    std::uint32_t sample_rate_;
    std::uint32_t chip_clock_;
    std::uint64_t chip_cycle_remainder_{};
    std::array<Voice, 3> voices_{};
};

} // namespace river_raid
