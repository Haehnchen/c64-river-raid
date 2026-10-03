#include "render/sprite_scene.hpp"

#include <climits>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        using namespace river_raid;
        Palette palette{};
        palette[1] = 0xff123456;
        palette[2] = 0xff789abc;
        Frame frame{4, 4, std::vector<std::uint32_t>(16, 0xff000000)};
        constexpr std::array<std::uint8_t, 4> pixels{1, 255, 2, 1};
        const std::array layers{ImageLayer{pixels, 2, 2, -1, 0},
                                ImageLayer{pixels, 2, 2, 1, 1}};
        draw_sprite_layers(frame, layers, palette, {0, 0, 4, 3});
        require(frame.pixels[0] == 0xff000000, "Transparent clipped edge");
        require(frame.pixels[4] == palette[1], "Negative origin clipping");
        require(frame.pixels[5] == palette[1] && frame.pixels[9] == palette[2], "Sprite placement");
        require(frame.pixels[12] == 0xff000000, "Clip must preserve HUD area");
        const std::array overlap{ImageLayer{pixels, 2, 2, 1, 0}};
        draw_sprite_layers(frame, overlap, palette, {0, 0, 4, 4});
        require(frame.pixels[5] == palette[2], "Later sprite priority");
        const auto before = frame.pixels;
        const std::array outside{ImageLayer{pixels, 2, 2, INT_MAX, INT_MIN}};
        draw_sprite_layers(frame, outside, palette, {0, 0, 4, 4});
        require(frame.pixels == before, "Offscreen sprite altered the frame");
        bool rejected = false;
        constexpr std::array<std::uint8_t, 1> bad{16};
        try {
            const std::array invalid{ImageLayer{bad, 1, 1, 0, 0}};
            draw_sprite_layers(frame, invalid, palette, {0, 0, 4, 4});
        } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Out-of-range sprite palette index accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
