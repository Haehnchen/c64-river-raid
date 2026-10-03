#include "render/character_scene.hpp"

#include <cstdint>
#include <stdexcept>

namespace river_raid {

Frame render_character_scene(const CharacterScene& scene) {
    if (scene.cells.size() != 1000 || scene.colors.size() != 1000) {
        throw std::invalid_argument("Character scene needs 40 by 25 cells and colors");
    }
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, scene.palette[0])};
    const auto frame_width = static_cast<std::size_t>(frame.width);
    for (std::size_t index = 0; index < scene.cells.size(); ++index) {
        if (scene.cells[index] >= scene.glyphs.size() || scene.colors[index] >= scene.palette.size()) {
            throw std::invalid_argument("Character or color index is out of range");
        }
        const auto& glyph = scene.glyphs[scene.cells[index]];
        const auto color = scene.palette[scene.colors[index]];
        const auto left = (index % 40) * 8;
        const auto top = (index / 40) * 8;
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                if ((glyph[y] & (0x80u >> x)) != 0) {
                    frame.pixels[(top + y) * frame_width + left + x] = color;
                }
            }
        }
    }
    for (const auto& layer : scene.overlays) {
        if (layer.width <= 0 || layer.height <= 0 || layer.width > 320 || layer.height > 200 ||
            layer.pixels.size() != static_cast<std::size_t>(layer.width * layer.height)) {
            throw std::invalid_argument("Invalid image layer dimensions");
        }
        const auto layer_width = static_cast<std::size_t>(layer.width);
        const auto layer_height = static_cast<std::size_t>(layer.height);
        for (std::size_t y = 0; y < layer_height; ++y) {
            for (std::size_t x = 0; x < layer_width; ++x) {
                const auto color = layer.pixels[y * layer_width + x];
                if (color == 255) {
                    continue;
                }
                if (color >= scene.palette.size()) {
                    throw std::invalid_argument("Invalid image layer palette index");
                }
                const auto destination_x = static_cast<std::int64_t>(layer.left) +
                                           static_cast<std::int64_t>(x);
                const auto destination_y = static_cast<std::int64_t>(layer.top) +
                                           static_cast<std::int64_t>(y);
                if (destination_x >= 0 && destination_x < frame.width &&
                    destination_y >= 0 && destination_y < frame.height) {
                    const auto destination_index = static_cast<std::size_t>(destination_y) *
                                                   frame_width +
                                                   static_cast<std::size_t>(destination_x);
                    frame.pixels[destination_index] = scene.palette[color];
                }
            }
        }
    }
    return frame;
}

} // namespace river_raid
