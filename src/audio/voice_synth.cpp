#include "audio/voice_synth.hpp"

#include <cmath>
#include <stdexcept>

namespace river_raid {
namespace {
constexpr std::array<std::uint16_t, 16> kRateComparison{
    8, 31, 62, 94, 148, 219, 266, 312,
    391, 976, 1953, 3125, 3906, 11719, 19531, 31250};
constexpr std::uint32_t kRateCounterWrap = 0x8000;
constexpr double kAccumulatorRange = 16777216.0;
constexpr double kVoiceGain = .25;
constexpr double kNoiseEdgesPerPeriod = 16.0;
constexpr std::array<unsigned, 8> kNoiseOutputBits{20, 18, 14, 11, 9, 5, 2, 0};

std::uint8_t exponential_period_at(std::uint8_t level) noexcept {
    switch (level) {
    case 0xff: return 1;
    case 0x5d: return 2;
    case 0x36: return 4;
    case 0x1a: return 8;
    case 0x0e: return 16;
    case 0x06: return 30;
    case 0x00: return 1;
    default: return 0;
    }
}

double noise_level(std::uint32_t state) noexcept {
    unsigned output = 0;
    for (const auto bit : kNoiseOutputBits) output = (output << 1U) | ((state >> bit) & 1U);
    return static_cast<double>(output) / 127.5 - 1.0;
}
} // namespace

VoiceSynth::VoiceSynth(std::uint32_t sample_rate, std::uint32_t chip_clock)
    : sample_rate_(sample_rate), chip_clock_(chip_clock) {
    if (sample_rate < 8000 || sample_rate > 192000 ||
        chip_clock < 500000 || chip_clock > 2000000) {
        throw std::invalid_argument("Unsupported synthesis clock");
    }
}

void VoiceSynth::set_output(const AudioControlOutput& output) noexcept {
    for (std::size_t index = 0; index < voices_.size(); ++index) {
        auto& voice = voices_[index];
        auto next = output.voices[index];
        next.sustain &= 15;
        next.release &= 15;
        const bool gated = next.control != AudioVoiceControl::Silent;
        const bool was_gated = voice.output.control != AudioVoiceControl::Silent;
        if (gated && (!was_gated || next.gate_restarted)) {
            voice.envelope = EnvelopeStage::Attack;
            voice.hold_zero = false;
        } else if (!gated && was_gated) {
            voice.envelope = EnvelopeStage::Release;
        } else if (gated && voice.envelope == EnvelopeStage::Sustain &&
                   voice.envelope_level != static_cast<unsigned>(next.sustain) * 0x11U) {
            voice.envelope = EnvelopeStage::Decay;
        }
        voice.output = next;
    }
}

void VoiceSynth::advance_envelope(Voice& voice, std::uint32_t cycles) noexcept {
    const auto initial_comparison = kRateComparison[
        voice.envelope == EnvelopeStage::Release ? voice.output.release : 0];
    if ((voice.hold_zero || voice.envelope == EnvelopeStage::Sustain) &&
        voice.rate_counter <= initial_comparison) {
        const auto total = static_cast<std::uint32_t>(voice.rate_counter) + cycles;
        const auto interval = static_cast<std::uint32_t>(initial_comparison) + 1U;
        voice.rate_counter = static_cast<std::uint16_t>(total % interval);
        if (!voice.hold_zero) {
            voice.exponential_counter = static_cast<std::uint8_t>(
                (voice.exponential_counter + total / interval) % voice.exponential_period);
        }
        return;
    }
    while (cycles != 0) {
        const auto comparison = kRateComparison[
            voice.envelope == EnvelopeStage::Release ? voice.output.release : 0];
        // The rate event follows the comparison, so zero-rate steps are nine cycles apart.
        const std::uint32_t until_event = voice.rate_counter <= comparison
            ? static_cast<std::uint32_t>(comparison - voice.rate_counter) + 1U
            : kRateCounterWrap - voice.rate_counter + comparison;
        if (cycles < until_event) {
            const auto advanced = static_cast<std::uint32_t>(voice.rate_counter) + cycles;
            voice.rate_counter = static_cast<std::uint16_t>(advanced < kRateCounterWrap
                ? advanced : (advanced + 1U) & (kRateCounterWrap - 1U));
            break;
        }
        cycles -= until_event;
        voice.rate_counter = 0;

        if (voice.envelope == EnvelopeStage::Attack) {
            voice.exponential_counter = 0;
            if (voice.envelope_level != 0xff) ++voice.envelope_level;
            if (const auto period = exponential_period_at(voice.envelope_level))
                voice.exponential_period = period;
            if (voice.envelope_level == 0xff) {
                voice.envelope = EnvelopeStage::Decay;
                if (voice.output.sustain == 15) voice.envelope = EnvelopeStage::Sustain;
            }
            continue;
        }
        if (voice.hold_zero) continue;
        if (++voice.exponential_counter < voice.exponential_period) continue;
        voice.exponential_counter = 0;
        if (voice.envelope == EnvelopeStage::Sustain) continue;
        if (voice.envelope == EnvelopeStage::Decay &&
            voice.envelope_level == static_cast<unsigned>(voice.output.sustain) * 0x11U) {
            voice.envelope = EnvelopeStage::Sustain;
            continue;
        }
        --voice.envelope_level;
        if (const auto period = exponential_period_at(voice.envelope_level))
            voice.exponential_period = period;
        if (voice.envelope_level == 0) voice.hold_zero = true;
        if (voice.envelope == EnvelopeStage::Decay &&
            voice.envelope_level == static_cast<unsigned>(voice.output.sustain) * 0x11U) {
            voice.envelope = EnvelopeStage::Sustain;
        }
    }
}

double VoiceSynth::sample_voice(Voice& voice, std::uint32_t cycles) noexcept {
    advance_envelope(voice, cycles);

    const double increment = static_cast<double>(voice.output.frequency) * chip_clock_ /
                             (kAccumulatorRange * sample_rate_);
    const double next_phase = voice.phase + increment;
    // Accumulator bit 19 rises 16 times per period, first at half a noise cycle.
    const auto noise_edges = static_cast<unsigned>(
        std::floor(next_phase * kNoiseEdgesPerPeriod + .5) -
        std::floor(voice.phase * kNoiseEdgesPerPeriod + .5));
    for (unsigned edge = 0; edge < noise_edges; ++edge) {
        const auto feedback = ((voice.noise >> 22) ^ (voice.noise >> 17)) & 1U;
        voice.noise = ((voice.noise << 1) | feedback) & 0x7fffffU;
    }
    voice.phase = next_phase - std::floor(next_phase);
    double waveform = 0;
    if (voice.output.control == AudioVoiceControl::TriangleGate) {
        waveform = 1.0 - 4.0 * std::abs(voice.phase - .5);
    } else if (voice.output.control == AudioVoiceControl::NoiseGate) {
        waveform = noise_level(voice.noise);
    }
    return waveform * (static_cast<double>(voice.envelope_level) / 255.0);
}

void VoiceSynth::render(std::span<float> samples) noexcept {
    for (auto& sample : samples) {
        chip_cycle_remainder_ += chip_clock_;
        const auto cycles = static_cast<std::uint32_t>(chip_cycle_remainder_ / sample_rate_);
        chip_cycle_remainder_ %= sample_rate_;
        double mixed = 0;
        for (auto& voice : voices_) mixed += sample_voice(voice, cycles);
        sample = static_cast<float>(mixed * kVoiceGain);
    }
}

void VoiceSynth::reset() noexcept {
    voices_ = {};
    chip_cycle_remainder_ = 0;
}

} // namespace river_raid
