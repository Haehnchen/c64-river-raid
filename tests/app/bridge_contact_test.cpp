#include "app/flight_preview_contacts.hpp"
#include "app/contact_scene.hpp"
#include "app/world_setup.hpp"
#include "game/player_shot.hpp"
#include "game/player_steering.hpp"
#include "game/shot_contact_resolution.hpp"
#include "player_data.hpp"
#include "render/world_object_schedule.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

river_raid::PlayerSteeringState generated_player_at_contact() {
    using namespace river_raid;
    PlayerSteeringState player{assets::player::initial_horizontal_position,
                               assets::player::initial_horizontal_fraction,
                               assets::player::initial_horizontal_step,
                               assets::player::initial_scroll_speed};
    for (int tick = 0; tick < 18; ++tick) {
        player = steer_player(player, {false, false, true, false, false}).state;
    }
    require(player.horizontal_position == 75, "Generated steering did not reach contact position");
    return player;
}

river_raid::PlayerShotResult generated_shot_at_contact() {
    using namespace river_raid;
    auto shot = update_player_shot({}, true);
    for (int tick = 0; tick < 20; ++tick) {
        shot = update_player_shot(shot.state, false);
    }
    require(shot.state.vertical_position == 0x70 && shot.pose == PlayerShotPose::Visible,
            "Generated shot did not reach contact position");
    return shot;
}

std::array<river_raid::ShotContactSlot, river_raid::kShotContactSlotCount>
sample_with_bridge_state(const river_raid::RiverWorld& world,
                         const river_raid::BridgeSpriteState& bridge,
                         const river_raid::PlayerSteeringState& player,
                         const river_raid::PlayerShotResult& shot) {
    using namespace river_raid;
    const auto& auxiliary = world.auxiliary_projectile_state();
    return detail::sample_contact_scene({
        world.object_state(), world.river_state().phase, &world.viewport(), &bridge,
        &auxiliary, player, FlightPose::Straight, LifeCycleImage::Hidden, &shot}).shot;
}

