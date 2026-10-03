#pragma once

#include "render/object_image.hpp"
#include "render/options_scene.hpp"
#include "render/sprite_scene.hpp"
#include "render/world_object_schedule.hpp"

namespace river_raid {

struct WorldRoleImage {
    int width{}, height{};
    std::span<const std::uint8_t> roles;
};

struct WorldImagePair {
    std::size_t primary{}, alternate{};
};

struct WorldSceneData {
    OptionsSceneData options;
    std::span<const WorldRoleImage> images;
    std::span<const WorldImagePair> image_pairs;
    std::span<const ObjectImageStyle> styles;
    std::uint8_t hud_background_color{};
};

// PAL options composition; inputs describe the visible presentation, not one
// atomic simulation state. Later placements cover earlier placements.
Frame render_world_scene(const WorldSceneData& data,
                         std::span<const PackedRiverRow> rows,
                         std::span<const WorldObjectPlacement> placements,
                         std::size_t selected_option = 0);

} // namespace river_raid
