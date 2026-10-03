#include "render/river_scene.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        const std::array<std::uint8_t, 4> colors{0, 14, 5, 3};
        river_raid::Palette palette{};
        palette[14] = 0xff8178ff;
        palette[5] = 0xff5ed638;
        palette[3] = 0xff87f0cb;
        std::array<river_raid::PackedRiverRow, 2> rows{};
        rows[0][0] = 0x1b;
        rows[1][39] = 0xe4;
        const auto frame = river_raid::render_river_rows(rows, colors, palette);
        if (frame.width != 320 || frame.height != 2) throw std::runtime_error("Dimensions");
        for (int pair = 0; pair < 4; ++pair) {
            for (int repeat = 0; repeat < 2; ++repeat) {
                if (frame.pixels[pair * 2 + repeat] != palette[colors[pair]] ||
                    frame.pixels[632 + pair * 2 + repeat] != palette[colors[3 - pair]]) {
                    throw std::runtime_error("Pixel order, horizontal doubling, or row boundary");
                }
            }
        }
        bool rejected = false;
        try { (void)river_raid::render_river_rows({}, colors, palette); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Empty rows accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
