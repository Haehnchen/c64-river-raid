#include "render/contact_masks.hpp"
#include "render/contact_producer.hpp"
#include "fixtures/contact_pixel_cases.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    using namespace river_raid;
    try {
        std::array<std::array<bool, 4>, 4> covered{};
        for (const auto& sample : test_data::contact_pixel_cases) {
            std::array<std::uint8_t, 24 * 21> a_roles, b_roles;
            a_roles.fill(sample.sprite_a_role);
            b_roles.fill(sample.sprite_b_role);
            const auto a = make_sprite_occupancy_mask(24, 21, a_roles, SpriteMode::Multicolor,
                                                     sample.horizontal_expansion);
            const auto b = make_sprite_occupancy_mask(24, 21, b_roles, SpriteMode::Multicolor,
                                                     sample.horizontal_expansion);
            // The measured scene has a two-character foreground strip.
            std::vector<std::uint8_t> packed(40 * 200, 0);
            for (int y = 0; y < 200; ++y) {
                for (int column : test_data::contact_terrain_screen_columns) {
                    packed[y * 40 + column] = static_cast<std::uint8_t>(sample.terrain_role * 0x55);
                }
            }
            const auto terrain = make_terrain_occupancy_mask(packed, 40);
            const std::array sprites{
                ContactSpritePlacement{{&a, sample.sprite_a_x, sample.sprite_y}, 1},
                ContactSpritePlacement{{&b, sample.sprite_b_x, sample.sprite_y}, 2}};
            PixelContactAccumulator producer;
            producer.scan(sprites, {&terrain, 24, 50}, {0, 0, 512, 312});
            const auto result = producer.pending_sample();
            if (result.object_participants != sample.sprite_sprite_mask ||
                result.terrain_participants != sample.sprite_background_mask) {
                throw std::runtime_error("Injected VIC contact-pixel measurement differs");
            }
            covered.at(sample.sprite_a_role).at(sample.terrain_role) = true;
        }
        for (const auto& role : covered) {
            for (bool found : role) if (!found) throw std::runtime_error("Missing contact role combination");
        }
        if (test_data::contact_pixel_cases.size() != 16) throw std::runtime_error("Unexpected hardware matrix size");
        std::cout << "16 injected VIC contact-pixel measurements match\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
