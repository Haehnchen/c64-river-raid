#include "render/object_image.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace river_raid {
namespace {

constexpr std::uint8_t transparent = 255;

std::uint8_t palette_index(std::uint8_t role, ObjectImageStyle style) {
    switch (role) {
    case 0: return transparent;
    case 1: return style.multicolor_1;
    case 2: return style.individual_color;
    case 3: return style.multicolor_2;
    default: throw std::invalid_argument("Invalid object image role");
    }
}

} // namespace

ObjectImage colorize_object_image(int width, int height,
                                  std::span<const std::uint8_t> roles,
                                  ObjectImageStyle style) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Object image dimensions must be positive");
    }
    if (style.individual_color >= 16 || style.multicolor_1 >= 16 ||
        style.multicolor_2 >= 16) {
        throw std::invalid_argument("Object image color is outside the palette");
    }

    const auto input_width = static_cast<std::size_t>(width);
    const auto input_height = static_cast<std::size_t>(height);
    if (input_width > std::numeric_limits<std::size_t>::max() / input_height ||
        roles.size() != input_width * input_height) {
        throw std::invalid_argument("Object image role span has wrong dimensions");
    }
    if (style.expand_horizontal && width > std::numeric_limits<int>::max() / 2) {
        throw std::invalid_argument("Expanded object image is too wide");
    }

    const auto horizontal_factor = style.expand_horizontal ? std::size_t{2} : std::size_t{1};
    if (input_width > std::numeric_limits<std::size_t>::max() / horizontal_factor ||
        input_width * horizontal_factor >
            std::numeric_limits<std::size_t>::max() / input_height) {
        throw std::invalid_argument("Object image is too large");
    }
    const auto output_width = style.expand_horizontal ? width * 2 : width;
    const auto output_size = static_cast<std::size_t>(output_width) * input_height;
    ObjectImage image{output_width, height, {}};
    image.pixels.reserve(output_size);
    for (const auto role : roles) {
        const auto color = palette_index(role, style);
        image.pixels.push_back(color);
        if (style.expand_horizontal) image.pixels.push_back(color);
    }
    return image;
}

} // namespace river_raid
