#include "game/river_generator.hpp"
#include "game/river_row.hpp"
#include "game/river_world.hpp"
#include "fixtures/river_row_cases.hpp"
#include "assets/river_generator_data.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using river_raid::RiverGenerator;
using river_raid::RiverGeneratorState;
using river_raid::RiverCheckpoint;
using river_raid::RiverCheckpointSelection;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

namespace generator_assets = river_raid::assets::generator;

constexpr auto feature_choices(const auto& source) {
    std::array<river_raid::TerrainFeatureChoice, std::size(source)> result{};
    for (std::size_t index = 0; index < source.size(); ++index) {
        result[index] = {source[index].margin, source[index].stamp_index};
    }
    return result;
}

constexpr auto regular_features = feature_choices(generator_assets::river_regular_feature_choices);
constexpr auto early_features = feature_choices(generator_assets::river_early_choices);

const auto stamps = [] {
    std::array<river_raid::TerrainStampPattern, 8> result{};
    for (std::size_t width = 3; width <= 10; ++width) {
        const auto span = generator_assets::river_stamp_spans[width];
        result[width - 3] = {
            static_cast<std::uint8_t>(width),
            span.width,
            span.height,
            std::span<const std::uint8_t>(generator_assets::river_stamp_data).subspan(
                span.offset, static_cast<std::size_t>(span.width) * span.height),
        };
    }
    return result;
}();

river_raid::RiverGeneratorTables tables() {
    return {generator_assets::river_course_deltas, generator_assets::river_bridge_lengths,
            regular_features, early_features, stamps};
}

constexpr RiverGeneratorState initial_state() {
    RiverGeneratorState state{};
    state.phase = 0;
    state.scroll_buffer = 5;
    state.section = 1;
    state.course = 32;
    state.channel_span = 20;
    state.bank_inset = 54;
    state.random = {0xE3, 0xD4, 0xF7};
    state.scripted_course = true;
    state.channel_mode = river_raid::RiverChannelMode::Single;
    state.section_transition_pending = false;
    state.target_width_units = 13;
    state.width_interval = 1;
    state.section_interval = 17;
    state.bridge_rows_remaining = 2;
    state.section_anchor = {state.random, state.course};
    return state;
}

void test_checkpoint_selection_is_pure_and_matches_legacy_choices() {
    auto current_state = initial_state();
    current_state.section = 10;
    current_state.section_interval = 10;
    current_state.section_anchor = {{0x11, 0x22, 0x33}, 41};
    current_state.previous_section_anchor = {{0x44, 0x55, 0x66}, 37};
    RiverGenerator current(current_state, tables());
    const auto before_current = current.state();
    require(current.selected_checkpoint() == RiverCheckpoint{10, current_state.section_anchor},
            "Current checkpoint selection changed its section or anchor");
    require(current.state() == before_current,
            "Current checkpoint selection mutated generator state");
    require(current.restore_checkpoint().selection == RiverCheckpointSelection::Current,
            "Legacy restore changed the current checkpoint result");

    auto advanced_state = initial_state();
    advanced_state.section = 10;
    advanced_state.section_interval = 11;
    advanced_state.section_transition_pending = true;
    advanced_state.random = {0x12, 0x34, 0x56};
    advanced_state.course = 49;
    advanced_state.section_anchor = {{0xA1, 0xB2, 0xC3}, 31};
    RiverGenerator advanced(advanced_state, tables());
    const auto before_advanced = advanced.state();
    const RiverCheckpoint advanced_checkpoint{11, {advanced_state.random, advanced_state.course}};
    require(advanced.selected_checkpoint() == advanced_checkpoint,
            "Advanced checkpoint selection did not use the live course state");
    require(advanced.state() == before_advanced,
            "Advanced checkpoint selection mutated generator state");
    const auto advanced_restore = advanced.restore_checkpoint();
    require(advanced_restore.selection == RiverCheckpointSelection::Advanced &&
                advanced.state().section == advanced_checkpoint.section &&
                advanced.state().section_anchor == advanced_checkpoint.anchor &&
                advanced.state().previous_section_anchor == advanced_state.section_anchor,
            "Legacy advanced restore changed its selected checkpoint or previous anchor");

    auto previous_state = initial_state();
    previous_state.section = 10;
    previous_state.section_interval = 11;
    previous_state.scripted_course = false;
    previous_state.previous_section_anchor = {{0xD4, 0xE5, 0xF6}, 27};
    RiverGenerator previous(previous_state, tables());
    const auto before_previous = previous.state();
    const RiverCheckpoint previous_checkpoint{9, previous_state.previous_section_anchor};
    require(previous.selected_checkpoint() == previous_checkpoint,
            "Previous checkpoint selection did not use the previous anchor");
    require(previous.state() == before_previous,
            "Previous checkpoint selection mutated generator state");
    require(previous.restore_checkpoint().selection == RiverCheckpointSelection::Previous &&
                previous.state().section == previous_checkpoint.section &&
                previous.state().section_anchor == previous_checkpoint.anchor,
            "Legacy previous restore changed its selected checkpoint");
}

