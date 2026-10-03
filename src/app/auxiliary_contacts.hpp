#pragma once

#include "game/player_steering.hpp"
#include "game/river_world.hpp"
#include "game/life_cycle.hpp"

#include <array>
#include <cstdint>

namespace river_raid {

struct AuxiliaryContactSnapshot {
    std::array<std::uint8_t, 6> terrain{};
    std::array<std::uint8_t, 2> player{};

    friend bool operator==(const AuxiliaryContactSnapshot&,
                           const AuxiliaryContactSnapshot&) = default;
};

[[nodiscard]] AuxiliaryContactSnapshot sample_auxiliary_projectile_contacts(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose,
    LifeCycleImage image = LifeCycleImage::Straight);

} // namespace river_raid
