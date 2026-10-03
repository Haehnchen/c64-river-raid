#pragma once

#include "game/player_shot.hpp"
#include "game/player_steering.hpp"
#include "render/glyph_atlas.hpp"

namespace river_raid {

void draw_shot_preview(Frame& destination, const PlayerSteeringState& player,
                       const PlayerShotResult& shot);

} // namespace river_raid
