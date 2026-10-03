#include "app/contact_asset_setup.hpp"
#include "app/generator_setup.hpp"
#include "game/contact_sampling.hpp"
#include "game/shot_contact_resolution.hpp"
#include "render/contact_producer.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    using namespace river_raid;
    try {
        constexpr std::array<std::uint8_t, 16> delay{}, clearance{};
        constexpr std::array<std::uint8_t, 8> choices{};
        WorldObjectState seed{};
        seed.records[2] = {100, 70, 0, 9, 9, 30, 130, 0};
        RiverWorld world(make_river_generator(), WorldObjectHistory(seed, {delay, choices, clearance}),
                         generator_edge_patterns());
        const WorldObjectPlacement target{2, WorldObjectImageSlot::primary, 116, 50, 9, 9};
        const auto object = make_world_object_contact_mask(target);
        const auto projectile = make_projectile_contact_mask();
        const auto object_pixel = std::find(object.pixels.begin(), object.pixels.end(), 1);
        const auto shot_pixel = std::find(projectile.pixels.begin(), projectile.pixels.end(), 1);
        require(object_pixel != object.pixels.end() && shot_pixel != projectile.pixels.end(), "Empty hit assets");
        const auto object_index = static_cast<int>(object_pixel - object.pixels.begin());
        const auto shot_index = static_cast<int>(shot_pixel - projectile.pixels.begin());
        const int shot_left = target.left + object_index % object.width - shot_index % projectile.width;
        const int shot_top = target.top + object_index / object.width - shot_index / projectile.width;
        std::array<PackedRiverRow, 157> rows;
        for (auto& row : rows) row.fill(0x55);
        const auto water = make_river_contact_mask(rows);
        const std::array sprites{
            ContactSpritePlacement{{&object, target.left, target.top}, kPrimaryObjectContactParticipant},
            ContactSpritePlacement{{&projectile, shot_left, shot_top}, kProjectileContactParticipant}};
        PixelContactAccumulator producer;
        producer.scan(sprites, {&water, 0, 4}, {0, 4, 320, 157});
        const auto sampled = producer.pending_sample();
        require(sampled.object_participants == (kPrimaryObjectContactParticipant | kProjectileContactParticipant) &&
                sampled.terrain_participants == 0, "Asset pixel contact was not produced");
        ContactSamplingBuffer buffer;
        buffer.replace_object_mapping({3}, ShotObjectRole::Primary, ShotObjectIndex{2});
        producer.sample_into(buffer, {3});
        require(producer.pending_sample() == ContactSectionSample{}, "Consumed contacts remained pending");
        producer.publish_into(buffer);
        auto shot = update_player_shot({}, true);
        const auto resolved = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        require(resolved.object_effect && resolved.object_effect->record_index == 2 &&
                resolved.object_effect->score_kind == 9 &&
                world.object_state().records[2].animated_kind == 2 &&
                world.object_state().records[2].base_kind == 9 &&
                shot.pose == PlayerShotPose::Hidden, "Produced contact failed to reach ordinary object effect");

        // The same sprite placement over foreground must prefer terrain.
        for (auto& row : rows) row.fill(0xAA);
        const auto land = make_river_contact_mask(rows);
        producer.scan(sprites, {&land, 0, 4}, {0, 4, 320, 157});
        buffer.replace_object_mapping({3}, ShotObjectRole::Primary, ShotObjectIndex{2});
        producer.sample_into(buffer, {3});
        producer.publish_into(buffer);
        world.reset();
        shot = update_player_shot({}, true);
        const auto terrain_hit = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        require(terrain_hit.selection.outcome == ShotContactOutcome::Terrain &&
                world.object_state() == seed, "Produced terrain did not override object contact");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
