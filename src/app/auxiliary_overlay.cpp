#include "app/auxiliary_overlay.hpp"

#include "auxiliary_sprite.hpp"
#include "presentation_palette.hpp"
#include "render/object_image.hpp"
#include "render/sprite_scene.hpp"

#include <array>

namespace river_raid {
namespace {
namespace data = assets::auxiliary_sprite;
}

OccupancyMask make_auxiliary_projectile_contact_mask() {
    return make_sprite_occupancy_mask(data::sprite_width, data::sprite_height,
                                      data::sprite_roles, SpriteMode::Multicolor);
}

void draw_auxiliary_projectile(Frame& frame, const AuxiliaryProjectileState& state) {
    if (!state.presentation) return;
    const auto& sprite = *state.presentation;
    const auto style = data::sprite_style;
    const auto image = colorize_object_image(data::sprite_width, data::sprite_height,
        data::sprite_roles, {style.individual_color, style.multicolor_1,
                            style.multicolor_2, style.expand_horizontal});
    const std::array layers{ImageLayer{image.pixels, image.width, image.height,
        sprite.vic_x - 24, sprite.vic_y - 50}};
    draw_sprite_layers(frame, layers, assets::presentation_palette, {0, 4, 320, 157});
}

} // namespace river_raid
