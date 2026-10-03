#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace river_raid {

struct ObjectImageStyle {
    std::uint8_t individual_color{};
    std::uint8_t multicolor_1{};
    std::uint8_t multicolor_2{};
    bool expand_horizontal{};
};

struct ObjectImage {
    int width{};
    int height{};
    std::vector<std::uint8_t> pixels;
};

// Convert roles 0..3 to palette indices; 0 becomes transparent (255).
// Horizontal expansion duplicates each converted pixel once.
ObjectImage colorize_object_image(int width, int height,
                                  std::span<const std::uint8_t> roles,
                                  ObjectImageStyle style);

} // namespace river_raid
