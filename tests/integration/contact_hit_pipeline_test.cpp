#include "game/contact_sampling.hpp"
#include "game/shot_contact_resolution.hpp"
#include "app/generator_setup.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    using namespace river_raid;
    try {
        constexpr std::array<std::uint8_t, 16> delay{}, clearance{};
        constexpr std::array<std::uint8_t, 8> choices{};
        WorldObjectState seed{};
        seed.records[2] = {80, 86, 0, 9, 8, 30, 130, 0};
        seed.records[3] = {100, 86, 0, 14, 14, 30, 130, 0};
        RiverWorld world(make_river_generator(), WorldObjectHistory(seed, {delay, choices, clearance}),
                         generator_edge_patterns());
        ContactSamplingBuffer buffer;
        buffer.replace_object_mapping({3}, ShotObjectRole::Primary, ShotObjectIndex{2});
        buffer.replace_sample({3}, {kProjectileContactParticipant | kPrimaryObjectContactParticipant, 0});
        auto shot = update_player_shot({}, true);
        auto resolution = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        if (resolution.selection.outcome != ShotContactOutcome::None || world.object_state() != seed) {
            throw std::runtime_error("Working contacts leaked before handoff");
        }
        buffer.rollover_for_foreground();
        buffer.replace_object_mapping({3}, ShotObjectRole::Primary, ShotObjectIndex{3});
        buffer.replace_sample({3}, {0, kProjectileContactParticipant});
        resolution = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        if (!resolution.object_effect || resolution.object_effect->record_index != 2 ||
            resolution.object_effect->score_kind != 9 ||
            world.object_state().records[2].animated_kind != 2 ||
            world.object_state().records[3] != seed.records[3] ||
            shot.pose != PlayerShotPose::Hidden) {
            throw std::runtime_error("New-cycle sample or mapping corrupted the published hit");
        }
        buffer.rollover_for_foreground();
        shot = update_player_shot({}, true);
        resolution = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        if (resolution.selection.outcome != ShotContactOutcome::Terrain) {
            throw std::runtime_error("Next-cycle terrain contact was not published");
        }
        buffer = {};
        world.reset();
        shot = update_player_shot({}, true);
        resolution = resolve_shot_contacts(world, shot,
            make_shot_contact_slots(buffer.foreground_snapshot()), false);
        if (resolution.selection.outcome != ShotContactOutcome::None || world.object_state() != seed) {
            throw std::runtime_error("Reset retained contacts");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
