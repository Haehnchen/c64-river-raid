#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace river_raid {

enum class AudioVoiceControl : std::uint8_t {
    Silent = 0x00,
    TriangleGate = 0x11,
    NoiseGate = 0x81,
};

struct AudioVoiceOutput {
    std::uint16_t frequency{};
    AudioVoiceControl control{AudioVoiceControl::Silent};
    std::uint8_t sustain{};
    std::uint8_t release{};
    bool gate_restarted{};

    friend bool operator==(const AudioVoiceOutput&, const AudioVoiceOutput&) = default;
};

struct AudioControlOutput {
    std::array<AudioVoiceOutput, 3> voices{};

    friend bool operator==(const AudioControlOutput&, const AudioControlOutput&) = default;
};

struct AudioControlTables {
    std::span<const std::uint8_t, 4> engine_voice1_selector;
    std::span<const std::uint8_t, 4> effect_voice2_frequency_high;
    std::span<const std::uint8_t, 17> special_voice3_frequency_high;
};

enum class LifeEndAudioCause : std::uint8_t {
    Collision,
    FuelExhausted,
};

enum class RefuelAudioKind : std::uint8_t {
    TankSaturated,
    FuelAdded,
};

enum class Voice1AudioEffect : std::uint8_t {
    None,
    Collision,
    FuelExhausted,
    RefuelTankSaturated,
    RefuelFuelAdded,
};

struct AudioControlTick {
    std::uint8_t scroll_speed{0x80};
    bool accelerating{};
    bool decelerating{};
    std::uint16_t fuel{0xffff};
    std::uint8_t special_sprite_counter{0xff};
    std::uint8_t timer_entropy{};
    bool flight_active{true};
    bool demo_mode{};
    bool lifecycle_muted{};
};

struct AudioControllerState {
    AudioControlOutput output{};
    Voice1AudioEffect voice1_effect{Voice1AudioEffect::None};
    std::uint8_t voice1_countdown{};
    std::uint8_t shot_countdown{};
    std::uint8_t explosion_countdown{};
    std::uint8_t extra_life_countdown{};
    std::uint8_t auxiliary_sprite_countdown{};
    std::uint8_t transition_countdown{};
    std::uint8_t engine_ramp{};

    friend bool operator==(const AudioControllerState&, const AudioControllerState&) = default;
};

class AudioController {
public:
    explicit AudioController(AudioControlTables tables) noexcept;

    void start_life_end(LifeEndAudioCause cause) noexcept;
    void start_refuel(RefuelAudioKind kind) noexcept;
    void start_shot() noexcept;
    void start_explosion() noexcept;
    void award_extra_life() noexcept;
    void start_auxiliary_sprite_effect() noexcept;
    void start_transition_effect() noexcept;
    void reset_engine_ramp() noexcept;

    [[nodiscard]] AudioControlOutput advance(const AudioControlTick& tick) noexcept;
    [[nodiscard]] const AudioControllerState& state() const noexcept;
    void reset() noexcept;

private:
    AudioControlTables tables_;
    AudioControllerState state_{};
    std::array<std::uint8_t, 3> previous_sustain_release_{};
};

} // namespace river_raid
