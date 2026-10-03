#include "audio/voice_synth.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct NoiseReference {
    std::array<bool, 23> bits{};
    std::uint32_t accumulator{};
    unsigned shifts{};

    NoiseReference() {
        for (unsigned bit = 3; bit < bits.size(); ++bit) bits[bit] = true;
    }

    void clock(std::uint16_t frequency) {
        const bool previous = (accumulator & (1U << 19U)) != 0;
        accumulator = (accumulator + frequency) & 0xffffffU;
        if (previous || (accumulator & (1U << 19U)) == 0) return;
        const bool feedback = bits[22] != bits[17];
        for (std::size_t bit = bits.size() - 1; bit != 0; --bit) bits[bit] = bits[bit - 1];
        bits[0] = feedback;
        ++shifts;
    }

    [[nodiscard]] float level() const {
        const unsigned byte = bits[20] * 128U + bits[18] * 64U + bits[14] * 32U +
            bits[11] * 16U + bits[9] * 8U + bits[5] * 4U + bits[2] * 2U + bits[0];
        return static_cast<float>((static_cast<double>(byte) / 127.5 - 1.0) * .25);
    }
};

void test_integer_clock_reference(std::uint32_t sample_rate, std::uint32_t chip_clock) {
    require(chip_clock % sample_rate == 0, "Reference needs whole chip cycles per sample");
    VoiceSynth synth(sample_rate, chip_clock);
    NoiseReference reference;
    AudioControlOutput output{};
    output.voices[0] = {0, AudioVoiceControl::NoiseGate, 15, 0, false};
    synth.set_output(output);
    std::array<float, 128> warmup{};
    synth.render(warmup);

    constexpr std::array<std::uint16_t, 6> frequencies{0, 0x4000, 0xffff, 0x0100, 0, 0x8000};
    std::array<float, 1024> samples{};
    for (const auto frequency : frequencies) {
        output.voices[0].frequency = frequency;
        synth.set_output(output);
        synth.render(samples);
        for (const auto sample : samples) {
            for (unsigned cycle = 0; cycle < chip_clock / sample_rate; ++cycle)
                reference.clock(frequency);
            require(std::abs(sample - reference.level()) < 0.000001f,
                    "Noise clock/output differs from the chip-cycle arithmetic reference");
        }
    }
    require(reference.shifts > 1024, "Reference did not exercise multiple noise edges");
}

void test_clock_runs_while_triangle_is_selected() {
    VoiceSynth synth(32768, 1048576);
    NoiseReference reference;
    AudioControlOutput output{};
    output.voices[0] = {0x4000, AudioVoiceControl::TriangleGate, 15, 0, false};
    synth.set_output(output);
    std::array<float, 128> samples{};
    synth.render(samples);
    for (unsigned cycle = 0; cycle < samples.size() * 32; ++cycle) reference.clock(0x4000);
    require(reference.shifts == 64, "Bit-19 reference must clock 16 times per oscillator period");

    output.voices[0].control = AudioVoiceControl::NoiseGate;
    synth.set_output(output);
    synth.render(samples);
    for (const auto sample : samples) {
        for (unsigned cycle = 0; cycle < 32; ++cycle) reference.clock(0x4000);
        require(std::abs(sample - reference.level()) < 0.000001f,
                "Changing waveform reset or stopped the noise shift clock");
    }
}
} // namespace

int main() {
    try {
        test_integer_clock_reference(32768, 1048576);
        test_integer_clock_reference(8000, 2000000);
        test_integer_clock_reference(48000, 960000);
        test_clock_runs_while_triangle_is_selected();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
