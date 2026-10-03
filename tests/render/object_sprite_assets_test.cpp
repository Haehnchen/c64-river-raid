#include "object_sprites.hpp"
#include "render/sprite_scene.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        namespace data = river_raid::assets::object_sprites;
        if (data::object_sprites.size() != 32 || data::sprite_images.size() != 32) {
            throw std::runtime_error("Incomplete sprite mapping");
        }
        river_raid::Palette test_palette{};
        for (std::size_t i = 0; i < test_palette.size(); ++i) test_palette[i] = 0xff000000U | i;
        for (const auto& image : data::sprite_images) {
            river_raid::Frame frame{24, 21, std::vector<std::uint32_t>(24 * 21, 0x12345678U)};
            if (image.width != 24 || image.height != 21 || image.roles.size() != 504) {
                throw std::runtime_error("Invalid sprite dimensions");
            }
            std::array<std::uint8_t, 504> pixels{};
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                const auto role = image.roles[i];
                if (role > 3 || role != image.roles[i ^ 1U]) {
                    throw std::runtime_error("Invalid paired sprite role");
                }
                pixels[i] = role == 0 ? 255 : role;
            }
            const std::array layers{river_raid::ImageLayer{pixels, 24, 21, 0, 0}};
            river_raid::draw_sprite_layers(frame, layers, test_palette, {0, 0, 24, 21});
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                const auto expected = pixels[i] == 255 ? 0x12345678U : test_palette[pixels[i]];
                if (frame.pixels[i] != expected) throw std::runtime_error("Sprite index transfer failed");
            }
        }
        for (const auto& mapping : data::object_sprites) {
            for (const auto image : {mapping.primary_image, mapping.alternate_image}) {
                if (image >= data::sprite_images.size()) {
                    throw std::runtime_error("Sprite mapping out of range");
                }
            }
        }
        for (const auto role : data::sprite_images[data::blank_image].roles) {
            if (role != 0) throw std::runtime_error("Blank sprite is not transparent");
        }
        for (const auto& style : data::object_sprite_styles) {
            if (style.individual_color > 15 || style.multicolor_1 > 15 || style.multicolor_2 > 15) {
                throw std::runtime_error("Invalid sprite style color");
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
