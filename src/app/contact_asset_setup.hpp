#pragma once

#include "game/player_steering.hpp"
#include "game/life_cycle.hpp"
#include "game/river_types.hpp"
#include "render/contact_masks.hpp"
#include "render/world_object_schedule.hpp"

#include <span>

namespace river_raid {

// Asset handoff only; positions and sample timing belong to the caller.
[[nodiscard]] OccupancyMask make_world_object_contact_mask(const WorldObjectPlacement& placement);
[[nodiscard]] OccupancyMask make_player_contact_mask(
    FlightPose pose, LifeCycleImage image = LifeCycleImage::Straight);
[[nodiscard]] OccupancyMask make_player_detail_contact_mask();
[[nodiscard]] OccupancyMask make_projectile_contact_mask();
[[nodiscard]] OccupancyMask make_river_contact_mask(std::span<const PackedRiverRow> rows);

} // namespace river_raid
