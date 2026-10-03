#include "app/object_sheet.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const auto sheet = river_raid::make_object_sprite_sheet();
        if (sheet.width != 512 || sheet.height != 224 || sheet.pixels.size() != 512 * 224) {
            throw std::runtime_error("Sprite specimen sheet dimensions changed");
        }
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto pixel : sheet.pixels) {
            if ((pixel >> 24) != 255) throw std::runtime_error("Sheet contains nonopaque output pixels");
            for (const auto shift : {16, 8, 0}) {
                hash ^= (pixel >> shift) & 255U;
                hash *= 1099511628211ULL;
            }
        }
        if (hash != 0x0983E1ED0E9CAE35ULL) {
            throw std::runtime_error("Sprite specimen sheet differs from the verified RGB output");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
