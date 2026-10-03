#pragma once

#include "render/character_scene.hpp"
#include "game/river_types.hpp"

namespace river_raid {

Frame render_river_rows(std::span<const PackedRiverRow> rows,
                        const std::array<std::uint8_t, 4>& colors,
                        const Palette& palette);

} // namespace river_raid
