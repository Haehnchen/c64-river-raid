#include "app/player_overlay.hpp"
#include "app/player_asset_selection.hpp"

#include "player_data.hpp"
#include "player_detail.hpp"
#include "object_sprites.hpp"
#include "presentation_palette.hpp"
#include "render/object_image.hpp"
#include "render/sprite_scene.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace river_raid {
namespace {

assets::player::SpritePixels colorize_sprite(const assets::player::SpriteRoles& roles,
                                             std::uint8_t individual_color) {
    namespace data = assets::player;
    assets::player::SpritePixels pixels{};
    for (std::size_t index = 0; index < roles.size(); ++index) {
        switch (roles[index]) {
        case 0: pixels[index] = data::transparent_pixel; break;
        case 1: pixels[index] = data::multicolor_1; break;
        case 2: pixels[index] = individual_color; break;
        case 3: pixels[index] = data::multicolor_2; break;
        default: throw std::invalid_argument("Invalid player sprite color role");
        }
    }
    return pixels;
}

assets::player::SpritePixels colorize_detail(std::uint8_t individual_color) noexcept {
    namespace data = assets::player;
    assets::player::SpritePixels pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const auto source = assets::player_detail::pixels[index];
        pixels[index] = source == data::individual_color ? individual_color : source;
    }
    return pixels;
}

} // namespace

void draw_player_preview(Frame& destination, const PlayerSteeringState& state,
                         FlightPose pose, LifeCycleImage image,
                         std::uint8_t individual_color) {
    namespace data = assets::player;
    if (image == LifeCycleImage::Hidden) return;
    if (image != LifeCycleImage::Straight) {
        const auto& source = assets::object_sprites::sprite_images[
            player_explosion_index(image)];
        const auto explosion = colorize_object_image(source.width, source.height,
                                                     source.roles,
                                                     {individual_color, data::multicolor_1,
                                                      data::multicolor_2, false});
        const std::array layers{ImageLayer{explosion.pixels, explosion.width, explosion.height,
            static_cast<int>(state.horizontal_position) * 2 - 24, data::fixed_y}};
        draw_sprite_layers(destination, layers, assets::presentation_palette,
                           {0, 0, destination.width, destination.height});
        return;
    }
    const auto pixels = colorize_sprite(data::sprite_roles[player_pose_index(pose)], individual_color);
    const auto detail = colorize_detail(individual_color);
    const std::array layers{ImageLayer{std::span<const std::uint8_t>(pixels),
                                       static_cast<int>(data::sprite_width),
                                       static_cast<int>(data::sprite_height),
                                       static_cast<int>(state.horizontal_position) * 2 - 24,
                                       static_cast<int>(data::fixed_y)},
                            ImageLayer{std::span<const std::uint8_t>(detail),
                                       assets::player_detail::width, assets::player_detail::height,
                                       static_cast<int>(state.horizontal_position) * 2 - 24,
                                       static_cast<int>(data::fixed_y)}};
    draw_sprite_layers(destination, layers, assets::presentation_palette,
                       {0, 0, destination.width, destination.height});
}

} // namespace river_raid
