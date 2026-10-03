#include "render/tile_scene.hpp"

#include <stdexcept>

namespace river_raid {

Frame render_tile_scene(const TileScene& scene) {
    if (scene.columns <= 0 || scene.columns > 40 || scene.rows <= 0 || scene.rows > 25 ||
        scene.cells.size() != static_cast<std::size_t>(scene.columns * scene.rows)) {
        throw std::invalid_argument("Invalid tile scene dimensions");
    }
    Frame frame{scene.columns * 8, scene.rows * 8,
                std::vector<std::uint32_t>(scene.cells.size() * 64)};
    const auto columns = static_cast<std::size_t>(scene.columns);
    const auto frame_width = static_cast<std::size_t>(frame.width);
    for (std::size_t cell = 0; cell < scene.cells.size(); ++cell) {
        if (scene.cells[cell] >= scene.tiles.size()) {
            throw std::invalid_argument("Tile index out of range");
        }
        const auto& tile = scene.tiles[scene.cells[cell]];
        const auto left = (cell % columns) * 8;
        const auto top = (cell / columns) * 8;
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                const auto color = tile[y * 8 + x];
                if (color >= scene.palette.size()) {
                    throw std::invalid_argument("Tile color out of range");
                }
                frame.pixels[(top + y) * frame_width + left + x] = scene.palette[color];
            }
        }
    }
    return frame;
}

void place_frame(Frame& destination, const Frame& source, int left, int top) {
    const auto valid = [](const Frame& frame) {
        return frame.width > 0 && frame.height > 0 &&
            frame.pixels.size() == static_cast<std::size_t>(frame.width) *
                                   static_cast<std::size_t>(frame.height);
    };
    if (!valid(destination) || !valid(source) || left < 0 || top < 0 ||
        left > destination.width || top > destination.height ||
        source.width > destination.width - left || source.height > destination.height - top) {
        throw std::invalid_argument("Frame placement outside destination");
    }
    const auto source_width = static_cast<std::size_t>(source.width);
    const auto source_height = static_cast<std::size_t>(source.height);
    const auto destination_width = static_cast<std::size_t>(destination.width);
    const auto destination_left = static_cast<std::size_t>(left);
    const auto destination_top = static_cast<std::size_t>(top);
    for (std::size_t y = 0; y < source_height; ++y) {
        for (std::size_t x = 0; x < source_width; ++x) {
            destination.pixels[(destination_top + y) * destination_width + destination_left + x] =
                source.pixels[y * source_width + x];
        }
    }
}

} // namespace river_raid
