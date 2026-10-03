#pragma once

#include "game/player_steering.hpp"
#include "game/life_cycle.hpp"
#include "render/glyph_atlas.hpp"

#include <cstdint>

namespace river_raid {

// Draw the selected original player pose at the current steering position.
void draw_player_preview(Frame& destination, const PlayerSteeringState& state,
                         FlightPose pose, LifeCycleImage image = LifeCycleImage::Straight,
                         std::uint8_t individual_color = 6);

} // namespace river_raid
