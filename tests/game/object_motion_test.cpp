#include "game/world_objects.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using river_raid::WorldObjectHistory;
using river_raid::WorldObjectMotionTick;
using river_raid::WorldObjectState;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

constexpr std::array<std::uint8_t, 16> delays{};
constexpr std::array<std::uint8_t, 8> choices{};
constexpr std::array<std::uint8_t, 16> clearance{};

river_raid::WorldObjectTables tables() {
    return {delays, choices, clearance};
}

void test_kind_animation_masks_and_lifetime() {
    WorldObjectState state{};
    state.active_begin = 0;
    state.records[0].animated_kind = 2;
    state.records[0].animation_count = 2;
    state.records[1].animated_kind = 3;
    state.records[1].animation_count = 1;
    state.records[2].animated_kind = 4;
    state.records[2].animation_count = 0;
    state.records[3].animated_kind = 6;
    state.records[4].animated_kind = 7;
    state.records[5].animated_kind = 8;
    state.records[6].animated_kind = 10;
    state.records[7].animated_kind = 11;
    state.records[8].animated_kind = 12;
    state.records[9].animated_kind = 13;
    state.records[10].animated_kind = 14;
    state.records[11].animated_kind = 15;

    WorldObjectHistory history(state, tables());
    history.advance_motion({0, false, std::nullopt});
    const auto& records = history.state().records;

    require(records[0].animated_kind == 3 && records[0].animation_count == 1,
            "Kind 2 did not advance on the eight-phase boundary");
    require(records[1].animated_kind == 0 && records[1].animation_count == 0,
            "Exhausted low-kind animation did not become inactive");
    require(records[2].animated_kind == 5 && records[2].animation_count == 0xFF,
            "Zero animation lifetime did not wrap like an 8-bit counter");
    require(records[3].animated_kind == 7,
            "Kind 6 did not enter its paired frame without consuming lifetime");
    require(records[4].animated_kind == 7, "Kind 7 was treated as an animation pair");
    require(records[5].animated_kind == 9 && records[6].animated_kind == 11 &&
                records[7].animated_kind == 10,
            "High-kind animation did not toggle on an even phase");
    require(records[8].animated_kind == 12 && records[9].animated_kind == 13 &&
                records[10].animated_kind == 14 && records[11].animated_kind == 15,
            "Non-animated kinds changed unexpectedly");

    history.advance_motion({1, false, std::nullopt});
    require(history.state().records[5].animated_kind == 9 &&
                history.state().records[7].animated_kind == 10,
            "High-kind animation ignored its even-phase mask");
}

void test_motion_direction_parity_and_wrapping() {
    WorldObjectState state{};
    state.active_begin = 0;

    state.records[0] = {0, 50, 0x40, 0, 0, 0, 100, 0};
    state.records[1] = {0, 60, 0x40, 7, 7, 10, 60, 0};
    state.records[2] = {0, 9, 0x48, 7, 7, 10, 80, 0};
    state.records[3] = {0, 30, 0x40, 8, 8, 0, 100, 0};
    state.records[4] = {0, 30, 0x40, 8, 8, 0, 100, 0};
    state.records[5] = {0, 40, 0x40, 12, 12, 0, 100, 0};
    state.records[6] = {0, 0xAB, 0x40, 7, 7, 0, 0xFF, 0};
    state.records[7] = {0, 2, 0x48, 7, 7, 0, 0xFF, 0};
    state.records[8] = {0, 70, 0x00, 8, 8, 0, 100, 0};
    state.records[9] = {0, 70, 0x00, 7, 7, 0, 100, 0};
    state.records[10] = {0, 60, 0x40, 8, 8, 10, 60, 0};

    WorldObjectHistory history(state, tables());
    history.advance_motion({0, true, std::nullopt});
    const auto& records = history.state().records;

    require(records[0].horizontal_coordinate == 50, "Kinds below 7 moved");
    require(records[1].horizontal_coordinate == 61 && records[1].behavior_flags == 0x40,
            "Right-moving kind 7 used its per-record boundary");
    require(records[2].horizontal_coordinate == 8 && records[2].behavior_flags == 0x48,
            "Left-moving kind 7 used its per-record boundary");
    require(records[3].horizontal_coordinate == 30 && records[4].horizontal_coordinate == 31,
            "Alternating high-kind motion selected the wrong slot parity");
    require(records[5].horizontal_coordinate == 40,
            "Kind 12 moved on a slot parity that was not selected");
    require(records[6].horizontal_coordinate == 2 && records[7].horizontal_coordinate == 0xAC,
            "Horizontal motion did not wrap at the original screen limits");
    require(records[8].horizontal_coordinate == 70,
            "A high-kind record moved without its movement-enabled behavior flag");
    require(records[9].horizontal_coordinate == 71,
            "Kind 7 incorrectly required the movement-enabled behavior flag");
    require(records[10].horizontal_coordinate == 59 && records[10].behavior_flags == 0x48,
            "Kind 8 stopped reversing at its per-record boundary");

    WorldObjectState left_boundary{};
    left_boundary.active_begin = 0;
    left_boundary.records[0] = {0, 9, 0x48, 8, 8, 10, 80, 0};
    WorldObjectHistory left_boundary_history(left_boundary, tables());
    left_boundary_history.advance_motion({0, true, std::nullopt});
    require(left_boundary_history.state().records[0].horizontal_coordinate == 10 &&
                left_boundary_history.state().records[0].behavior_flags == 0x40,
            "Kind 8 stopped reversing at its left per-record boundary");

    history.advance_motion({2, true, std::nullopt});
    require(history.state().records[5].horizontal_coordinate == 40,
            "Kind 12 ignored its additional two-phase gate");

    WorldObjectState alternating = state;
    alternating.alternating_update = true;
    WorldObjectHistory alternating_history(alternating, tables());
    alternating_history.advance_motion({0, true, std::nullopt});
    require(alternating_history.state().records[3].horizontal_coordinate == 31 &&
                alternating_history.state().records[4].horizontal_coordinate == 30,
            "History alternation did not invert high-kind slot parity");
}

