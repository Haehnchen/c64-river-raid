#include "app/world_setup.hpp"
#include "game/river_generator.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using river_raid::RiverCheckpointSelection;
using river_raid::RiverGenerator;
using river_raid::RiverGeneratorState;
using river_raid::RiverRandomState;
using river_raid::RiverSectionAnchor;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

const auto& tables() {
    static constexpr std::array<std::int8_t, 32> deltas{};
    static constexpr std::array<std::uint8_t, 32> bridge_lengths{};
    static constexpr std::array<river_raid::TerrainFeatureChoice, 26> regular{};
    static constexpr std::array<river_raid::TerrainFeatureChoice, 8> early{};
    static constexpr std::array<std::uint8_t, 1> pixel{0};
    static constexpr std::array<river_raid::TerrainStampPattern, 1> stamps{{
        {0, 1, 1, pixel},
    }};
    static const river_raid::RiverGeneratorTables result{
        deltas, bridge_lengths, regular, early, stamps,
    };
    return result;
}

RiverGeneratorState state_for(std::uint8_t section, std::uint8_t interval,
                              bool scripted) {
    RiverGeneratorState state{};
    state.phase = 17;
    state.scroll_buffer = 3;
    state.section = section;
    state.course = 42;
    state.channel_span = 72;
    state.bank_inset = 28;
    state.random = {0x12, 0x34, 0x56};
    state.scripted_course = scripted;
    state.channel_mode = river_raid::RiverChannelMode::SplitClosing;
    state.transition_bias = 3;
    state.target_width_units = 9;
    state.previous_target_width_units = 8;
    state.width_interval = 2;
    state.section_interval = interval;
    state.target_bank_inset = 36;
    state.bank_inset_step = 1;
    state.course_step = -1;
    state.bridge_rows_remaining = 7;
    state.feature = {true, 0, 0, 0, river_raid::RiverBankSide::Left};
    state.section_anchor = {{0xA1, 0xB2, 0xC3}, 31};
    state.previous_section_anchor = {{0xD4, 0xE5, 0xF6}, 27};
    return state;
}

void require_respawn_core(const RiverGeneratorState& state,
                          const RiverSectionAnchor& anchor) {
    require(state.phase == 0 && state.scroll_buffer == 5,
            "Respawn did not reset the row phase and scrolling buffer");
    require(state.random == anchor.random && state.course == anchor.course,
            "Respawn did not load the selected section anchor");
    require(state.scripted_course &&
                state.channel_mode == river_raid::RiverChannelMode::Single,
            "Respawn did not enter the scripted single-channel course");
    require(!state.section_transition_pending,
            "Respawn retained the consumed bridge progress latch");
    require(state.transition_bias == 0,
            "Respawn retained the cleared transition bias");
    require(!state.feature.active, "Respawn retained a partial terrain feature");
}

void test_destroyed_bridge_advances_from_runtime_course() {
    const auto seed = state_for(10, 1, true);
    RiverGenerator generator(seed, tables());
    generator.mark_bridge_destroyed();
    const auto restore = generator.restore_checkpoint();
    require(restore.selection == RiverCheckpointSelection::Advanced,
            "Destroyed bridge did not select the next checkpoint");

    const auto expected = RiverSectionAnchor{seed.random, seed.course};
    const auto& restored = generator.state();
    require(restored.section == 11, "Destroyed bridge did not advance the section");
    require(restored.previous_section_anchor == seed.section_anchor,
            "Destroyed bridge did not retain the old current anchor as previous");
    require(restored.section_anchor == expected,
            "Destroyed bridge checkpoint was not made from the live generator state");
    require(restore.player_horizontal_position ==
                static_cast<std::uint8_t>(expected.course + seed.bank_inset),
            "Respawn did not derive player horizontal position from the selected course");
    require_respawn_core(restored, expected);
}

void test_checkpoint_window_boundaries() {
    for (const auto interval : {std::uint8_t{1}, std::uint8_t{11}, std::uint8_t{17}}) {
        auto seed = state_for(10, interval, true);
        RiverGenerator generator(seed, tables());
        generator.mark_bridge_destroyed();
        require(generator.restore_checkpoint().selection == RiverCheckpointSelection::Advanced,
                "Eligible checkpoint window did not advance");
    }
    for (const auto interval : {std::uint8_t{2}, std::uint8_t{10}}) {
        auto seed = state_for(10, interval, true);
        RiverGenerator generator(seed, tables());
        generator.mark_bridge_destroyed();
        require(generator.restore_checkpoint().selection == RiverCheckpointSelection::Current,
                "Ineligible checkpoint window changed the section");
        require(generator.state().section == seed.section &&
                    generator.state().section_anchor == seed.section_anchor,
                "Ineligible checkpoint window changed its current anchor");
        require_respawn_core(generator.state(), seed.section_anchor);
    }
}

void test_uncleared_bridge_restores_previous_anchor() {
    const auto seed = state_for(10, 11, false);
    RiverGenerator generator(seed, tables());
    require(generator.restore_checkpoint().selection == RiverCheckpointSelection::Previous,
            "Uncleared non-scripted bridge did not select the previous checkpoint");
    require(generator.state().section == 9, "Previous checkpoint did not decrement section");
    require(generator.state().section_anchor == seed.previous_section_anchor,
            "Previous checkpoint did not restore the previous anchor");
    require_respawn_core(generator.state(), seed.previous_section_anchor);
}

