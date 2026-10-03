#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "app/flight_preview_contacts.hpp"
#include "app/generator_setup.hpp"
#include "app/world_scene_setup.hpp"
#include "game/shot_contact_resolution.hpp"
#include "render/world_object_schedule.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace river_raid;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool any_contact(const std::array<ShotContactSlot, kShotContactSlotCount>& contacts) {
    for (const auto& contact : contacts) {
        if (contact.terrain_contact || contact.object_contact ||
            contact.primary_present || contact.secondary_present) {
            return true;
        }
    }
    return false;
}

WorldObjectState contact_seed() {
    WorldObjectState state{};
    state.active_begin = 2;
    state.generator_cursor = kWorldObjectCapacity;
    state.special_cursor = kWorldObjectCapacity;
    state.records[2] = {100, 86, 0, 9, 9, 30, 130, 0};
    state.records[3] = {100, 100, 0, 10, 10, 30, 130, 0};
    return state;
}

struct HeldFireRun {
    std::size_t ordinary_hits{};
    WorldObjectState final_world{};
    bool saw_explosion{};
    bool saw_animation_step{};
    bool saw_animation_clear{};
    std::vector<std::uint8_t> animation_counts;
};

struct HitAnimationTrack {
    bool active{};
    std::uint8_t base_kind{};
    std::uint8_t last_count{};
};

bool ordinary_kind(std::uint8_t kind) {
    return kind >= 7 && kind != 14;
}

bool explosion_kind(std::uint8_t kind) {
    return kind >= 2 && kind <= 5;
}

RiverWorld make_contact_world(const WorldObjectState& state) {
    static constexpr std::array<std::uint8_t, 16> delays{};
    static constexpr std::array<std::uint8_t, 8> choices{};
    static constexpr std::array<std::uint8_t, 16> clearance{};
    return {make_river_generator(), WorldObjectHistory(state, {delays, choices, clearance}),
            generator_edge_patterns()};
}

std::vector<WorldObjectPlacement> placements_for(const RiverWorld& world) {
    std::vector<WorldObjectPlacement> placements;
    for (const auto& band : schedule_world_object_snapshot(
             world.river_state().phase, world.object_state())) {
        placements.insert(placements.end(), band.placements.begin(), band.placements.end());
    }
    return placements;
}

void test_snapshot_adapter_edges() {
    FlightPreview preview;
    preview.start();
    river_raid::test::wait_for_launch(preview);
    const auto hidden = sample_flight_preview_shot_contacts(
        preview.world(), preview.steering(), preview.shot());
    require(!any_contact(hidden), "Hidden projectile was sampled as a contact");

    // 0x31 is the first visible projectile position; its top is above the
    // clipped river, so this checks an occupied-scene transparent miss.
    const PlayerShotResult transparent{{0x31}, PlayerShotPose::Visible, 0x31, std::nullopt};
    const auto miss = sample_flight_preview_shot_contacts(
        preview.world(), preview.steering(), transparent);
    require(!any_contact(miss), "Transparent projectile miss produced a contact");

    bool saw_terrain = false;
    auto probe = preview.steering();
    for (unsigned horizontal = 0; horizontal <= 180 && !saw_terrain; horizontal += 4) {
        probe.horizontal_position = static_cast<std::uint8_t>(horizontal);
        for (unsigned vertical = 0x31; vertical <= 0xD0 && !saw_terrain; vertical += 2) {
            const PlayerShotResult shot{{static_cast<std::uint8_t>(vertical)},
                                        PlayerShotPose::Visible, std::nullopt, std::nullopt};
            const auto contacts = sample_flight_preview_shot_contacts(
                preview.world(), probe, shot);
            for (const auto& contact : contacts) saw_terrain |= contact.terrain_contact;
        }
    }
    require(saw_terrain, "Snapshot adapter never admitted terrain contact");

    // Terrain has priority over an ordinary object in the admitted snapshot.
    ShotContactSnapshot terrain_and_object{};
    auto& terrain_slot = terrain_and_object.slots[5];
    terrain_slot.terrain_contact = true;
    terrain_slot.object_contact = true;
    terrain_slot.primary_present = true;
    terrain_slot.primary_object = {2};
    terrain_and_object.object_kinds[2] = 9;
    const auto terrain_result = select_shot_contact(terrain_and_object);
    require(terrain_result.outcome == ShotContactOutcome::Terrain &&
                terrain_result.clear_projectile,
            "Terrain did not block a coincident ordinary object");
}

