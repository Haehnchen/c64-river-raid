#include "game/audio_control.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using namespace river_raid;

constexpr std::array<std::uint8_t, 4> kEngineSelectors{0x00, 0x81, 0x84, 0x82};
constexpr std::array<std::uint8_t, 4> kExtraLifeFrequencies{0x40, 0x30, 0x26, 0x20};
constexpr std::array<std::uint8_t, 17> kSpecialFrequencies{
    0xde, 0xb9, 0x9a, 0x80, 0x6b, 0x59, 0x4a, 0x3e, 0x34,
    0x2b, 0x24, 0x1e, 0x19, 0x15, 0x11, 0x0e, 0x0c,
};

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

AudioController controller() {
    return AudioController({kEngineSelectors, kExtraLifeFrequencies, kSpecialFrequencies});
}

const AudioVoiceOutput& voice(const AudioControlOutput& output, std::size_t index) {
    return output.voices[index];
}

void test_engine_ramp_and_lifecycle_gate() {
    auto audio = controller();
    const auto first = audio.advance({});
    require(voice(first, 0) == AudioVoiceOutput{
                                   0x0000, AudioVoiceControl::NoiseGate, 2, 0, true},
            "Initial engine voice differs");
    require(voice(first, 1).frequency == 0x8000 &&
                voice(first, 1).control == AudioVoiceControl::Silent,
            "Idle voice 2 did not retain the engine-code accumulator value");
    require(audio.state().engine_ramp == 1, "Engine ramp did not advance from zero");

    auto tick = AudioControlTick{};
    tick.accelerating = true;
    const auto accelerated = audio.advance(tick);
    require(voice(accelerated, 0) == AudioVoiceOutput{
                                         0x0100, AudioVoiceControl::NoiseGate, 4, 0, true},
            "Vertical-input engine selector differs");

    audio.reset();
    AudioControlOutput output{};
    for (int count = 0; count < 22; ++count) output = audio.advance({});
    require(voice(output, 0).frequency == 0x1600 && audio.state().engine_ramp == 0x15,
            "Engine did not switch from the 21-step ramp to scroll-speed frequency");

    tick = {};
    tick.flight_active = false;
    const auto inactive = audio.advance(tick);
    require(voice(inactive, 0).frequency == 0x1600 &&
                voice(inactive, 0).control == AudioVoiceControl::Silent,
            "Inactive flight overwrote frequency or retained the engine gate");

    audio.reset_engine_ramp();
    require(audio.state().engine_ramp == 0, "Explicit engine-ramp reset was ignored");
}

void test_voice2_priority_and_parallel_countdowns() {
    auto audio = controller();
    audio.start_shot();
    const auto shot = audio.advance({});
    require(voice(shot, 1) == AudioVoiceOutput{
                                  0x2000, AudioVoiceControl::TriangleGate, 10, 0, true} &&
                audio.state().shot_countdown == 0x1f,
            "Shot did not start from its source countdown");

    const auto held_shot = audio.advance({});
    require(!voice(held_shot, 1).gate_restarted &&
                audio.state().shot_countdown == 0x1e,
            "Unchanged shot envelope retriggered or failed to decrement");

    audio.start_explosion();
    auto tick = AudioControlTick{};
    tick.timer_entropy = 1;
    const auto explosion = audio.advance(tick);
    require(voice(explosion, 1) == AudioVoiceOutput{
                                       0x0200, AudioVoiceControl::NoiseGate, 15, 0, true},
            "Explosion did not override the shot with timer-derived frequency");
    require(audio.state().shot_countdown == 0x1d &&
                audio.state().explosion_countdown == 0x1e,
            "Explosion priority stopped a lower-priority countdown");

    audio.award_extra_life();
    const auto extra_life = audio.advance(tick);
    require(voice(extra_life, 1) == AudioVoiceOutput{
                                       0x2000, AudioVoiceControl::TriangleGate, 15, 0, false},
            "Extra-life effect did not override explosion and shot");
    require(audio.state().extra_life_countdown == 0x3f &&
                audio.state().explosion_countdown == 0x1d &&
                audio.state().shot_countdown == 0x1c,
            "Voice-2 priority failed to consume all active countdowns");

    auto boundary_audio = controller();
    boundary_audio.award_extra_life();
    AudioControlOutput boundary{};
    for (int count = 0; count < 16; ++count) boundary = boundary_audio.advance({});
    require(voice(boundary, 1).frequency == 0x2000,
            "Extra-life table changed before the 0x30 boundary");
    boundary = boundary_audio.advance({});
    require(voice(boundary, 1).frequency == 0x2600 &&
                boundary_audio.state().extra_life_countdown == 0x2f,
            "Extra-life table boundary differs");
}

