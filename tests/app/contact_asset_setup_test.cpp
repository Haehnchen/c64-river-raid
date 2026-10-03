#include "app/contact_asset_setup.hpp"
#include "render/object_image.hpp"
#include "render/contact_producer.hpp"
#include "object_sprites.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok) {
    if (!ok) throw std::runtime_error("Contact asset handoff differs");
}
template<class Action> void invalid(Action action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid contact asset accepted");
}
}

int main() {
    using namespace river_raid;
    namespace objects = assets::object_sprites;
    try {
        for (std::uint8_t key = 0; key < objects::object_sprites.size(); ++key) {
            for (std::uint8_t style = 0; style < objects::object_sprite_styles.size(); ++style) {
                for (auto slot : {WorldObjectImageSlot::primary, WorldObjectImageSlot::alternate}) {
                    const WorldObjectPlacement placement{0, slot, 0, 0, key, style};
                    const auto mask = make_world_object_contact_mask(placement);
                    const auto pair = objects::object_sprites[key];
                    const auto& image = objects::sprite_images[slot == WorldObjectImageSlot::primary
                        ? pair.primary_image : pair.alternate_image];
                    const auto& palette = objects::object_sprite_styles[style];
                    const auto indexed = colorize_object_image(image.width, image.height, image.roles,
                        {palette.individual_color, palette.multicolor_1, palette.multicolor_2, palette.expand_horizontal});
                    require(mask.width == indexed.width && mask.height == indexed.height);
                    for (std::size_t i = 0; i < mask.pixels.size(); ++i) {
                        require((mask.pixels[i] != 0) == (indexed.pixels[i] != 255));
                    }
                }
            }
        }
        for (auto pose : {FlightPose::Straight, FlightPose::Right, FlightPose::Left}) {
            const auto mask = make_player_contact_mask(pose);
            const auto detail = make_player_detail_contact_mask();
            require(mask.width == 24 && mask.height == 21 &&
                    std::count(mask.pixels.begin(), mask.pixels.end(), 1) > 0);
            require(detail.width == 24 && detail.height == 21 &&
                    std::count(detail.pixels.begin(), detail.pixels.end(), 1) == 14);
            for (int y = 0; y < 21; ++y) {
                for (int x = 0; x < 24; ++x) {
                    require(detail.occupied_at(x, y) == (x >= 12 && x <= 13 && y >= 8 && y <= 14));
                    require(!detail.occupied_at(x, y) || !mask.occupied_at(x, y));
                }
            }
        }
        const auto shot = make_projectile_contact_mask();
        const auto plane = make_player_contact_mask(FlightPose::Straight);
        const auto detail = make_player_detail_contact_mask();
        const OccupancyMask point{1, 1, {1}};
        const std::array aircraft{ContactSpritePlacement{{&plane, 0, 0}, 1},
                                  ContactSpritePlacement{{&detail, 0, 0}, 4}};
        PixelContactAccumulator contacts;
        contacts.scan(aircraft, {&point, 12, 8}, {0, 0, 24, 21});
        require(contacts.consume_terrain_participants() == 4);
        require(contacts.consume_object_participants() == 0);
        require(std::count(shot.pixels.begin(), shot.pixels.end(), 1) == 10);
        std::array<PackedRiverRow, 2> rows;
        rows[0].fill(0x55);
        rows[1].fill(0xAA);
        const auto river = make_river_contact_mask(rows);
        require(river.width == 320 && river.height == 2);
        for (int x = 0; x < 320; ++x) require(!river.occupied_at(x, 0) && river.occupied_at(x, 1));
        invalid([] { (void)make_world_object_contact_mask({0, WorldObjectImageSlot::primary, 0, 0, 32, 0}); });
        invalid([] { (void)make_world_object_contact_mask({0, WorldObjectImageSlot::primary, 0, 0, 0, 16}); });
        invalid([] { (void)make_world_object_contact_mask({0, static_cast<WorldObjectImageSlot>(99), 0, 0, 0, 0}); });
        invalid([] { (void)make_player_contact_mask(static_cast<FlightPose>(99)); });
        invalid([] { (void)make_river_contact_mask({}); });
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
