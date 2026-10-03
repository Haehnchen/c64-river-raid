#include "render/character_scene.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::array<river_raid::Glyph, 2> glyphs{};
        glyphs[1][0] = 0x80;
        glyphs[1][7] = 0x01;
        std::array<std::uint8_t, 1000> cells{};
        std::array<std::uint8_t, 1000> colors{};
        cells[999] = 1;
        colors[999] = 2;
        river_raid::Palette palette{};
        palette.fill(0xff000000);
        palette[1] = 0xffffffff;
        palette[2] = 0xffaa3355;
        const std::array<std::uint8_t, 4> pixels{1, 255, 255, 1};
        const std::array<river_raid::ImageLayer, 1> overlays{{{pixels, 2, 2, -1, -1}}};
        const river_raid::CharacterScene scene{glyphs, cells, colors, palette, overlays};
        auto frame = river_raid::render_character_scene(scene);
        require(frame.width == 320 && frame.height == 200, "Wrong scene dimensions");
        require(frame.pixels[0] == palette[1], "Negative overlay clipping failed");
        require(frame.pixels[1] == palette[0], "Transparent/outside pixel overwrote background");
        require(frame.pixels[192 * 320 + 312] == palette[2], "Last cell starts at wrong position");
        require(frame.pixels.back() == palette[2], "Last cell glyph row is wrong");
        const auto minimum = std::numeric_limits<int>::min();
        const auto maximum = std::numeric_limits<int>::max();
        const std::array extreme_overlays{
            river_raid::ImageLayer{pixels, 2, 2, maximum, maximum},
            river_raid::ImageLayer{pixels, 2, 2, minimum, minimum},
        };
        const auto extreme_frame = river_raid::render_character_scene(
            {glyphs, cells, colors, palette, extreme_overlays});
        const auto base_frame = river_raid::render_character_scene(
            {glyphs, cells, colors, palette, {}});
        require(extreme_frame.pixels == base_frame.pixels,
                "Offscreen extreme overlays changed the scene");
        cells[0] = 2;
        bool rejected = false;
        try { (void)river_raid::render_character_scene(scene); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Undefined glyph must not read outside the asset");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