void test_checkpoint_commit_and_typed_restore_preserve_shared_previous_anchor() {
    auto seed = initial_state();
    seed.section = 10;
    seed.section_interval = 11;
    seed.section_transition_pending = true;
    seed.random = {0x12, 0x34, 0x56};
    seed.course = 49;
    seed.section_anchor = {{0xA1, 0xB2, 0xC3}, 31};
    seed.previous_section_anchor = {{0xD4, 0xE5, 0xF6}, 27};
    RiverGenerator generator(seed, tables());

    const RiverCheckpoint outgoing{11, {seed.random, seed.course}};
    require(generator.commit_checkpoint() == outgoing,
            "Checkpoint commit returned the wrong selected checkpoint");
    require(generator.state().section == outgoing.section &&
                generator.state().section_anchor == outgoing.anchor,
            "Checkpoint commit did not update the outgoing section and anchor");
    require(generator.state().previous_section_anchor == seed.section_anchor,
            "Advanced checkpoint commit did not update the shared previous anchor");
    require(generator.state().phase == seed.phase &&
                generator.state().scroll_buffer == seed.scroll_buffer,
            "Checkpoint commit reset generator state");

    const RiverCheckpoint incoming{23, {{0x70, 0x81, 0x92}, 53}};
    const auto restore = generator.restore_checkpoint(incoming);
    require(restore.selection == RiverCheckpointSelection::Current &&
                restore.player_horizontal_position ==
                    static_cast<std::uint8_t>(incoming.anchor.course + seed.bank_inset),
            "Typed checkpoint restore returned the wrong player position");
    const auto& restored = generator.state();
    require(restored.section == incoming.section && restored.section_anchor == incoming.anchor &&
                restored.course == incoming.anchor.course && restored.random == incoming.anchor.random,
            "Typed checkpoint restore did not install the incoming section and anchor");
    require(restored.previous_section_anchor == seed.section_anchor,
            "Typed checkpoint restore did not retain the committed shared previous anchor");
    require(restored.phase == 0 && restored.scroll_buffer == 5 && restored.scripted_course &&
                restored.channel_mode == river_raid::RiverChannelMode::Single &&
                !restored.section_transition_pending && !restored.feature.active,
            "Typed checkpoint restore did not clear respawn state");

    generator.reset();
    require(generator.state() == seed,
            "New-game reset did not restore the generator's constructor state");
}

void test_set_checkpoint_preserves_live_world_until_restore() {
    const auto seed = initial_state();
    const RiverCheckpoint incoming{23, {{0x70, 0x81, 0x92}, 53}};
    RiverGenerator generator(seed, tables());
    (void)generator.advance();
    auto expected = generator.state();
    expected.section = incoming.section;
    expected.section_anchor = incoming.anchor;
    generator.set_checkpoint(incoming);
    require(generator.state() == expected,
            "Checkpoint setter changed live generator fields beyond section and anchor");
    generator.reset();
    require(generator.state() == seed,
            "Checkpoint setter changed the generator's new-game seed");

    static constexpr std::array<std::uint8_t, 16> delays{};
    static constexpr std::array<std::uint8_t, 8> choices{};
    static constexpr std::array<std::uint8_t, 16> clearance{};
    river_raid::WorldObjectHistory objects({}, {delays, choices, clearance});
    river_raid::RiverWorld world(RiverGenerator(seed, tables()), objects,
                                 generator_assets::river_edge_patterns);
    (void)world.advance();
    const auto before_river = world.river_state();
    const auto before_objects = world.object_state();
    std::array<river_raid::PackedRiverRow, river_raid::RiverViewport::height> before_rows{};
    std::copy(world.viewport().rows().begin(), world.viewport().rows().end(),
              before_rows.begin());

    world.set_checkpoint(incoming);
    auto expected_river = before_river;
    expected_river.section = incoming.section;
    expected_river.section_anchor = incoming.anchor;
    require(world.river_state() == expected_river,
            "World checkpoint setter changed live course or random state");
    require(world.object_state() == before_objects && world.generated_rows() == 1,
            "World checkpoint setter reset object history or row count");
    require(std::equal(world.viewport().rows().begin(), world.viewport().rows().end(),
                       before_rows.begin()),
            "World checkpoint setter cleared the visible river");
}

