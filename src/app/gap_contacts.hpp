#pragma once

#include "game/bridge_sprites.hpp"
#include "game/gap_contact.hpp"
#include "game/player_steering.hpp"
#include "game/life_cycle.hpp"

namespace river_raid {

// Visible-scene approximation using shipped masks and scheduled object roles.
[[nodiscard]] GapContactSnapshot sample_gap_contacts(
    std::uint8_t phase, const WorldObjectState& objects, const BridgeSpriteState& bridge,
    const PlayerSteeringState& player, FlightPose pose,
    LifeCycleImage image = LifeCycleImage::Straight);

} // namespace river_raid
