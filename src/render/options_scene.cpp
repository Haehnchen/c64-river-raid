#include "render/options_scene.hpp"

#include <stdexcept>

namespace river_raid {

Frame render_options_scene(const OptionsSceneData& data,
                           std::span<const PackedRiverRow> rows,
                           std::size_t selected_option) {
    if (data.hud_cells.size() != 120 || data.option_cell >= 120 ||
        selected_option >= data.option_tiles.size() || rows.size() > 160) {
        throw std::invalid_argument("Invalid options scene data");
    }
    std::vector<std::uint8_t> cells(data.hud_cells.begin(), data.hud_cells.end());
    cells[data.option_cell] = data.option_tiles[selected_option];
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, data.palette[0])};
    place_frame(frame, render_river_rows(rows, data.terrain_colors, data.palette), 0, 0);
    place_frame(frame, render_tile_scene({data.hud_tiles, cells, 40, 3, data.palette}), 0, 176);
    const auto& marker = data.fuel_marker;
    if (!marker.pixels.empty()) {
        if (marker.width <= 0 || marker.height <= 0 || marker.width > 320 || marker.height > 200 ||
            marker.left < 0 || marker.top < 0 || marker.left > 320 - marker.width ||
            marker.top > 200 - marker.height ||
            marker.pixels.size() != static_cast<std::size_t>(marker.width * marker.height)) {
            throw std::invalid_argument("Invalid fixed HUD marker");
        }
        const auto marker_width = static_cast<std::size_t>(marker.width);
        const auto marker_height = static_cast<std::size_t>(marker.height);
        const auto marker_left = static_cast<std::size_t>(marker.left);
        const auto marker_top = static_cast<std::size_t>(marker.top);
        for (std::size_t y = 0; y < marker_height; ++y) {
            for (std::size_t x = 0; x < marker_width; ++x) {
                const auto color = marker.pixels[y * marker_width + x];
                if (color == 255) continue;
                if (color >= data.palette.size()) throw std::invalid_argument("Invalid marker color");
                frame.pixels[(marker_top + y) * 320 + marker_left + x] = data.palette[color];
            }
        }
    }
    return frame;
}

} // namespace river_raid