void require_case(const river_raid::RiverRowInput& actual,
                  const river_raid::test_data::RiverRowCase& expected, std::size_t step) {
    const auto prefix = "Generator differs at step " + std::to_string(step) + ": ";
    require(actual.course == expected.course, prefix + "course");
    require(actual.channel_span == expected.channel_span, prefix + "channel span");
    require(actual.bank_inset == expected.bank_inset, prefix + "bank inset");
    require(actual.split_channel == expected.split, prefix + "channel mode");
    require(actual.bridge == expected.bridge, prefix + "bridge state");
    require(actual.stamp.placement == expected.placement, prefix + "stamp placement");
    require(actual.stamp.margin == expected.margin, prefix + "stamp margin");
    require(actual.stamp.pixels.size() == expected.stamp_width, prefix + "stamp width");
    for (std::size_t index = 0; index < actual.stamp.pixels.size(); ++index) {
        require(actual.stamp.pixels[index] == expected.stamp[index], prefix + "stamp pixels");
    }
    require(river_raid::compose_river_row(actual, generator_assets::river_edge_patterns) ==
                expected.expected,
            prefix + "composed pixels");
}

void test_first_200_native_rows_match_the_reference() {
    struct Checkpoint {
        std::size_t step;
        std::uint8_t phase;
        std::uint8_t buffer;
        std::uint8_t course;
        std::uint8_t channel_span;
        std::uint8_t bank_inset;
        river_raid::RiverRandomState random;
    };
    constexpr std::array checkpoints{
        Checkpoint{32, 0, 1, 32, 22, 53, {0xC6, 0xA9, 0xEF}},
        Checkpoint{64, 0, 2, 27, 72, 28, {0x16, 0x17, 0x14}},
        Checkpoint{96, 0, 3, 28, 72, 28, {0x47, 0x96, 0xC8}},
        Checkpoint{128, 0, 4, 35, 72, 28, {0xB7, 0xC0, 0xAE}},
        Checkpoint{160, 0, 5, 28, 72, 28, {0x7A, 0x78, 0x4D}},
        Checkpoint{192, 0, 1, 30, 72, 28, {0x4D, 0xE2, 0x34}},
        Checkpoint{200, 8, 1, 29, 72, 28, {0xA4, 0x34, 0x4D}},
    };

    RiverGenerator generator(initial_state(), tables());
    for (std::size_t index = 0; index < river_raid::test_data::river_row_cases.size(); ++index) {
        require_case(generator.advance(), river_raid::test_data::river_row_cases[index], index + 1);
        for (const auto& checkpoint : checkpoints) {
            if (checkpoint.step != index + 1) {
                continue;
            }
            const auto& state = generator.state();
            const auto prefix = "Generator state differs at step " +
                                std::to_string(checkpoint.step) + ": ";
            require(state.phase == checkpoint.phase, prefix + "phase");
            require(state.scroll_buffer == checkpoint.buffer, prefix + "scroll buffer");
            require(state.course == checkpoint.course, prefix + "course");
            require(state.channel_span == checkpoint.channel_span, prefix + "channel span");
            require(state.bank_inset == checkpoint.bank_inset, prefix + "bank inset");
            require(state.random == checkpoint.random, prefix + "random state");
        }
    }

    const auto& state = generator.state();
    require(state.phase == 8, "Step 200 has the wrong phase");
    require(state.scroll_buffer == 1, "Step 200 has the wrong scroll buffer");
    require(state.section == 1, "Step 200 has the wrong section");
    require(state.course == 29, "Step 200 has the wrong course");
    require(state.channel_span == 72, "Step 200 has the wrong channel span");
    require(state.bank_inset == 28, "Step 200 has the wrong bank inset");
    require(state.random == river_raid::RiverRandomState{0xA4, 0x34, 0x4D},
            "Step 200 has the wrong random state");
}

void test_reset_restores_the_semantic_seed() {
    RiverGenerator generator(initial_state(), tables());
    (void)generator.advance();
    (void)generator.advance();
    generator.reset();
    require(generator.state() == initial_state(), "Reset did not restore the generator seed");
    require_case(generator.advance(), river_raid::test_data::river_row_cases.front(), 1);
}

void test_width_target_moves_both_banks_and_preserves_total_width() {
    RiverGenerator generator(initial_state(), tables());
    for (std::size_t count = 0; count < 32; ++count) {
        (void)generator.advance();
    }
    const auto& state = generator.state();
    require(state.channel_span == 22 && state.bank_inset == 53,
            "First coarse transition did not move both banks");
    require(static_cast<unsigned>(state.channel_span) + 2U * state.bank_inset == 128,
            "Width transition did not preserve the terrain width");
    require(state.random == river_raid::RiverRandomState{0xC6, 0xA9, 0xEF},
            "First coarse transition did not advance the native random state");
}

