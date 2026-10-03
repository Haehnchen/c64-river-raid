#pragma once

#include "render/river_scene.hpp"
#include "render/tile_scene.hpp"

namespace river_raid {

struct OptionsSceneData {
    std::span<const ColorTile> hud_tiles;
    std::span<const std::uint8_t> hud_cells;
    std::span<const std::uint8_t> option_tiles;
    std::size_t option_cell{};
    std::array<std::uint8_t, 4> terrain_colors;
    const Palette& palette;
    ImageLayer fuel_marker{};
};

Frame render_options_scene(const OptionsSceneData& data,
                           std::span<const PackedRiverRow> rows,
                           std::size_t selected_option);

} // namespace river_raid
