#include "app/bridge_overlay.hpp"

#include "bridge_sprites.hpp"
#include "presentation_palette.hpp"
#include "render/object_image.hpp"
#include "render/sprite_scene.hpp"

#include <array>

namespace river_raid {
namespace {
namespace data = assets::bridge_sprites;

void draw_sprite(Frame& destination, const BridgeSpritePresentation& sprite) {
    const auto index = static_cast<std::size_t>(sprite.image);
    const auto& source = data::images.at(index);
    const auto style = data::image_styles.at(index);
    const auto image = colorize_object_image(data::sprite_width, data::sprite_height,
        source.roles, {sprite.color, style.multicolor_1, style.multicolor_2, false});
    const std::array layers{ImageLayer{image.pixels, image.width, image.height,
        sprite.vic_x - kBridgeViewportOriginX, sprite.vic_y - kBridgeViewportOriginY}};
    draw_sprite_layers(destination, layers, assets::presentation_palette, {0, 4, 320, 157});
}
} // namespace

OccupancyMask make_bridge_sprite_contact_mask(BridgeSpriteImage image) {
    const auto& source = data::images.at(static_cast<std::size_t>(image));
    return make_sprite_occupancy_mask(data::sprite_width, data::sprite_height,
                                     source.roles, SpriteMode::Multicolor);
}

void draw_bridge_sprites(Frame& destination, const BridgeSpriteState& state) {
    if (state.crossing) draw_sprite(destination, *state.crossing);
    if (state.gap) draw_sprite(destination, *state.gap);
}

} // namespace river_raid
