#include "app/foreground_contacts.hpp"
#include "app/world_setup.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_generated_gap_depot_hit() {
    auto world = make_river_world();
    std::uint8_t phase = 0;
    // Component cadence: one foreground update followed by one generated row.
    for (int row = 0; row < 6894; ++row) {
        world.advance_motion({phase, world.river_state().section >= 2, std::nullopt});
        world.advance_bridge_sprites(phase, true);
        ++phase;
        (void)world.advance();
    }
    const auto contacts = sample_foreground_contacts(
        world, {45, 0, 0, 0}, FlightPose::Straight,
        {{}, PlayerShotPose::Hidden, std::nullopt, std::nullopt});
    world.advance_motion({phase, world.river_state().section >= 2, std::nullopt});
    world.advance_bridge_crossing(phase, true);
    const auto aircraft = select_player_contact(contacts.player, world.object_state());
    require(!aircraft.contacted() && !aircraft.refuel_contact &&
                !contacts.player.player_fully_offscreen && !contacts.shot_presented,
            "Generated fixture does not reach the gap consumer");
    const auto sprites = world.bridge_sprite_state();
    const auto selected = select_gap_contact(sprites.gap_animation_counter,
                                             contacts.gap, world.object_state());
    require(world.generated_rows() == 6894 && sprites.gap_animation_counter == 18 &&
                selected.object == GapObjectTarget{4, WorldObjectHitSlot::Primary} &&
                !selected.fatal_player_contact,
            "Generated gap/depot contact changed");
    auto expected = world.object_state();
    require(expected.records[4].animated_kind == 15,
            "Generated gap fixture lost its depot");
    expected.records[4].animated_kind = 2;
    expected.records[4].animation_count = 6;
    const auto river = world.river_state();
    const auto effect = world.apply_gap_contact_hit(selected.object->record_index,
                                                   selected.object->slot);
    require(effect && effect->record_index == 4 && effect->destroyed_kind == 15 &&
                world.object_state() == expected && world.river_state() == river &&
                world.bridge_sprite_state() == sprites && world.generated_rows() == 6894,
            "Silent gap mutation changed state outside its contacted object");
    const auto repeated = select_gap_contact(sprites.gap_animation_counter,
                                             contacts.gap, world.object_state());
    require(!repeated.object && !repeated.fatal_player_contact,
            "Retained gap snapshot ignored the newly mutated object kind");
    world.advance_bridge_gap(phase, true);
    require(world.bridge_sprite_state().gap_animation_counter == 19,
            "Gap publication did not follow the contact mutation");
}
} // namespace

int main() {
    try {
        test_generated_gap_depot_hit();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
