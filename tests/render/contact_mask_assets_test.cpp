#include "render/contact_masks.hpp"
#include "render/object_image.hpp"
#include "object_sprites.hpp"
#include "player_data.hpp"
#include "shot_data.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace river_raid;
    try {
        namespace objects = assets::object_sprites;
        std::size_t shared_color_pixels = 0;
        for (const auto& image : objects::sprite_images) {
            shared_color_pixels += std::count(image.roles.begin(), image.roles.end(), 1);
            for (const auto& style : objects::object_sprite_styles) {
                const auto mask = make_sprite_occupancy_mask(image.width, image.height, image.roles,
                    SpriteMode::Multicolor, style.expand_horizontal);
                const auto indexed = colorize_object_image(image.width, image.height, image.roles,
                    {style.individual_color, style.multicolor_1, style.multicolor_2, style.expand_horizontal});
                const auto visible = make_indexed_occupancy_mask(indexed.width, indexed.height, indexed.pixels);
                if (mask.width != visible.width || mask.height != visible.height || mask.pixels != visible.pixels) {
                    throw std::runtime_error("Object occupancy differs from nontransparent asset roles");
                }
            }
        }
        if (shared_color_pixels == 0) throw std::runtime_error("Missing sprite role-1 coverage");
        namespace player = assets::player;
        for (std::size_t pose = 0; pose < player::player_pixels.size(); ++pose) {
            const auto mask = make_indexed_occupancy_mask(player::sprite_width, player::sprite_height,
                                                         player::player_pixels[pose]);
            for (std::size_t byte = 0; byte < player::sprite_bytes[pose].size(); ++byte) {
                const auto packed = player::sprite_bytes[pose][byte];
                for (std::size_t pair = 0; pair < 4; ++pair) {
                    const bool occupied = ((packed >> (6 - 2 * pair)) & 3) != 0;
                    if (mask.pixels[byte * 8 + pair * 2] != occupied ||
                        mask.pixels[byte * 8 + pair * 2 + 1] != occupied) {
                        throw std::runtime_error("Player mask differs from decoded nonzero sprite pairs");
                    }
                }
            }
        }
        const auto shot = make_indexed_occupancy_mask(assets::shot::width, assets::shot::height,
                                                     assets::shot::pixels);
        if (std::count(shot.pixels.begin(), shot.pixels.end(), 1) != 10) {
            throw std::runtime_error("Projectile mask differs from exported 2x5 shape");
        }
        for (unsigned packed = 0; packed < 256; ++packed) {
            const std::array<std::uint8_t, 1> row{static_cast<std::uint8_t>(packed)};
            const auto terrain = make_terrain_occupancy_mask(row, 1);
            for (unsigned pair = 0; pair < 4; ++pair) {
                const bool foreground = (packed & (0x80U >> (pair * 2))) != 0;
                if (terrain.pixels[pair * 2] != foreground || terrain.pixels[pair * 2 + 1] != foreground) {
                    throw std::runtime_error("Terrain foreground differs from pair high bit");
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
