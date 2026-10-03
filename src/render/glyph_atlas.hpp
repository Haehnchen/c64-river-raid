#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace river_raid {

using Glyph = std::array<std::uint8_t, 8>;

struct Frame {
    int width{};
    int height{};
    std::vector<std::uint32_t> pixels;
};

// Pixels are opaque ARGB; each glyph's most significant bit is leftmost.
Frame render_glyph_atlas(std::span<const Glyph> glyphs, int columns = 16);

} // namespace river_raid
