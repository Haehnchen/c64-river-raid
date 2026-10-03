#pragma once

#include "render/character_scene.hpp"

namespace river_raid {

using ColorTile = std::array<std::uint8_t, 64>;

struct TileScene {
    std::span<const ColorTile> tiles;
    std::span<const std::uint8_t> cells;
    int columns{};
    int rows{};
    const Palette& palette;
};

// Tiles contain final palette indices, one per pixel.
Frame render_tile_scene(const TileScene& scene);
void place_frame(Frame& destination, const Frame& source, int left, int top);

} // namespace river_raid
