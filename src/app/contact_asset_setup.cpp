#include "app/contact_asset_setup.hpp"
#include "app/player_asset_selection.hpp"

#include "object_sprites.hpp"
#include "player_data.hpp"
#include "player_detail.hpp"
#include "shot_data.hpp"

#include <stdexcept>
#include <vector>

namespace river_raid {

OccupancyMask make_world_object_contact_mask(const WorldObjectPlacement& placement) {
    namespace data = assets::object_sprites;
    if (placement.image_key >= data::object_sprites.size() ||
        placement.style_key >= data::object_sprite_styles.size() ||
        (placement.image_slot != WorldObjectImageSlot::primary &&
         placement.image_slot != WorldObjectImageSlot::alternate)) {
        throw std::invalid_argument("Invalid contact object asset selection");
    }
    const auto pair = data::object_sprites[placement.image_key];
    const auto index = placement.image_slot == WorldObjectImageSlot::primary
        ? pair.primary_image : pair.alternate_image;
    const auto& image = data::sprite_images.at(index);
    return make_sprite_occupancy_mask(image.width, image.height, image.roles,
        SpriteMode::Multicolor, data::object_sprite_styles[placement.style_key].expand_horizontal);
}

OccupancyMask make_player_contact_mask(FlightPose pose, LifeCycleImage image) {
    if (image == LifeCycleImage::Hidden) return {1, 1, {0}};
    if (image != LifeCycleImage::Straight) {
        const auto& sprite = assets::object_sprites::sprite_images[
            player_explosion_index(image)];
        return make_sprite_occupancy_mask(sprite.width, sprite.height, sprite.roles,
                                          SpriteMode::Multicolor);
    }
    namespace data = assets::player;
    return make_indexed_occupancy_mask(data::sprite_width, data::sprite_height,
                                      data::player_pixels[player_pose_index(pose)]);
}

OccupancyMask make_projectile_contact_mask() {
    namespace data = assets::shot;
    return make_indexed_occupancy_mask(data::width, data::height, data::pixels);
}

OccupancyMask make_player_detail_contact_mask() {
    namespace data = assets::player_detail;
    return make_indexed_occupancy_mask(data::width, data::height, data::pixels);
}

OccupancyMask make_river_contact_mask(std::span<const PackedRiverRow> rows) {
    if (rows.empty() || rows.size() > 200) throw std::invalid_argument("Invalid contact terrain rows");
    std::vector<std::uint8_t> packed;
    packed.reserve(rows.size() * PackedRiverRow{}.size());
    for (const auto& row : rows) packed.insert(packed.end(), row.begin(), row.end());
    return make_terrain_occupancy_mask(packed, static_cast<int>(PackedRiverRow{}.size()));
}

} // namespace river_raid
