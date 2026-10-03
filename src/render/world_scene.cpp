#include "render/world_scene.hpp"

#include <array>
#include <stdexcept>

namespace river_raid {

Frame render_world_scene(const WorldSceneData& data,
                         std::span<const PackedRiverRow> rows,
                         std::span<const WorldObjectPlacement> placements,
                         std::size_t selected_option) {
    constexpr int terrain_top = 4;
    constexpr int terrain_height = 157;
    constexpr std::size_t hud_background_top = 170;
    constexpr std::size_t hud_tiles_top = 176;
    if (rows.size() != terrain_height || data.hud_background_color >= data.options.palette.size()) {
        throw std::invalid_argument("Invalid world presentation background");
    }
    auto frame = render_options_scene(data.options, rows, selected_option);
    const auto frame_width = static_cast<std::size_t>(frame.width);
    for (std::size_t y = 0; y < hud_tiles_top; ++y) {
        for (std::size_t x = 0; x < frame_width; ++x) {
            frame.pixels[y * frame_width + x] = data.options.palette[
                y < hud_background_top ? 0 : data.hud_background_color];
        }
    }
    place_frame(frame, render_river_rows(rows, data.options.terrain_colors, data.options.palette), 0, terrain_top);
    for (const auto& placement : placements) {
        if (placement.image_key >= data.image_pairs.size() || placement.style_key >= data.styles.size() ||
            (placement.image_slot != WorldObjectImageSlot::primary &&
             placement.image_slot != WorldObjectImageSlot::alternate)) {
            throw std::invalid_argument("Invalid world presentation object");
        }
        const auto& pair = data.image_pairs[placement.image_key];
        const auto index = placement.image_slot == WorldObjectImageSlot::primary ? pair.primary : pair.alternate;
        if (index >= data.images.size()) throw std::invalid_argument("Invalid world image mapping");
        const auto& source = data.images[index];
        const auto image = colorize_object_image(source.width, source.height, source.roles,
                                                data.styles[placement.style_key]);
        const std::array layers{ImageLayer{image.pixels, image.width, image.height, placement.left, placement.top}};
        draw_sprite_layers(frame, layers, data.options.palette, {0, terrain_top, 320, terrain_height});
    }
    return frame;
}

} // namespace river_raid