void test_neutral_generator_continues_beyond_the_reference_excerpt() {
    RiverGenerator generator(initial_state(), tables());
    river_raid::PackedRiverRow last{};
    for (std::size_t step = 0; step < 4096; ++step) {
        last = river_raid::compose_river_row(generator.advance(),
                                             generator_assets::river_edge_patterns);
    }
    require(last != river_raid::PackedRiverRow{},
            "Extended neutral generator run produced no terrain");
    require(generator.state().section > 1,
            "Extended neutral generator run did not advance its section");
}

void test_typed_object_history_can_reverse_a_course_change() {
    auto state = initial_state();
    state.scripted_course = false;
    state.phase = 1;
    state.course = 32;
    state.course_step = 1;
    state.random = {0x00, 0x00, 0x0F};
    state.width_interval = 2;

    river_raid::RiverObjectHistory objects{};
    objects.entries[0].progress = 8;
    objects.entries[0].horizontal_coordinate = 50;
    RiverGenerator generator(state, tables());
    const auto row = generator.advance(objects);

    require(row.course == 32, "Blocking object must hold the current course");
    require(generator.state().course_step == -1,
            "Blocking object must reverse the semantic course direction");
}

void test_invalid_seed_and_stamp_tables_are_rejected() {
    auto bad_state = initial_state();
    bad_state.phase = 32;
    bool rejected_state = false;
    try {
        (void)RiverGenerator(bad_state, tables());
    } catch (const std::invalid_argument&) {
        rejected_state = true;
    }
    require(rejected_state, "Out-of-range generator phase was accepted");

    auto bad_stamps = stamps;
    bad_stamps[0].pixels = {};
    bool rejected_stamp = false;
    try {
        const river_raid::RiverGeneratorTables bad_tables{
            generator_assets::river_course_deltas, generator_assets::river_bridge_lengths,
            regular_features, early_features, bad_stamps};
        (void)RiverGenerator(initial_state(), bad_tables);
    } catch (const std::invalid_argument&) {
        rejected_stamp = true;
    }
    require(rejected_stamp, "Malformed terrain stamp was accepted");
}

void test_scripted_section_entry_still_advances_random() {
    auto state = initial_state();
    state.phase = 31;
    state.width_interval = 2;
    state.section_interval = 1;
    state.scripted_course = false;
    RiverGenerator generator(state, tables());
    (void)generator.advance();
    require(generator.state().scripted_course, "Section entry must select scripted course");
    require(generator.state().random == river_raid::RiverRandomState{0xC6, 0xA9, 0xEF},
            "Scripted section entry must advance the random state once");
}

void test_channel_transition_first_closes_the_central_gap() {
    auto state = initial_state();
    state.phase = 31;
    state.section = 2;
    state.section_interval = 5;
    state.random = {};
    RiverGenerator generator(state, tables());
    (void)generator.advance();
    require(generator.state().channel_mode == river_raid::RiverChannelMode::PreparingSplit,
            "Channel transition must prepare the split");
    require(generator.state().target_width_units == 0 && generator.state().target_bank_inset == 0,
            "Channel transition must first converge on a zero inset");
}

void test_section_approach_closes_a_split_before_the_bridge() {
    auto state = initial_state();
    state.phase = 31;
    state.section = 2;
    state.section_interval = 3;
    state.channel_mode = river_raid::RiverChannelMode::SplitOpening;
    RiverGenerator generator(state, tables());
    (void)generator.advance();
    require(generator.state().channel_mode == river_raid::RiverChannelMode::SplitClosing &&
            generator.state().target_width_units == 0,
            "Bridge approach must close the active split");
}

} // namespace

int main() {
    try {
        test_checkpoint_selection_is_pure_and_matches_legacy_choices();
        test_checkpoint_commit_and_typed_restore_preserve_shared_previous_anchor();
        test_set_checkpoint_preserves_live_world_until_restore();
        test_first_200_native_rows_match_the_reference();
        test_reset_restores_the_semantic_seed();
        test_width_target_moves_both_banks_and_preserves_total_width();
        test_neutral_generator_continues_beyond_the_reference_excerpt();
        test_typed_object_history_can_reverse_a_course_change();
        test_invalid_seed_and_stamp_tables_are_rejected();
        test_scripted_section_entry_still_advances_random();
        test_channel_transition_first_closes_the_central_gap();
        test_section_approach_closes_a_split_before_the_bridge();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
