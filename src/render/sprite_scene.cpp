#include "render/sprite_scene.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace river_raid {

void draw_sprite_layers(Frame& destination, std::span<const ImageLayer> layers,
                        const Palette& palette, ClipRectangle clip) {
    if (destination.width <= 0 || destination.height <= 0 ||
        destination.pixels.size() != static_cast<std::size_t>(destination.width) *
                                     static_cast<std::size_t>(destination.height) ||
        clip.width < 0 || clip.height < 0) {
        throw std::invalid_argument("Invalid sprite destination or clip");
    }
    const auto destination_width = static_cast<std::size_t>(destination.width);
    using Wide = std::int64_t;
    const auto left = std::clamp<Wide>(clip.left, 0, destination.width);
    const auto top = std::clamp<Wide>(clip.top, 0, destination.height);
    const auto right = std::clamp<Wide>(static_cast<Wide>(clip.left) + clip.width, 0, destination.width);
    const auto bottom = std::clamp<Wide>(static_cast<Wide>(clip.top) + clip.height, 0, destination.height);
    for (const auto& image : layers) {
        if (image.width <= 0 || image.height <= 0 ||
            image.pixels.size() != static_cast<std::size_t>(image.width) *
                                   static_cast<std::size_t>(image.height)) {
            throw std::invalid_argument("Malformed sprite pixels");
        }
        for (const auto color : image.pixels) {
            if (color != 255 && color >= palette.size()) throw std::invalid_argument("Invalid sprite color");
        }
        const auto x0 = std::max<Wide>(left, image.left);
        const auto y0 = std::max<Wide>(top, image.top);
        const auto x1 = std::min<Wide>(right, static_cast<Wide>(image.left) + image.width);
        const auto y1 = std::min<Wide>(bottom, static_cast<Wide>(image.top) + image.height);
        if (x0 >= x1 || y0 >= y1) continue;
        const auto width = static_cast<std::size_t>(x1 - x0);
        const auto height = static_cast<std::size_t>(y1 - y0);
        const auto image_width = static_cast<std::size_t>(image.width);
        const auto source_begin = static_cast<std::size_t>(y0 - image.top) * image_width +
                                  static_cast<std::size_t>(x0 - image.left);
        const auto destination_begin = static_cast<std::size_t>(y0) * destination_width +
                                       static_cast<std::size_t>(x0);
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const auto color = image.pixels[source_begin + y * image_width + x];
                if (color != 255) {
                    destination.pixels[destination_begin + y * destination_width + x] = palette[color];
                }
            }
        }
    }
}

} // namespace river_raid
