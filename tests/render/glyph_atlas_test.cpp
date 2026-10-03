#include "render/glyph_atlas.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    try {
        std::array<river_raid::Glyph, 3> glyphs{};
        glyphs[0][0] = 0x80;
        glyphs[1][7] = 0x01;
        glyphs[2][0] = 0xff;
        const auto frame = river_raid::render_glyph_atlas(glyphs, 2);
        const auto pixel = [&](int x, int y) { return frame.pixels[y * frame.width + x]; };
        require(frame.width == 20 && frame.height == 20, "Partial atlas row has wrong size");
        require(pixel(1, 1) == 0xffffffff && pixel(8, 1) == 0xff000000, "Glyph bit order is reversed");
        require(pixel(18, 8) == 0xffffffff, "Last glyph bit must stay in its cell");
        require(pixel(1, 11) == 0xffffffff && pixel(8, 11) == 0xffffffff, "Row wrapping failed");
        require(pixel(0, 0) == 0xff242424 && pixel(11, 11) == 0xff242424, "Padding was overwritten");
        bool rejected = false;
        try { (void)river_raid::render_glyph_atlas(glyphs, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Zero columns must be rejected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
