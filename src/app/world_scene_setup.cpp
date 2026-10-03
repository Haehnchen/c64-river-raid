#include "app/world_scene_setup.hpp"
#include "game/demo_presentation.hpp"

#include "game_data.hpp"
#include "object_sprites.hpp"
#include "presentation_palette.hpp"
#include "render/world_scene.hpp"

#include <array>
#include <vector>

namespace river_raid {

Frame make_world_scene(std::span<const PackedRiverRow> rows,
                       std::span<const WorldObjectPlacement> placements,
                       std::size_t selected_option, bool bridge_flash) {
    namespace data = assets;
    namespace sprites = assets::object_sprites;

    std::array<WorldRoleImage, sprites::sprite_images.size()> images{};
    for (std::size_t index = 0; index < images.size(); ++index) {
        const auto& source = sprites::sprite_images[index];
        images[index] = {source.width, source.height, source.roles};
    }

    std::array<WorldImagePair, sprites::object_sprites.size()> image_pairs{};
    for (std::size_t index = 0; index < image_pairs.size(); ++index) {
        const auto& source = sprites::object_sprites[index];
        image_pairs[index] = {source.primary_image, source.alternate_image};
    }

    std::array<ObjectImageStyle, sprites::object_sprite_styles.size()> styles{};
    for (std::size_t index = 0; index < styles.size(); ++index) {
        const auto& source = sprites::object_sprite_styles[index];
        styles[index] = {source.individual_color, source.multicolor_1,
                         source.multicolor_2, source.expand_horizontal};
    }

    auto terrain_colors = data::river_color_roles;
    if (bridge_flash) terrain_colors[1] = 2;
    const OptionsSceneData options{
        data::hud_tiles,
        data::hud_cells,
        data::hud_option_glyphs,
        data::hud_option_cell,
        terrain_colors,
        data::presentation_palette,
        {data::hud_fuel_marker, 4, 12, data::hud_fuel_marker_x,
         data::hud_fuel_marker_y},
    };
    const WorldSceneData scene{options, images, image_pairs, styles, 12};
    return render_world_scene(scene, rows, placements, selected_option);
}

Frame make_demo_scene(const DemoPresentationStep& step, std::size_t selected_option) {
    std::vector<WorldObjectPlacement> placements;
    for (const auto& band : schedule_demo_presentation(step)) {
        placements.insert(placements.end(), band.placements.begin(), band.placements.end());
    }
    return make_world_scene(std::span(step.terrain_before_scroll).first(157), placements, selected_option);
}

} // namespace river_raid
