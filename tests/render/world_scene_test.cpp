#include "render/world_scene.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace river_raid;
    try {
        Palette palette{};
        for (std::size_t i = 0; i < palette.size(); ++i) palette[i] = 0xff000000 | i;
        std::array<ColorTile, 1> tiles{};
        tiles[0].fill(6);
        const std::array<std::uint8_t, 120> cells{};
        const std::array<std::uint8_t, 1> options{};
        std::array<PackedRiverRow, 157> rows{};
        for (auto& row : rows) row.fill(0x55);
        const std::array<std::uint8_t, 4> primary{2, 0, 1, 3};
        const std::array<std::uint8_t, 4> alternate{3, 3, 3, 3};
        const std::array images{WorldRoleImage{2, 2, primary}, WorldRoleImage{2, 2, alternate}};
        const std::array pairs{WorldImagePair{0, 1}};
        const std::array styles{ObjectImageStyle{7, 8, 9, true}};
        const WorldSceneData data{{tiles, cells, options, 0, {0, 1, 2, 3}, palette, {}},
                                  images, pairs, styles, 12};
        auto frame = render_world_scene(data, rows, {});
        for (int y = 0; y < 200; ++y) {
            const auto color = y < 4 ? 0 : y < 161 ? 1 : y < 170 ? 0 : y < 176 ? 12 : 6;
            require(frame.pixels[y * 320] == palette[color], "World background bands differ");
        }
        const std::array placements{
            WorldObjectPlacement{0, WorldObjectImageSlot::primary, 4, 3, 0, 0},
            WorldObjectPlacement{1, WorldObjectImageSlot::alternate, 10, 160, 0, 0}};
        frame = render_world_scene(data, rows, placements);
        require(frame.pixels[3 * 320 + 4] == palette[0], "Object escaped top clip");
        require(frame.pixels[4 * 320 + 4] == palette[8] && frame.pixels[4 * 320 + 5] == palette[8] &&
                frame.pixels[4 * 320 + 6] == palette[9], "Object expansion or colors differ");
        require(frame.pixels[160 * 320 + 10] == palette[9] && frame.pixels[161 * 320 + 10] == palette[0],
                "Object escaped bottom clip");
        auto invalid = placements;
        invalid[0].image_key = 1;
        bool rejected = false;
        try { (void)render_world_scene(data, rows, invalid); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid image key accepted");
        rejected = false;
        try { (void)render_world_scene(data, std::span(rows).first(156), {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid terrain height accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
