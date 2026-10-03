#include "game/shot_contact_resolution.hpp"
#include "app/generator_setup.hpp"

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
        seed.active_begin = 0;
        seed.records[2] = {80, 86, 0x48, 9, 8, 30, 130, 0};
        seed.records[3] = {100, 86, 0, 14, 14, 30, 130, 0};
        RiverWorld world(make_river_generator(), WorldObjectHistory(seed, {delay, choices, clearance}),
                         generator_edge_patterns());
        std::array<ShotContactSlot, kShotContactSlotCount> contacts{};
        auto shot = update_player_shot({}, true);
        const auto start_event = shot.start_event;
        auto result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.selection.outcome == ShotContactOutcome::None &&
                shot.pose == PlayerShotPose::Visible && world.object_state() == seed, "No-contact mutation");
        contacts[4] = {false, true, false, true, {0}, {2}};
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.object_effect && result.object_effect->score_kind == 9 &&
                world.object_state().records[2].animated_kind == 4 &&
                world.object_state().records[2].base_kind == 8 &&
                world.object_state().records[2].animation_count == 6, "Ordinary effect handoff differs");
        require(shot.pose == PlayerShotPose::Hidden && shot.state.vertical_position == 0 &&
                shot.vertical_position_write == 0 && shot.start_event == start_event,
                "Hit failed to clear shot or erased prior launch event");
        world.reset();
        shot = update_player_shot({}, true);
        contacts[0].terrain_contact = true;
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.selection.outcome == ShotContactOutcome::Terrain && !result.object_effect &&
                world.object_state() == seed && shot.pose == PlayerShotPose::Hidden,
                "Terrain did not override ordinary hit");
        contacts[0] = {};
        contacts[5] = {false, true, true, false, {3}, {0}};
        shot = update_player_shot({}, true);
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.selection.outcome == ShotContactOutcome::Bridge && result.object_effect &&
                world.object_state().records[3].animated_kind == 2 &&
                shot.pose == PlayerShotPose::Hidden && result.object_effect->score_kind == 14,
                "Bridge hit failed to explode, score, or clear shot");
        world.reset();
        shot = update_player_shot({}, true);
        contacts[5].bridge_sprite_present = true;
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.bridge_sprite_contact && result.object_effect->score_kind == 16 &&
                world.object_state().records[3].animated_kind == 2,
                "Joint bridge sprite contact did not request the special score");
        world.reset();
        result = resolve_shot_contacts(world, shot, contacts, true);
        require(result.bridge_sprite_contact && result.object_effect->score_kind == 0,
                "Joint bridge contact awarded points during a pending round event");
        world.reset();
        contacts[5] = {false, true, false, true, {0}, {3}, true};
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.bridge_sprite_contact && result.object_effect->score_kind == 16 &&
                world.object_state().records[3].animated_kind == 4,
                "Secondary bridge role lost joint-contact scoring or explosion role");
        contacts[5] = {};
        result = resolve_shot_contacts(world, shot, contacts, true);
        require(result.object_effect && !result.object_effect->score_kind &&
                shot.pose == PlayerShotPose::Hidden, "Pending event blocked destruction instead of score request");
        world.reset();
        contacts = {};
        contacts[5] = {false, true, false, false, {3}, {3}, true};
        result = resolve_shot_contacts(world, shot, contacts, false);
        require(result.selection.outcome == ShotContactOutcome::None &&
                !result.bridge_sprite_contact && world.object_state() == seed,
                "Separate sprite contact without an object role destroyed a bridge");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
