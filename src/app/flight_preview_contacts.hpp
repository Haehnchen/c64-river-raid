#pragma once

#include "game/player_shot.hpp"
#include "game/player_steering.hpp"
#include "game/river_world.hpp"
#include "game/shot_contact.hpp"

#include <array>

namespace river_raid {

// Samples the single-state scene shown by FlightPreview. This is a visible
// snapshot approximation, not reconstruction of the original raster timeline.
[[nodiscard]] std::array<ShotContactSlot, kShotContactSlotCount>
sample_flight_preview_shot_contacts(const RiverWorld& world,
                                    const PlayerSteeringState& player,
                                    const PlayerShotResult& shot);

} // namespace river_raid
