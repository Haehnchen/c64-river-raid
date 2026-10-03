#pragma once

#include "game/flight_controls.hpp"

#include <cstdint>

namespace river_raid {

enum class FlightPose { Straight, Right, Left };

struct PlayerSteeringState {
    std::uint8_t horizontal_position{};
    std::uint8_t horizontal_fraction{};
    std::uint8_t horizontal_step{};
    std::uint8_t scroll_speed{};

    friend bool operator==(const PlayerSteeringState&, const PlayerSteeringState&) = default;
};

struct PlayerSteeringContext {
    bool use_max_fraction_for_rightward_add{};
};

struct PlayerSteeringResult {
    PlayerSteeringState state;
    FlightPose pose;
};

// One admitted steering update; round/life gating belongs to the caller.
[[nodiscard]] PlayerSteeringResult steer_player(PlayerSteeringState state, FlightControls controls,
                                                PlayerSteeringContext context = {}) noexcept;

} // namespace river_raid
