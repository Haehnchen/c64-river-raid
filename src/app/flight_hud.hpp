#pragma once

#include "render/glyph_atlas.hpp"

#include <cstdint>

namespace river_raid {

// Draw the 320x200 round HUD. Fuel is 16-bit; the bridge field wraps at 10000.
void draw_flight_hud(Frame& destination, std::uint32_t score, int lives,
                     std::uint16_t fuel, std::uint16_t bridge_number,
                     std::uint32_t secondary_score = 0, bool two_players = false);

} // namespace river_raid