void test_voice1_effects_and_fuel_warning() {
    auto audio = controller();
    audio.start_life_end(LifeEndAudioCause::Collision);
    auto tick = AudioControlTick{};
    tick.flight_active = false;
    const auto collision = audio.advance(tick);
    require(voice(collision, 0) == AudioVoiceOutput{
                                       0x0300, AudioVoiceControl::NoiseGate, 12, 0, true} &&
                audio.state().voice1_countdown == 0x17,
            "Collision effect lost priority over inactive flight");

    audio.reset();
    audio.start_refuel(RefuelAudioKind::FuelAdded);
    const auto refuel = audio.advance({});
    require(voice(refuel, 0) == AudioVoiceOutput{
                                    0x2000, AudioVoiceControl::TriangleGate, 8, 0, true} &&
                audio.state().voice1_countdown == 7,
            "Refuel effect does not match selector 4/countdown 8");
    audio.start_refuel(RefuelAudioKind::FuelAdded);
    require(audio.state().voice1_countdown == 7,
            "Repeated same-selector refuel restarted the source countdown");
    audio.start_refuel(RefuelAudioKind::TankSaturated);
    require(audio.state().voice1_effect == Voice1AudioEffect::RefuelTankSaturated &&
                audio.state().voice1_countdown == 8,
            "Changed refuel selector did not restart the source countdown");
    const auto saturated = audio.advance({});
    require(voice(saturated, 0) == AudioVoiceOutput{
                                       0x3f00, AudioVoiceControl::TriangleGate, 8, 0, false},
            "Saturated refuel selector did not retain frequency high 0x3f");

    audio.reset();
    tick = {};
    tick.fuel = 0x3fff;
    const auto warning = audio.advance(tick);
    require(voice(warning, 0) == AudioVoiceOutput{
                                     0x1900, AudioVoiceControl::TriangleGate, 15, 0, true} &&
                audio.state().voice1_countdown == 0x3e,
            "Fuel-warning cadence did not start from phase 63 then decrement");
    AudioControlOutput warning_boundary{};
    for (int count = 0; count < 35; ++count) warning_boundary = audio.advance(tick);
    require(audio.state().voice1_countdown == 0x1b &&
                voice(warning_boundary, 0).frequency == 0x0000 &&
                audio.state().engine_ramp == 1,
            "Fuel-warning phase did not fall back to the engine below 28");

    audio.reset();
    tick.fuel = 0;
    const auto empty_warning = audio.advance(tick);
    require(voice(empty_warning, 0).frequency == 0x1000,
            "Near-empty fuel byte rotation differs");

    audio.reset();
    tick.fuel = 0x4000;
    const auto threshold = audio.advance(tick);
    require(voice(threshold, 0).frequency == 0 && audio.state().engine_ramp == 1 &&
                audio.state().voice1_countdown == 0,
            "Fuel-warning threshold included high byte 0x40");
}

void test_voice1_effect_completion() {
    struct EffectCase {
        void (*start)(AudioController&);
        Voice1AudioEffect effect;
        std::uint8_t duration;
        std::uint16_t frequency;
        AudioVoiceControl control;
        std::uint8_t final_sustain;
    };
    const std::array cases{
        EffectCase{[](AudioController& audio) { audio.start_life_end(LifeEndAudioCause::Collision); },
                   Voice1AudioEffect::Collision, 24, 0x0300, AudioVoiceControl::NoiseGate, 0},
        EffectCase{[](AudioController& audio) { audio.start_life_end(LifeEndAudioCause::FuelExhausted); },
                   Voice1AudioEffect::FuelExhausted, 24, 0x0300, AudioVoiceControl::NoiseGate, 0},
        EffectCase{[](AudioController& audio) { audio.start_refuel(RefuelAudioKind::TankSaturated); },
                   Voice1AudioEffect::RefuelTankSaturated, 8, 0x3f00, AudioVoiceControl::TriangleGate, 1},
        EffectCase{[](AudioController& audio) { audio.start_refuel(RefuelAudioKind::FuelAdded); },
                   Voice1AudioEffect::RefuelFuelAdded, 8, 0x2000, AudioVoiceControl::TriangleGate, 1},
    };

    for (const auto& effect : cases) {
        auto audio = controller();
        effect.start(audio);
        auto tick = AudioControlTick{};
        tick.flight_active = false;
        AudioControlOutput output{};
        for (auto remaining = effect.duration; remaining != 0; --remaining) {
            require(audio.state().voice1_effect == effect.effect &&
                        audio.state().voice1_countdown == remaining,
                    "Voice-1 effect ended before its complete countdown");
            output = audio.advance(tick);
            require(voice(output, 0).frequency == effect.frequency &&
                        voice(output, 0).control == effect.control,
                    "Voice-1 effect lost its frequency or control before completion");
        }
        require(audio.state().voice1_effect == Voice1AudioEffect::None &&
                    audio.state().voice1_countdown == 0 &&
                    voice(output, 0).sustain == effect.final_sustain,
                "Voice-1 effect did not finish on its expected envelope tail");

        output = audio.advance(tick);
        require(voice(output, 0).control == AudioVoiceControl::Silent &&
                    voice(output, 0).frequency == effect.frequency &&
                    audio.state().engine_ramp == 0,
                "Completed effect restarted or changed frequency during inactive flight");
        tick.flight_active = true;
        output = audio.advance(tick);
        require(voice(output, 0).frequency == 0 &&
                    voice(output, 0).control == AudioVoiceControl::NoiseGate &&
                    audio.state().engine_ramp == 1,
                "Engine did not resume after the completed voice-1 effect");
    }
}