void test_ordinary_roles_and_bridge() {
    const auto seed = contact_seed();
    auto world = make_contact_world(seed);
    auto shot = update_player_shot({}, true);
    std::array<ShotContactSlot, kShotContactSlotCount> contacts{};
    contacts[4] = {false, true, true, false, {2}, {}};
    auto result = resolve_shot_contacts(world, shot, contacts, false);
    require(result.selection.outcome == ShotContactOutcome::Ordinary &&
                result.object_effect && result.object_effect->record_index == 2 &&
                world.object_state().records[2].animated_kind == 2 &&
                shot.pose == PlayerShotPose::Hidden,
            "Primary ordinary contact did not destroy its target");

    world = make_contact_world(seed);
    shot = update_player_shot({}, true);
    contacts = {};
    contacts[4] = {false, true, false, true, {}, {3}};
    result = resolve_shot_contacts(world, shot, contacts, false);
    require(result.selection.outcome == ShotContactOutcome::Ordinary &&
                result.object_effect && result.object_effect->record_index == 3 &&
                world.object_state().records[3].animated_kind == 4 &&
                shot.pose == PlayerShotPose::Hidden,
            "Alternate ordinary contact did not destroy its target");

    auto bridge = seed;
    bridge.records[2].animated_kind = 14;
    world = make_contact_world(bridge);
    shot = update_player_shot({}, true);
    contacts = {};
    contacts[5] = {false, true, true, false, {2}, {}};
    result = resolve_shot_contacts(world, shot, contacts, false);
    require(result.selection.outcome == ShotContactOutcome::Bridge &&
                shot.pose == PlayerShotPose::Hidden &&
                world.object_state().records[2].animated_kind == 2,
            "Bridge contact did not destroy the bridge and clear the shot");
}

void test_rendered_effect(std::uint8_t kind) {
    auto seed = contact_seed();
    seed.records[2].animated_kind = seed.records[2].base_kind = kind;
    auto world = make_contact_world(seed);
    std::array<PackedRiverRow, 157> rows{};
    const auto placements_before = placements_for(world);
    const auto frame_before = make_world_scene(rows, placements_before);

    auto shot = update_player_shot({}, true);
    std::array<ShotContactSlot, kShotContactSlotCount> contacts{};
    contacts[4] = {false, true, true, false, {2}, {}};
    const auto result = resolve_shot_contacts(world, shot, contacts, false);
    require(result.object_effect.has_value(), "Render fixture did not resolve");

    const auto placements_after = placements_for(world);
    const auto frame_after = make_world_scene(rows, placements_after);
    require(frame_before.pixels != frame_after.pixels,
            "Destruction did not change the rendered object image");
}

