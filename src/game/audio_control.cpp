#include "game/audio_control.hpp"

#include <cstddef>

namespace river_raid {
namespace {

using Byte = std::uint8_t;

constexpr Byte wrapping_add(Byte left, Byte right) noexcept {
    return static_cast<Byte>(static_cast<unsigned>(left) + right);
}

constexpr Byte subtract_with_carry(Byte left, Byte right, bool carry) noexcept {
    return static_cast<Byte>(static_cast<unsigned>(left) - right - (carry ? 0U : 1U));
}

constexpr void rotate_left(Byte& value, bool& carry) noexcept {
    const auto next_carry = (value & 0x80U) != 0;
    value = static_cast<Byte>((value << 1U) | (carry ? 1U : 0U));
    carry = next_carry;
}

constexpr AudioVoiceControl control_for(Byte effect_code) noexcept {
    switch (effect_code & 0xf0U) {
    case 0x80:
        return AudioVoiceControl::NoiseGate;
    case 0xa0:
        return AudioVoiceControl::TriangleGate;
    default:
        return AudioVoiceControl::Silent;
    }
}

void write_frequency_high(AudioVoiceOutput& voice, Byte value) noexcept {
    voice.frequency = static_cast<std::uint16_t>(value) << 8U;
}

void apply_effect_code(AudioVoiceOutput& voice, Byte& previous_sustain_release,
                       Byte effect_code) noexcept {
    const auto sustain_release = static_cast<Byte>((effect_code & 0x0fU) << 4U);
    voice.gate_restarted = sustain_release > previous_sustain_release;
    previous_sustain_release = sustain_release;
    voice.sustain = static_cast<Byte>(sustain_release >> 4U);
    voice.release = 0;
    voice.control = control_for(effect_code);
}

constexpr Voice1AudioEffect effect_for(LifeEndAudioCause cause) noexcept {
    return cause == LifeEndAudioCause::Collision ? Voice1AudioEffect::Collision
                                                  : Voice1AudioEffect::FuelExhausted;
}

constexpr Voice1AudioEffect effect_for(RefuelAudioKind kind) noexcept {
    return kind == RefuelAudioKind::TankSaturated
               ? Voice1AudioEffect::RefuelTankSaturated
               : Voice1AudioEffect::RefuelFuelAdded;
}

Byte vertical_input_selector(const AudioControlTick& tick) noexcept {
    auto selector = Byte{0};
    if (!tick.accelerating) selector |= 0x01U;
    if (!tick.decelerating) selector |= 0x02U;
    return selector;
}

struct VoiceSelection {
    Byte effect_code{};
    Byte frequency_high{};
    bool writes_frequency{true};
};

VoiceSelection select_engine(const AudioControlTick& tick, AudioControllerState& state,
                             const AudioControlTables& tables) noexcept {
    const auto effect_code = tables.engine_voice1_selector[vertical_input_selector(tick)];
    if (state.engine_ramp < 0x15U) {
        const auto frequency = state.engine_ramp;
        ++state.engine_ramp;
        return {effect_code, frequency, true};
    }
    return {effect_code, static_cast<Byte>((tick.scroll_speed >> 4U) + 0x0eU), true};
}

VoiceSelection select_fuel_warning(const AudioControlTick& tick, AudioControllerState& state,
                                   const AudioControlTables& tables) noexcept {
    auto phase = state.voice1_countdown;
    if (phase == 0) phase = 0x3f;
    --phase;
    state.voice1_countdown = phase;

    auto fuel_high = static_cast<Byte>(tick.fuel >> 8U);
    if (fuel_high < 4U) {
        auto fuel_low = static_cast<Byte>(tick.fuel);
        auto carry = false;
        rotate_left(fuel_low, carry);
        rotate_left(fuel_high, carry);
        rotate_left(fuel_low, carry);
        rotate_left(fuel_high, carry);

        const auto sum = static_cast<unsigned>(fuel_high) + 8U + (carry ? 1U : 0U);
        fuel_high = static_cast<Byte>(sum);
        fuel_high = static_cast<Byte>(fuel_high << 1U);
        if (fuel_high != 0) return {0xaf, fuel_high, true};
    }

    if (phase < 0x1cU) return select_engine(tick, state, tables);
    auto frequency = static_cast<Byte>(phase ^ 0xffU);
    frequency = subtract_with_carry(frequency, 0xa8, true);
    return {0xaf, frequency, true};
}

VoiceSelection select_voice1(const AudioControlTick& tick, AudioControllerState& state,
                             const AudioControlTables& tables) noexcept {
    const auto effect = state.voice1_effect;
    if (effect != Voice1AudioEffect::None) {
        const auto countdown = state.voice1_countdown;
        Byte effect_code{};
        Byte frequency{};
        if (effect == Voice1AudioEffect::Collision || effect == Voice1AudioEffect::FuelExhausted) {
            effect_code = static_cast<Byte>((countdown >> 1U) | 0x80U);
            frequency = 3;
        } else {
            effect_code = wrapping_add(countdown, 0xa0);
            frequency = effect == Voice1AudioEffect::RefuelTankSaturated ? Byte{0x3f} : Byte{0x20};
        }
        --state.voice1_countdown;
        if (state.voice1_countdown == 0) state.voice1_effect = Voice1AudioEffect::None;
        return {effect_code, frequency, true};
    }

    if (!tick.flight_active) return {0, 0, false};
    if (static_cast<Byte>(tick.fuel >> 8U) < 0x40U) {
        return select_fuel_warning(tick, state, tables);
    }
    return select_engine(tick, state, tables);
}

Byte update_voice2(const AudioControlTick& tick, AudioControllerState& state,
                   const AudioControlTables& tables,
                   std::array<Byte, 3>& previous_sustain_release,
                   Byte voice1_effect_code) noexcept {
    auto frequency = static_cast<Byte>(voice1_effect_code & 0xf0U);
    auto effect_code = Byte{0};

    if (state.shot_countdown != 0) {
        frequency = state.shot_countdown;
        --state.shot_countdown;
        effect_code = 0xaa;
    }

    if (state.explosion_countdown != 0) {
        const auto countdown = state.explosion_countdown;
        --state.explosion_countdown;
        effect_code = static_cast<Byte>(0x80U + ((countdown >> 1U) & 0x0fU));
        frequency = static_cast<Byte>((tick.timer_entropy & 0x01U) + 1U);
    }

    if (state.extra_life_countdown != 0) {
        --state.extra_life_countdown;
        const auto countdown = state.extra_life_countdown;
        effect_code = static_cast<Byte>(0xa0U + (countdown & 0x0fU));
        frequency = tables.effect_voice2_frequency_high[countdown >> 4U];
    }

    write_frequency_high(state.output.voices[1], frequency);
    apply_effect_code(state.output.voices[1], previous_sustain_release[1], effect_code);
    return effect_code;
}

void update_voice3(const AudioControlTick& tick, AudioControllerState& state,
                   const AudioControlTables& tables,
                   std::array<Byte, 3>& previous_sustain_release,
                   Byte voice2_effect_code) noexcept {
    auto frequency = static_cast<Byte>(voice2_effect_code & 0xf0U);
    auto effect_code = Byte{0};

    if (state.auxiliary_sprite_countdown != 0) {
        effect_code = state.auxiliary_sprite_countdown;
        --state.auxiliary_sprite_countdown;
        if (tick.demo_mode) {
            apply_effect_code(state.output.voices[2], previous_sustain_release[2], effect_code);
            return;
        }
        effect_code = wrapping_add(effect_code, 0x86);
        frequency = static_cast<Byte>(effect_code & 0x0fU);
    } else if (tick.demo_mode) {
        apply_effect_code(state.output.voices[2], previous_sustain_release[2], effect_code);
        return;
    }

    const auto counter = tick.special_sprite_counter;
    if ((counter & 0x80U) == 0 && counter != 0) {
        if (counter < 0x11U) {
            frequency = tables.special_voice3_frequency_high[counter];
            effect_code = 0xac;
        } else {
            frequency = static_cast<Byte>(tick.timer_entropy & 0x07U);
            effect_code = 0x8c;
        }
    }

    write_frequency_high(state.output.voices[2], frequency);
    apply_effect_code(state.output.voices[2], previous_sustain_release[2], effect_code);
}

void silence_voice3(AudioControllerState& state,
                    std::array<Byte, 3>& previous_sustain_release) noexcept {
    apply_effect_code(state.output.voices[2], previous_sustain_release[2], 0);
}

} // namespace

AudioController::AudioController(AudioControlTables tables) noexcept : tables_(tables) {}

void AudioController::start_life_end(LifeEndAudioCause cause) noexcept {
    state_.voice1_effect = effect_for(cause);
    state_.voice1_countdown = 0x18;
}

void AudioController::start_refuel(RefuelAudioKind kind) noexcept {
    const auto effect = effect_for(kind);
    if (state_.voice1_effect == effect) return;
    state_.voice1_effect = effect;
    state_.voice1_countdown = 8;
}

void AudioController::start_shot() noexcept { state_.shot_countdown = 0x20; }

void AudioController::start_explosion() noexcept { state_.explosion_countdown = 0x1f; }

void AudioController::award_extra_life() noexcept { state_.extra_life_countdown = 0x40; }

void AudioController::start_auxiliary_sprite_effect() noexcept {
    if (state_.auxiliary_sprite_countdown == 0) state_.auxiliary_sprite_countdown = 8;
}

void AudioController::start_transition_effect() noexcept { state_.transition_countdown = 2; }

void AudioController::reset_engine_ramp() noexcept { state_.engine_ramp = 0; }

AudioControlOutput AudioController::advance(const AudioControlTick& tick) noexcept {
    for (auto& voice : state_.output.voices) voice.gate_restarted = false;

    Byte voice1_effect_code{};
    if (state_.transition_countdown != 0) {
        if (state_.transition_countdown == 2) {
            --state_.transition_countdown;
            voice1_effect_code = 0xaf;
            write_frequency_high(state_.output.voices[0], 0x20);
            apply_effect_code(state_.output.voices[0], previous_sustain_release_[0],
                              voice1_effect_code);
        } else {
            --state_.transition_countdown;
            state_.output.voices[1].control = AudioVoiceControl::Silent;
            silence_voice3(state_, previous_sustain_release_);
            return state_.output;
        }
    } else if (tick.lifecycle_muted) {
        state_.output.voices[0].control = AudioVoiceControl::Silent;
        state_.output.voices[1].control = AudioVoiceControl::Silent;
        silence_voice3(state_, previous_sustain_release_);
        return state_.output;
    } else {
        const auto selection = select_voice1(tick, state_, tables_);
        voice1_effect_code = selection.effect_code;
        if (selection.writes_frequency) {
            write_frequency_high(state_.output.voices[0], selection.frequency_high);
        }
        apply_effect_code(state_.output.voices[0], previous_sustain_release_[0],
                          voice1_effect_code);
    }

    const auto voice2_effect_code = update_voice2(
        tick, state_, tables_, previous_sustain_release_, voice1_effect_code);
    update_voice3(tick, state_, tables_, previous_sustain_release_, voice2_effect_code);
    return state_.output;
}

const AudioControllerState& AudioController::state() const noexcept { return state_; }

void AudioController::reset() noexcept {
    state_ = {};
    previous_sustain_release_ = {};
}

} // namespace river_raid