void test_voice3_special_and_auxiliary_effects() {
    auto audio = controller();
    auto tick = AudioControlTick{};
    tick.special_sprite_counter = 1;
    const auto first_flight = audio.advance(tick);
    require(voice(first_flight, 2) == AudioVoiceOutput{
                                          0xb900, AudioVoiceControl::TriangleGate, 12, 0, true},
            "Special-sprite flight table did not use counter index 1");

    tick.special_sprite_counter = 16;
    const auto last_flight = audio.advance(tick);
    require(voice(last_flight, 2).frequency == 0x0c00 &&
                voice(last_flight, 2).control == AudioVoiceControl::TriangleGate,
            "Special-sprite flight table end differs");

    tick.special_sprite_counter = 17;
    tick.timer_entropy = 5;
    const auto explosion = audio.advance(tick);
    require(voice(explosion, 2) == AudioVoiceOutput{
                                       0x0500, AudioVoiceControl::NoiseGate, 12, 0, false},
            "Special-sprite explosion did not use three timer bits");

    audio.reset();
    audio.start_auxiliary_sprite_effect();
    tick = {};
    const auto auxiliary = audio.advance(tick);
    require(voice(auxiliary, 2) == AudioVoiceOutput{
                                       0x0e00, AudioVoiceControl::NoiseGate, 14, 0, true} &&
                audio.state().auxiliary_sprite_countdown == 7,
            "Auxiliary sprite effect did not consume old countdown 8");
    audio.start_auxiliary_sprite_effect();
    require(audio.state().auxiliary_sprite_countdown == 7,
            "Active auxiliary sprite effect was restarted");

    audio.reset();
    audio.start_auxiliary_sprite_effect();
    tick.demo_mode = true;
    tick.special_sprite_counter = 1;
    const auto demo = audio.advance(tick);
    require(voice(demo, 2) == AudioVoiceOutput{
                                  0x0000, AudioVoiceControl::Silent, 8, 0, true} &&
                audio.state().auxiliary_sprite_countdown == 7,
            "Demo branch did not bypass auxiliary transform and special-sprite tone");
}

void test_transition_and_mute_freeze() {
    auto audio = controller();
    audio.start_transition_effect();
    audio.start_shot();
    const auto first = audio.advance({});
    require(voice(first, 0) == AudioVoiceOutput{
                                   0x2000, AudioVoiceControl::TriangleGate, 15, 0, true} &&
                audio.state().transition_countdown == 1 &&
                audio.state().shot_countdown == 0x1f,
            "Transition first tick did not flow through later voices");

    const auto second = audio.advance({});
    require(voice(second, 0).control == AudioVoiceControl::TriangleGate &&
                voice(second, 1).control == AudioVoiceControl::Silent &&
                voice(second, 2).control == AudioVoiceControl::Silent &&
                audio.state().transition_countdown == 0 &&
                audio.state().shot_countdown == 0x1f,
            "Transition second tick did not bypass and freeze later voice effects");

    auto muted_audio = controller();
    muted_audio.start_explosion();
    auto tick = AudioControlTick{};
    tick.lifecycle_muted = true;
    const auto muted = muted_audio.advance(tick);
    require(voice(muted, 0).control == AudioVoiceControl::Silent &&
                voice(muted, 1).control == AudioVoiceControl::Silent &&
                voice(muted, 2).control == AudioVoiceControl::Silent &&
                muted_audio.state().explosion_countdown == 0x1f,
            "Lifecycle mute did not freeze pending effects and close all gates");

    muted_audio.reset();
    require(muted_audio.state() == AudioControllerState{},
            "Reset retained audio controller state");
}

} // namespace

int main() {
    try {
        test_engine_ramp_and_lifecycle_gate();
        test_voice2_priority_and_parallel_countdowns();
        test_voice1_effects_and_fuel_warning();
        test_voice1_effect_completion();
        test_voice3_special_and_auxiliary_effects();
        test_transition_and_mute_freeze();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