HeldFireRun run_held_fire_preview(FlightPreview& preview) {
    using namespace std::chrono_literals;
    preview.start();
    require(preview.ordinary_hits() == 0 && preview.shot().pose == PlayerShotPose::Hidden,
            "Restart retained hit count or projectile");
    preview.set_controls({false, false, false, false, true});
    std::array<HitAnimationTrack, kWorldObjectCapacity> tracks{};
    HeldFireRun run{};
    std::size_t ticks = 0;
    while (preview.running() && ticks < 5000) {
        const auto before = preview.world().object_state();
        const auto before_rows = preview.world().generated_rows();
        const auto before_lives = preview.lives();
        const auto completed = preview.advance_elapsed(20ms);
        require(completed <= 2, "Held-fire preview exceeded the bounded PAL catch-up interval");
        ticks += completed;

        const auto& after = preview.world().object_state();
        if (preview.lives() != before_lives ||
            preview.world().generated_rows() < before_rows) {
            tracks = {};
            continue;
        }
        const auto rows_advanced = preview.world().generated_rows() - before_rows;
        bool history_shift = after.alternating_update != before.alternating_update;
        if (!history_shift) {
            // A row update increments every retained record's vertical byte;
            // a shift instead copies the preceding record into the slot.
            for (std::size_t index = before.active_begin;
                 index < kWorldObjectCapacity; ++index) {
                const auto expected_vertical = static_cast<std::uint8_t>(
                    before.records[index].vertical_position + rows_advanced);
                if (after.records[index].vertical_position != expected_vertical) {
                    history_shift = true;
                    break;
                }
            }
        }
        // A history shift changes record identity; do not call eviction an
        // explosion expiry, or establish a false transition from the shifted
        // snapshot. Remap retained tracks by source-backed record fields;
        // ambiguous or evicted records are discarded.
        if (history_shift) {
            std::array<HitAnimationTrack, kWorldObjectCapacity> remapped{};
            for (std::size_t old_index = 0; old_index < tracks.size(); ++old_index) {
                if (!tracks[old_index].active) continue;
                const auto& old_record = before.records[old_index];
                const auto expected_vertical = static_cast<std::uint8_t>(
                    old_record.vertical_position + rows_advanced);
                std::size_t match = kWorldObjectCapacity;
                std::size_t matches = 0;
                for (std::size_t new_index = 0; new_index < tracks.size(); ++new_index) {
                    const auto& record = after.records[new_index];
                    if (record.vertical_position != expected_vertical ||
                        record.base_kind != tracks[old_index].base_kind ||
                        record.horizontal_coordinate != old_record.horizontal_coordinate ||
                        record.behavior_flags != old_record.behavior_flags ||
                        record.left_bound != old_record.left_bound ||
                        record.right_bound != old_record.right_bound) {
                        continue;
                    }
                    match = new_index;
                    ++matches;
                }
                if (matches == 1) remapped[match] = tracks[old_index];
            }
            tracks = remapped;
            continue;
        }
        for (std::size_t index = 0; index < tracks.size(); ++index) {
            const auto& old_record = before.records[index];
            const auto& record = after.records[index];
            auto& track = tracks[index];

            if (ordinary_kind(old_record.animated_kind) &&
                explosion_kind(record.animated_kind) && record.animation_count != 0 &&
                record.base_kind == old_record.base_kind) {
                track = {true, record.base_kind, record.animation_count};
                run.saw_explosion = true;
                run.animation_counts.push_back(record.animation_count);
                continue;
            }
            if (!track.active || record.base_kind != track.base_kind) continue;
            if (explosion_kind(record.animated_kind) && record.animation_count != 0) {
                if (record.animation_count < track.last_count) {
                    run.saw_animation_step = true;
                    run.animation_counts.push_back(record.animation_count);
                }
                track.last_count = record.animation_count;
                continue;
            }
            if (record.animated_kind == 0 && record.animation_count == 0) {
                run.saw_animation_clear = true;
                run.animation_counts.push_back(0);
                track = {};
            }
        }
    }
    require(!preview.running() && ticks <= 5000 &&
                preview.status() == FlightStatus::GameOver,
            "Held-fire round did not finish (rows=" +
                std::to_string(preview.world().generated_rows()) + ", ticks=" +
                std::to_string(ticks) + ")");
    require(preview.ordinary_hits() > 0,
            "Natural held-fire preview produced no ordinary projectile hit");
    require(run.saw_explosion && run.saw_animation_step && run.saw_animation_clear,
            "Natural held-fire hit did not evolve and clear as an explosion");
    run.ordinary_hits = preview.ordinary_hits();
    run.final_world = preview.world().object_state();
    return run;
}

void test_restart_determinism() {
    FlightPreview preview;
    require(!preview.running(), "Unstarted preview was unexpectedly active");
    const auto inactive_world = preview.world().object_state();
    require(preview.advance_elapsed(std::chrono::seconds(1)) == 0 &&
                preview.world().object_state() == inactive_world,
            "Inactive preview advanced object animation");

    const auto first = run_held_fire_preview(preview);
    const auto finished_rows = preview.world().generated_rows();
    const auto finished_hits = preview.ordinary_hits();
    require(preview.advance_elapsed(std::chrono::seconds(1)) > 0 &&
                preview.status() == FlightStatus::GameOver &&
                preview.world().generated_rows() == finished_rows &&
                preview.ordinary_hits() == finished_hits && preview.idle_state().countdown < 255,
            "Terminal foreground did not retain terrain and hits during its attract wait");
    const auto second = run_held_fire_preview(preview);
    require(first.ordinary_hits == second.ordinary_hits,
            "Restart changed deterministic ordinary-hit count");
    require(first.final_world == second.final_world,
            "Restart changed final object state");
    require(first.animation_counts == second.animation_counts,
            "Restart changed explosion animation phase sequence");
}

} // namespace

int main() {
    try {
        test_snapshot_adapter_edges();
        test_ordinary_roles_and_bridge();
        test_rendered_effect(9);
        test_rendered_effect(14);
        test_restart_determinism();
        std::cout << "Flight preview snapshot contacts and deterministic ordinary hits verified\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