void test_original_section_end_remap() {
    auto advance_seed = state_for(80, 11, true);
    RiverGenerator advanced(advance_seed, tables());
    advanced.mark_bridge_destroyed();
    require(advanced.restore_checkpoint().selection == RiverCheckpointSelection::Advanced &&
                advanced.state().section == 79,
            "Section 80 advance did not clamp to section 79");

    const auto previous_seed = state_for(79, 11, false);
    RiverGenerator previous(previous_seed, tables());
    require(previous.restore_checkpoint().selection == RiverCheckpointSelection::Previous &&
                previous.state().section == 80,
            "Section 79 rewind did not remap to section 80");
}

void test_zero_course_uses_original_subtract_carry() {
    auto seed = state_for(10, 11, true);
    seed.course = 0;
    RiverGenerator generator(seed, tables());
    generator.mark_bridge_destroyed();
    const auto restore = generator.restore_checkpoint();
    require(restore.player_horizontal_position ==
                static_cast<std::uint8_t>(seed.bank_inset - 1),
            "Zero course did not preserve the original SBC carry result");
}

void test_initial_life_gate_keeps_the_current_anchor() {
    auto world = river_raid::make_river_world();
    world.begin_life();
    for (std::size_t row = 0; row < river_raid::RiverViewport::height; ++row) {
        (void)world.advance();
    }
    require(world.river_state().section == 1 &&
                world.river_state().section_interval == 14 &&
                !world.river_state().scripted_course,
            "Initial viewport did not reach the early source rewind window");

    require(world.river_state().section_transition_pending,
            "Source life-entry gate did not retain checkpoint progress");
    const auto restore = world.restore_checkpoint();
    require(restore.selection == RiverCheckpointSelection::Current &&
                restore.player_horizontal_position == 86,
            "Initial life gate did not retain the current anchor");
    world.begin_life();
    for (std::size_t row = 0; row < river_raid::RiverViewport::height; ++row) {
        (void)world.advance();
    }
    require(world.river_state().section == 1 && world.river_state().phase == 0,
            "Initial life checkpoint did not rebuild a complete viewport");
}

void test_source_previous_anchor_wraps_its_first_row() {
    auto world = river_raid::make_river_world();
    for (std::size_t row = 0; row < river_raid::RiverViewport::height; ++row) {
        (void)world.advance();
    }
    const auto restore = world.restore_checkpoint();
    require(restore.selection == RiverCheckpointSelection::Previous &&
                restore.player_horizontal_position == 53 &&
                world.river_state().section == 0 && world.river_state().course == 0,
            "Source previous-anchor selection changed");
    (void)world.advance();
    require(world.river_state().course == 0xFF,
            "First scripted previous-anchor row did not wrap its course byte");
}

void test_world_restore_discards_only_volatile_presentation() {
    auto world = river_raid::make_river_world();
    const auto initial_objects = world.object_state();
    for (int row = 0; row < 20; ++row) (void)world.advance();
    world.mark_bridge_destroyed();
    require(world.restore_checkpoint().selection == RiverCheckpointSelection::Advanced,
            "Initial scripted bridge did not advance the world checkpoint");
    require(world.river_state().section == 2,
            "World checkpoint did not retain course progression");
    require(world.object_state() == initial_objects && world.generated_rows() == 0,
            "World checkpoint did not reset object and row history");
    for (const auto& row : world.viewport().rows()) {
        require(row == river_raid::PackedRiverRow{},
                "World checkpoint retained a saved framebuffer row");
    }

    world.reset();
    require(world.river_state().section == 1,
            "New-game reset retained checkpoint progression");
}

void test_first_bridge_route_has_a_deterministic_checkpoint_destination() {
    auto world = river_raid::make_river_world();
    for (int row = 0; row < 1037; ++row) (void)world.advance();
    const auto& bridge = world.river_state();
    require(bridge.section == 1 && bridge.phase == 13 && bridge.width_interval == 1 &&
                bridge.section_interval == 1 && bridge.scripted_course,
            "First bridge route no longer reaches the source checkpoint branch");

    world.mark_bridge_destroyed();
    const auto restore = world.restore_checkpoint();
    require(restore.selection == RiverCheckpointSelection::Advanced &&
                world.river_state().section == 2,
            "First bridge did not select section 2");
    require(world.river_state().width_interval == 1 &&
                world.river_state().section_interval == 17 &&
                world.river_state().target_width_units == 13,
            "Checkpoint respawn did not reload the source interval seed");
    for (std::size_t row = 0; row < river_raid::RiverViewport::height; ++row) {
        (void)world.advance();
    }
    require(world.river_state().section == 2 && world.river_state().phase == 0,
            "Checkpoint viewport rebuild did not retain its section 2 destination");
}

} // namespace

int main() {
    try {
        test_destroyed_bridge_advances_from_runtime_course();
        test_checkpoint_window_boundaries();
        test_uncleared_bridge_restores_previous_anchor();
        test_original_section_end_remap();
        test_zero_course_uses_original_subtract_carry();
        test_initial_life_gate_keeps_the_current_anchor();
        test_source_previous_anchor_wraps_its_first_row();
        test_world_restore_discards_only_volatile_presentation();
        test_first_bridge_route_has_a_deterministic_checkpoint_destination();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
