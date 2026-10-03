#pragma once

#include "game/world_objects.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace river_raid {

// Coordinates use VIC pixels. The native 320x200 viewport origin is (24, 50).
struct AuxiliaryProjectilePresentation {
    std::uint16_t vic_x{};
    std::uint8_t vic_y{};

    friend bool operator==(const AuxiliaryProjectilePresentation&,
                           const AuxiliaryProjectilePresentation&) = default;
};

struct AuxiliaryProjectileSpawn {
    std::uint8_t generator_phase{};
};

struct AuxiliaryProjectileTick {
    const WorldObjectState& objects;
    bool demo_mode{};
    bool lifecycle_active{true};
    bool audio_effect_idle{true};
    std::array<std::uint8_t, 6> terrain_contact_samples{};
    std::array<std::uint8_t, 2> player_contact_samples{};
};

struct AuxiliaryProjectileTickResult {
    bool audio_started{};
    bool fatal_player_contact{};

    friend bool operator==(const AuxiliaryProjectileTickResult&,
                           const AuxiliaryProjectileTickResult&) = default;
};

struct AuxiliaryProjectileState {
    // One horizontal coordinate unit spans two VIC pixels.
    std::uint8_t horizontal_coordinate{};
    std::uint8_t vertical_position{};
    // Presence means the nonblank image is published. It can remain published
    // at a clipped coordinate while the producer is temporarily ineligible.
    std::optional<AuxiliaryProjectilePresentation> presentation;

    friend bool operator==(const AuxiliaryProjectileState&,
                           const AuxiliaryProjectileState&) = default;
};

class AuxiliaryProjectileSystem {
public:
    // Existing Y scrolls first; a new source selection then replaces X/Y.
    void advance_after_row(std::optional<AuxiliaryProjectileSpawn> spawn = std::nullopt);
    [[nodiscard]] AuxiliaryProjectileTickResult advance_tick(
        const AuxiliaryProjectileTick& tick);

    // The source crash boundary clears stored and published Y only.
    void hide_for_life_reveal() noexcept;
    void reset() noexcept;

    [[nodiscard]] const AuxiliaryProjectileState& state() const noexcept;

private:
    [[nodiscard]] AuxiliaryProjectileTickResult publish(
        std::uint8_t published_horizontal_coordinate,
        const AuxiliaryProjectileTick& tick);

    AuxiliaryProjectileState state_{};
};

} // namespace river_raid
