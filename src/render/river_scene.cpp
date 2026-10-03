#include "render/river_scene.hpp"

#include <stdexcept>

namespace river_raid {

Frame render_river_rows(std::span<const PackedRiverRow> rows,
                        const std::array<std::uint8_t, 4>& colors,
                        const Palette& palette) {
    if (rows.empty() || rows.size() > 200) {
        throw std::invalid_argument("River view requires one to 200 rows");
    }
    for (const auto color : colors) {
        if (color >= palette.size()) throw std::invalid_argument("Invalid river palette index");
    }
    Frame frame{320, static_cast<int>(rows.size()), std::vector<std::uint32_t>(rows.size() * 320)};
    for (std::size_t y = 0; y < rows.size(); ++y) {
        for (std::size_t column = 0; column < 40; ++column) {
            const auto packed = rows[y][column];
            for (std::size_t pair = 0; pair < 4; ++pair) {
                const auto color = colors[(packed >> (6 - pair * 2)) & 3];
                const auto index = y * 320 + column * 8 + pair * 2;
                frame.pixels[index] = palette[color];
                frame.pixels[index + 1] = palette[color];
            }
        }
    }
    return frame;
}

} // namespace river_raid