void test_activation_event_width_cursor_and_phase_guards() {
    WorldObjectState state{};
    state.active_begin = 0;
    state.generator_cursor = 2;
    state.records[1].left_bound = 10;
    state.records[1].right_bound = 14;
    state.records[2].left_bound = 10;
    state.records[2].right_bound = 30;
    state.records[3].left_bound = 10;
    state.records[3].right_bound = 13;
    state.records[4].left_bound = 30;
    state.records[4].right_bound = 2;

    WorldObjectHistory history(state, tables());
    history.advance_motion({1, false, 1});
    require(history.state().records[1].behavior_flags == 0,
            "Activation ran away from its 16-phase boundary");

    history.advance_motion({16, false, 1});
    history.advance_motion({16, false, 2});
    history.advance_motion({16, false, 3});
    history.advance_motion({16, false, 4});
    require(history.state().records[1].behavior_flags == 0x40,
            "Eligible activation candidate did not gain movement");
    require(history.state().records[2].behavior_flags == 0,
            "Generator-selected record was activated");
    require(history.state().records[3].behavior_flags == 0 &&
                history.state().records[4].behavior_flags == 0,
            "Out-of-range wrapped movement width was accepted");

    bool rejected = false;
    try {
        history.advance_motion({16, false, 12});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Out-of-range activation candidate was accepted");
}

void test_observed_natural_pal_transitions() {
    // Calls 83, 291 and 1043 from the unpatched $8DDA-$851E PAL trace.
    WorldObjectState animation{};
    animation.active_begin = 8;
    animation.generator_cursor = 8;
    animation.alternating_update = true;
    animation.records[8] = {33, 65, 0x14, 8, 8, 50, 110, 0};
    WorldObjectHistory animation_history(animation, tables());
    animation_history.advance_motion({80, false, 3});
    require(animation_history.state().records[8] ==
                river_raid::WorldObjectRecord{33, 65, 0x14, 9, 8, 50, 110, 0},
            "Observed natural animation transition differs");

    WorldObjectState activation{};
    activation.active_begin = 2;
    activation.generator_cursor = 2;
    activation.records[6] = {113, 104, 0x14, 13, 13, 56, 108, 0};
    WorldObjectHistory activation_history(activation, tables());
    activation_history.advance_motion({32, false, 6});
    require(activation_history.state().records[6] ==
                river_raid::WorldObjectRecord{113, 104, 0x54, 13, 13, 56, 108, 0},
            "Observed natural activation transition differs");

    WorldObjectState movement{};
    movement.active_begin = 2;
    movement.generator_cursor = 2;
    movement.alternating_update = true;
    movement.records[7] = {129, 81, 0x5C, 8, 8, 54, 114, 0};
    movement.records[11] = {193, 94, 0x54, 13, 13, 52, 104, 0};
    WorldObjectHistory movement_history(movement, tables());
    movement_history.advance_motion({16, true, 0});
    require(movement_history.state().records[7] ==
                river_raid::WorldObjectRecord{129, 80, 0x5C, 9, 8, 54, 114, 0} &&
                movement_history.state().records[11] ==
                    river_raid::WorldObjectRecord{193, 95, 0x54, 13, 13, 52, 104, 0},
            "Observed natural alternating movement transition differs");
}

void test_spawn_behavior_mode_and_initial_direction() {
    WorldObjectState scripted{};
    scripted.spawn_behavior_flags = 0x84;
    scripted.active_begin = 1;
    scripted.generator_cursor = river_raid::kWorldObjectCapacity;
    river_raid::RiverGeneratorState scripted_river{};
    scripted_river.phase = 0;
    scripted_river.section = 1;
    scripted_river.course = 20;
    scripted_river.bank_inset = 10;
    scripted_river.scripted_course = true;
    WorldObjectHistory scripted_history(scripted, tables());
    scripted_history.advance_after_row(scripted_river, {});
    require(scripted_history.state().spawn_behavior_flags == 0x80 &&
                scripted_history.state().records[0].behavior_flags == 0x80,
            "Odd scripted section did not preserve its spawn behavior mode");

    constexpr std::array<std::uint8_t, 8> moving_choices{8, 0, 0, 0, 0, 0, 0, 0};
    WorldObjectState moving{};
    moving.spawn_behavior_flags = 0x84;
    moving.active_begin = 1;
    moving.generator_cursor = river_raid::kWorldObjectCapacity;
    river_raid::RiverGeneratorState river{};
    river.phase = 0;
    river.section = 11;
    river.course = 20;
    river.bank_inset = 10;
    river.random = {0x80, 200, 0};
    river.target_width_units = 5;
    river.previous_target_width_units = 5;
    river.width_interval = 1;
    river.section_interval = 1;
    WorldObjectHistory moving_history(moving, {delays, moving_choices, clearance});
    moving_history.advance_after_row(river, {10, 100, 0, 0});
    require(moving_history.state().records[0].animated_kind == 8 &&
                moving_history.state().records[0].behavior_flags == 0x1C,
            "Spawn placement did not set the observed initial direction");
}

} // namespace

int main() {
    try {
        test_kind_animation_masks_and_lifetime();
        test_motion_direction_parity_and_wrapping();
        test_activation_event_width_cursor_and_phase_guards();
        test_observed_natural_pal_transitions();
        test_spawn_behavior_mode_and_initial_direction();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