void test_generated_joint_bridge_contact() {
    using namespace river_raid;

    auto world = make_river_world();
    // First stable overlap under the component cadence: one bridge tick, then one row.
    std::uint8_t animation_phase = 0;
    for (int row = 0; row < 1105; ++row) {
        world.advance_motion({animation_phase, world.river_state().section >= 2, std::nullopt});
        world.advance_bridge_sprites(animation_phase, true);
        ++animation_phase;
        (void)world.advance();
    }

    require(world.generated_rows() == 1105 && animation_phase == 81,
            "Generated row or animation-phase boundary changed");
    const auto bridge = world.bridge_sprite_state().crossing;
    require(bridge && bridge->image == BridgeSpriteImage::CrossingLeftB &&
                bridge->vic_x == 204 && bridge->vic_y == 114,
            "Generated mechanical bridge crossing differs at the frozen contact row");
    const auto before_next_crossing_update = world.bridge_presentation_state().crossing;
    require(before_next_crossing_update && before_next_crossing_update->vic_y == 113,
            "Generated-row mutation moved the published bridge before its next update");

    // Reconstruct the same typed contact scene with the current mechanics as a
    // counterfactual. Search only the nearby ordinary projectile window for a
    // concrete mask difference at the one-row publication boundary.
    const auto published_before_update = world.bridge_presentation_state();
    const auto mechanical_before_update = world.bridge_sprite_state();
    bool found_publication_sensitive_contact = false;
    for (int horizontal = 88; horizontal <= 112 && !found_publication_sensitive_contact;
         ++horizontal) {
        for (int vertical = 104; vertical <= 128; ++vertical) {
            auto player = generated_player_at_contact();
            player.horizontal_position = static_cast<std::uint8_t>(horizontal);
            PlayerShotResult shot{{static_cast<std::uint8_t>(vertical)},
                                  PlayerShotPose::Visible, std::nullopt, std::nullopt};
            const auto from_world = sample_flight_preview_shot_contacts(world, player, shot);
            const auto published_scene = sample_with_bridge_state(
                world, published_before_update, player, shot);
            const auto mechanical_scene = sample_with_bridge_state(
                world, mechanical_before_update, player, shot);
            require(from_world == published_scene,
                    "World shot adapter did not consume the published bridge snapshot");
            found_publication_sensitive_contact = from_world != mechanical_scene;
            if (found_publication_sensitive_contact) break;
        }
    }
    require(found_publication_sensitive_contact,
            "Generated bridge fixture had no contact-mask distinction at publication boundary");

    world.advance_bridge_crossing(animation_phase, true);
    const auto published_bridge = world.bridge_presentation_state().crossing;
    require(published_bridge && published_bridge->image == bridge->image &&
                published_bridge->vic_x == bridge->vic_x &&
                published_bridge->vic_y == bridge->vic_y,
            "Next normal crossing update did not publish the current geometry");

    const auto& records = world.object_state().records;
    require(records[5].animated_kind == 14 && records[5].base_kind == 14,
            "Generated bridge-contact object is not kind 14");
    const auto bands = schedule_world_object_snapshot(world.river_state().phase,
                                                       world.object_state());
    bool found_object = false;
    for (const auto& band : bands) {
        for (const auto& placement : band.placements) {
            if (placement.record_index != 5) continue;
            found_object = placement.image_key == 14 && placement.style_key == 14 &&
                           placement.left == 134 && placement.top == 64;
        }
    }
    require(found_object, "Generated kind-14 placement moved at the frozen contact row");

    const auto player = generated_player_at_contact();
    auto shot = generated_shot_at_contact();
    const auto contacts = sample_flight_preview_shot_contacts(world, player, shot);
    for (const auto& contact : contacts) {
        require(!contact.terrain_contact, "Terrain incorrectly wins the generated joint sample");
    }
    const auto& joint = contacts[3];
    require(joint.object_contact && joint.primary_present &&
                joint.primary_object.value == 5 && joint.bridge_sprite_present,
            "Published bridge placement did not reach the generated contact adapter");

    const auto resolution = resolve_shot_contacts(world, shot, contacts, false);
    require(resolution.selection.outcome == ShotContactOutcome::Bridge &&
                resolution.selection.slot && resolution.selection.slot->value == 3 &&
                resolution.selection.object && resolution.selection.object->index.value == 5 &&
                resolution.selection.object->role == ShotObjectRole::Primary,
            "Generated joint sample selected the wrong contact");
    require(resolution.bridge_sprite_contact && resolution.object_effect &&
                resolution.object_effect->record_index == 5 &&
                resolution.object_effect->destroyed_kind == 14 &&
                resolution.object_effect->score_kind == std::optional<std::uint8_t>{16} &&
                resolution.object_effect->sound_countdown == 0x1f &&
                world.object_state().records[5].animated_kind == 2 &&
                shot.pose == PlayerShotPose::Hidden && shot.state.vertical_position == 0,
            "Generated joint resolution lost special score/effect or projectile clear");

    // This is the same lifecycle boundary used by the production flight path.
    const auto displayed_before_hit = world.bridge_presentation_state().crossing;
    world.mark_bridge_destroyed();
    world.apply_bridge_sprite_destruction({resolution.bridge_sprite_contact, 2});
    const auto mechanical_after = world.bridge_sprite_state().crossing;
    require(mechanical_after && mechanical_after->image == BridgeSpriteImage::JointExplosionA &&
                world.bridge_presentation_state().crossing == displayed_before_hit,
            "Generated joint effect changed the published image before crossing update");
    ++animation_phase;
    world.advance_bridge_crossing(animation_phase, true);
    const auto published_after = world.bridge_presentation_state().crossing;
    require(published_after && published_after->image == BridgeSpriteImage::JointExplosionA,
            "Generated joint image was not published by the next crossing update");
}

} // namespace

int main() {
    try {
        test_generated_joint_bridge_contact();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
