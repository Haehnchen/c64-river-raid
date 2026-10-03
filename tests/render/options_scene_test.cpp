#include "render/options_scene.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        river_raid::Palette palette{};
        palette[1] = 0xffffffff;
        palette[5] = 0xff5ed638;
        palette[7] = 0xffffff3b;
        std::array<river_raid::ColorTile, 2> tiles{};
        tiles[1].fill(1);
        std::array<std::uint8_t, 120> cells{};
        constexpr std::array<std::uint8_t, 2> options{0, 1};
        std::array<river_raid::PackedRiverRow, 160> terrain{};
        for (auto& row : terrain) row.fill(0xaa);
        const river_raid::OptionsSceneData data{tiles, cells, options, 42, {0, 14, 5, 3}, palette};
        const auto first = river_raid::render_options_scene(data, terrain, 0);
        const auto second = river_raid::render_options_scene(data, terrain, 1);
        if (first.pixels[0] != palette[5] || first.pixels[160 * 320] != palette[0] ||
            first.pixels[184 * 320 + 16] != palette[0] ||
            second.pixels[184 * 320 + 16] != palette[1] || cells[42] != 0) {
            throw std::runtime_error("Terrain/HUD placement or immutable asset contract");
        }
        bool rejected = false;
        try { (void)river_raid::render_options_scene(data, terrain, 2); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid selection accepted");
        constexpr std::array<std::uint8_t, 2> marker{7, 255};
        auto marked = data;
        marked.fuel_marker = {marker, 2, 1, 16, 184};
        const auto with_marker = river_raid::render_options_scene(marked, terrain, 1);
        if (with_marker.pixels[184 * 320 + 16] != palette[7] ||
            with_marker.pixels[184 * 320 + 17] != palette[1]) {
            throw std::runtime_error("Marker color or transparency");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
