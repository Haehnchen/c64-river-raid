#include "audio/voice_synth.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace river_raid;

constexpr std::uint32_t kSampleRate = 96'000;
constexpr std::uint32_t kNineCyclesPerSample = 864'000;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

AudioControlOutput gated_voice(std::uint8_t sustain, bool restarted = false) {
    AudioControlOutput output{};
    output.voices[0] = {0, AudioVoiceControl::TriangleGate, sustain, 0, restarted};
    return output;
}

AudioControlOutput released_voice() {
    AudioControlOutput output{};
    output.voices[0].release = 0;
    return output;
}

void require_level(float sample, unsigned level, std::string_view message) {
    // At zero oscillator frequency the triangle remains at -1; one voice has 1/4 mix gain.
    const auto expected = -static_cast<double>(level) / (255.0 * 4.0);
    require(std::abs(static_cast<double>(sample) - expected) < 0.000001, message);
}

std::vector<float> render(VoiceSynth& synth, std::size_t count) {
    std::vector<float> samples(count);
    synth.render(samples);
    return samples;
}

void test_zero_attack_rate_and_full_attack() {
    auto output = gated_voice(15);

    VoiceSynth eight_cycles(kSampleRate, 768'000);
    eight_cycles.set_output(output);
    const auto before_rate = render(eight_cycles, 1);
    require_level(before_rate.back(), 0, "AD=0 attack advanced before nine chip cycles");
    const auto after_rate = render(eight_cycles, 1);
    require_level(after_rate.back(), 1, "AD=0 attack did not advance at its nine-cycle period");

    VoiceSynth nine_cycles(kSampleRate, kNineCyclesPerSample);
    nine_cycles.set_output(output);
    const auto attack = render(nine_cycles, 255);
    require_level(attack.front(), 1, "First attack step did not occur on cycle nine");
    require_level(attack.back(), 255, "Full attack did not reach all 255 envelope levels");
    const auto held = render(nine_cycles, 20);
    require_level(held.back(), 255, "Maximum sustain did not hold the full envelope");
}

void test_decay_thresholds_and_sustain_level() {
    VoiceSynth decay(kSampleRate, kNineCyclesPerSample);
    decay.set_output(gated_voice(0));
    require_level(render(decay, 255).back(), 255, "Attack did not reach the decay peak");

    constexpr std::array<std::size_t, 6> pulse_counts{162, 78, 112, 96, 128, 180};
    constexpr std::array<unsigned, 6> levels{93, 54, 26, 14, 6, 0};
    for (std::size_t index = 0; index < pulse_counts.size(); ++index) {
        const auto samples = render(decay, pulse_counts[index]);
        require_level(samples.back(), levels[index], "Exponential decay crossed a SID level threshold incorrectly");
    }
    require_level(render(decay, 40).back(), 0, "Zero envelope underflowed while the gate remained set");

    VoiceSynth sustain(kSampleRate, kNineCyclesPerSample);
    sustain.set_output(gated_voice(8));
    render(sustain, 255);
    require_level(render(sustain, 119).back(), 136, "Sustain nibble was not expanded to its 8-bit target");
    require_level(render(sustain, 200).back(), 136, "Decay did not hold exactly at the selected sustain level");
}

void test_retrigger_and_sustain_change_without_retrigger() {
    VoiceSynth synth(kSampleRate, kNineCyclesPerSample);
    auto output = gated_voice(0);
    synth.set_output(output);
    render(synth, 255);
    require_level(render(synth, 155).back(), 100, "Decay fixture did not reach its expected level");

    output.voices[0].gate_restarted = true;
    synth.set_output(output);
    require_level(render(synth, 1).back(), 101, "Gate retrigger did not attack from the current envelope level");

    // With no gate restart the SID does not raise the envelope to a newly larger sustain target.
    VoiceSynth changed_target(kSampleRate, kNineCyclesPerSample);
    output = gated_voice(0);
    changed_target.set_output(output);
    render(changed_target, 255);
    require_level(render(changed_target, 155).back(), 100,
                  "Sustain-change fixture did not reach its expected level");
    output.voices[0].sustain = 10;
    changed_target.set_output(output);
    require_level(render(changed_target, 2).back(), 98,
                  "Changing sustain upward without a gate restart raised or clamped the envelope");
}

void test_release_rate_and_zero_hold() {
    VoiceSynth partial_release(kSampleRate, kNineCyclesPerSample);
    partial_release.set_output(gated_voice(15));
    render(partial_release, 255);
    partial_release.set_output(released_voice());
    render(partial_release, 10);
    partial_release.set_output(gated_voice(15));
    require_level(render(partial_release, 1).back(), 246,
                  "Release rate zero did not lower the envelope before a new attack");

    VoiceSynth zero_hold(kSampleRate, kNineCyclesPerSample);
    zero_hold.set_output(gated_voice(15));
    render(zero_hold, 255);
    zero_hold.set_output(released_voice());
    render(zero_hold, 900);
    zero_hold.set_output(gated_voice(15));
    require_level(render(zero_hold, 1).back(), 1,
                  "Release did not hold at zero before the next gate attack");
}

void test_chip_clock_and_partition_invariance() {
    auto output = gated_voice(15);
    VoiceSynth normal_clock(kSampleRate, kNineCyclesPerSample);
    VoiceSynth double_clock(kSampleRate, 1'728'000);
    normal_clock.set_output(output);
    double_clock.set_output(output);
    require_level(render(normal_clock, 10).back(), 10, "Envelope did not use chip-cycle time");
    require_level(render(double_clock, 10).back(), 20, "Doubling chip cycles did not double the envelope rate");

    constexpr std::uint32_t fractional_clock = 985'248;
    VoiceSynth whole(kSampleRate, fractional_clock);
    VoiceSynth partitioned(kSampleRate, fractional_clock);
    std::vector<float> whole_pcm(1'800), partitioned_pcm(1'800);
    output = gated_voice(8);
    whole.set_output(output);
    partitioned.set_output(output);
    whole.render(std::span(whole_pcm).first(700));
    constexpr std::array<std::size_t, 5> chunks{1, 17, 4, 63, 9};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < 700) {
        const auto count = std::min(chunks[chunk_index++ % chunks.size()], 700 - offset);
        partitioned.render(std::span(partitioned_pcm).subspan(offset, count));
        offset += count;
    }

    output.voices[0].sustain = 4;
    whole.set_output(output);
    partitioned.set_output(output);
    whole.render(std::span(whole_pcm).subspan(700, 600));
    offset = 700;
    chunk_index = 0;
    while (offset < 1'300) {
        const auto count = std::min(chunks[chunk_index++ % chunks.size()], 1'300 - offset);
        partitioned.render(std::span(partitioned_pcm).subspan(offset, count));
        offset += count;
    }

    whole.set_output(released_voice());
    partitioned.set_output(released_voice());
    whole.render(std::span(whole_pcm).subspan(1'300));
    offset = 1'300;
    chunk_index = 0;
    while (offset < partitioned_pcm.size()) {
        const auto count = std::min(chunks[chunk_index++ % chunks.size()], partitioned_pcm.size() - offset);
        partitioned.render(std::span(partitioned_pcm).subspan(offset, count));
        offset += count;
    }
    require(whole_pcm == partitioned_pcm,
            "Fractional chip-cycle envelopes changed with render-buffer partitioning");

    whole.reset();
    whole.set_output(gated_voice(15));
    require_level(render(whole, 1).back(), 1, "Reset did not restart the envelope and cycle phase");
}

void test_retrigger_retains_rate_phase_and_fractional_cycles() {
    auto output = gated_voice(15);
    VoiceSynth phased(kSampleRate, 768'000);
    phased.set_output(output);
    require_level(render(phased, 1).back(), 0, "Eight-cycle prefix advanced the attack");
    output.voices[0].gate_restarted = true;
    phased.set_output(output);
    require_level(render(phased, 1).back(), 1,
                  "Gate retrigger discarded the partially elapsed nine-cycle rate interval");

    constexpr std::uint32_t fractional_clock = 985'248;
    VoiceSynth fractional(kSampleRate, fractional_clock);
    fractional.set_output(gated_voice(15));
    const auto prefix = render(fractional, 100);
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        const auto elapsed_cycles = (static_cast<std::uint64_t>(index + 1) * fractional_clock) /
                                    kSampleRate;
        const auto expected_level = static_cast<unsigned>(elapsed_cycles / 9);
        require(expected_level < 255, "Fractional-cycle prefix unexpectedly left attack");
        require_level(prefix[index], expected_level,
                      "Fractional chip cycles did not preserve the exact attack prefix");
    }
}
} // namespace

int main() {
    try {
        test_zero_attack_rate_and_full_attack();
        test_decay_thresholds_and_sustain_level();
        test_retrigger_and_sustain_change_without_retrigger();
        test_release_rate_and_zero_hold();
        test_chip_clock_and_partition_invariance();
        test_retrigger_retains_rate_phase_and_fractional_cycles();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
