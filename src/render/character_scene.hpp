#pragma once

#include "render/glyph_atlas.hpp"

namespace river_raid {

using Palette = std::array<std::uint32_t, 16>;

struct ImageLayer {
    std::span<const std::uint8_t> pixels;
    int width{};
    int height{};
    int left{};
    int top{};
};

struct CharacterScene {
    std::span<const Glyph> glyphs;
    std::span<const std::uint8_t> cells;
    std::span<const std::uint8_t> colors;
    const Palette& palette;
    std::span<const ImageLayer> overlays;
};

// Image layers use palette indices and 255 for transparent pixels.
Frame render_character_scene(const CharacterScene& scene);

} // namespace river_raid
