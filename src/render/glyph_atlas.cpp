#include "render/glyph_atlas.hpp"

#include <stdexcept>

namespace river_raid {

Frame render_glyph_atlas(std::span<const Glyph> glyphs, int columns) {
    if (columns <= 0 || columns > 256 || glyphs.empty() || glyphs.size() > 256) {
        throw std::invalid_argument("Atlas needs 1..256 glyphs and columns");
    }
    constexpr int cell_size = 10;
    const auto rows = (static_cast<int>(glyphs.size()) + columns - 1) / columns;
    Frame frame{columns * cell_size, rows * cell_size, {}};
    const auto column_count = static_cast<std::size_t>(columns);
    const auto frame_width = static_cast<std::size_t>(frame.width);
    frame.pixels.assign(frame_width * static_cast<std::size_t>(frame.height), 0xff242424u);
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
        const auto left = (index % column_count) * cell_size + 1;
        const auto top = (index / column_count) * cell_size + 1;
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                const bool lit = (glyphs[index][y] & (0x80u >> x)) != 0;
                frame.pixels[(top + y) * frame_width + left + x] =
                    lit ? 0xffffffffu : 0xff000000u;
            }
        }
    }
    return frame;
}

} // namespace river_raid
