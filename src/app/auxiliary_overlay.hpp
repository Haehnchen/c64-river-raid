#pragma once

#include "game/auxiliary_projectile.hpp"
#include "render/character_scene.hpp"
#include "render/contact_masks.hpp"

namespace river_raid {

[[nodiscard]] OccupancyMask make_auxiliary_projectile_contact_mask();
void draw_auxiliary_projectile(Frame& frame, const AuxiliaryProjectileState& state);

} // namespace river_raid
