#include "app/flight_audio.hpp"

#include "app/audio_setup.hpp"
#include "regional_timing.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace river_raid {
namespace {
constexpr std::uint32_t kSampleRate = 48000;
namespace timing = assets::regional_timing;
} // namespace

FlightAudio::FlightAudio()
    : controller_(make_audio_controller()),
      synth_(kSampleRate, timing::pal_ticks_numerator) {}

void FlightAudio::reset() noexcept {
    controller_.reset();
    synth_.reset();
    sample_remainder_ = 0;
    samples_.clear();
}

void FlightAudio::begin_batch() noexcept { samples_.clear(); }

void FlightAudio::advance(const AudioControlTick& tick) {
    synth_.set_output(controller_.advance(tick));
    sustain();
}

void FlightAudio::sustain() {
    sample_remainder_ += static_cast<std::uint64_t>(kSampleRate) * timing::pal_ticks_denominator;
    const auto count = static_cast<std::size_t>(sample_remainder_ / timing::pal_ticks_numerator);
    sample_remainder_ %= timing::pal_ticks_numerator;
    const auto offset = samples_.size();
    samples_.resize(offset + count);
    synth_.render(std::span(samples_).subspan(offset));
}

AudioController& FlightAudio::controller() noexcept { return controller_; }
const AudioControllerState& FlightAudio::state() const noexcept { return controller_.state(); }
bool FlightAudio::pending() const noexcept {
    const auto& current = state();
    return (current.voice1_effect != Voice1AudioEffect::None && current.voice1_countdown != 0) ||
        current.shot_countdown != 0 ||
        current.explosion_countdown != 0 || current.extra_life_countdown != 0 ||
        current.auxiliary_sprite_countdown != 0 || current.transition_countdown != 0 ||
        std::ranges::any_of(current.output.voices, [](const auto& voice) {
            return voice.control != AudioVoiceControl::Silent;
        });
}
std::vector<float> FlightAudio::take_samples() noexcept { return std::exchange(samples_, {}); }

} // namespace river_raid
