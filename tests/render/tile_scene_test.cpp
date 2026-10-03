#include "render/tile_scene.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("Tile scene test failed");
}
}

int main() {
    try {
        river_raid::Palette palette{};
        palette[1] = 0xff123456;
        palette[2] = 0xffabcdef;
        std::array<river_raid::ColorTile, 2> tiles{};
        tiles[0].fill(1);
        tiles[1].fill(2);
        constexpr std::array<std::uint8_t, 4> cells{0, 1, 1, 0};
        const auto frame = river_raid::render_tile_scene({tiles, cells, 2, 2, palette});
        require(frame.width == 16 && frame.height == 16);
        require(frame.pixels[0] == palette[1] && frame.pixels[8] == palette[2]);
        require(frame.pixels[128] == palette[2] && frame.pixels[255] == palette[1]);
        river_raid::Frame output{32, 24, std::vector<std::uint32_t>(32 * 24)};
        river_raid::place_frame(output, frame, 16, 8);
        require(output.pixels[0] == 0 && output.pixels[8 * 32 + 16] == palette[1]);
        require(output.pixels.back() == palette[1]);
        bool rejected = false;
        try { river_raid::place_frame(output, frame, 17, 8); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected);
        tiles[0][0] = 16;
        rejected = false;
        try { (void)river_raid::render_tile_scene({tiles, cells, 2, 2, palette}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
