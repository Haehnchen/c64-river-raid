#include "render/world_object_schedule.hpp"
#include "game/demo_presentation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using river_raid::WorldObjectBandSlots;
using river_raid::WorldObjectImageSlot;
using river_raid::WorldObjectRecord;
using river_raid::WorldObjectState;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

WorldObjectRecord record(std::uint8_t y, std::uint8_t x, std::uint8_t flags,
                         std::uint8_t animated_kind, std::uint8_t base_kind) {
    return {y, x, flags, animated_kind, base_kind, 0, 0, 0};
}

WorldObjectState empty_state() {
    WorldObjectState state{};
    state.active_begin = river_raid::kWorldObjectCapacity;
    for (auto& entry : state.records) entry.vertical_position = 0xff;
    return state;
}

void test_single_band_selection_and_coordinates() {
    auto state = empty_state();
    state.records[3] = record(54, 10, 0x08, 5, 13);
    state.records[4] = record(55, 130, 0x00, 7, 8);
    state.records[5] = record(1, 40, 0x00, 3, 4);

    const auto primary_then_alternate = river_raid::schedule_world_object_band(
        state, 3, 55, WorldObjectBandSlots::primary_then_alternate);
    require(primary_then_alternate.exclusive_bottom == 55, "Band cut was not retained");
    require(primary_then_alternate.next_record == 4,
            "Equality at the cut did not stop the cursor");
    require(primary_then_alternate.placements.size() == 1,
            "Band did not stop before an equal-Y record");
    require(primary_then_alternate.placements[0] ==
                river_raid::WorldObjectPlacement{
                    3, WorldObjectImageSlot::primary, -4, 4, 21, 13},
            "Primary placement semantics differ");

    state.records[4].vertical_position = 54;
    const auto two_slots = river_raid::schedule_world_object_band(
        state, 3, 55, WorldObjectBandSlots::primary_then_alternate);
    require(two_slots.next_record == 5 && two_slots.placements.size() == 2,
            "Two-slot band capacity differs");
    require(two_slots.placements[1] ==
                river_raid::WorldObjectPlacement{
                    4, WorldObjectImageSlot::alternate, 236, 4, 7, 8},
            "Alternate placement semantics differ");

    const auto alternate_only = river_raid::schedule_world_object_band(
        state, 3, 55, WorldObjectBandSlots::alternate_only);
    require(alternate_only.next_record == 4 && alternate_only.placements.size() == 1,
            "Alternate-only band consumed the wrong number of records");
    require(alternate_only.placements[0].image_slot == WorldObjectImageSlot::alternate,
            "Alternate-only band used the primary image");

    const auto at_end = river_raid::schedule_world_object_band(
        state, river_raid::kWorldObjectCapacity, 255,
        WorldObjectBandSlots::primary_then_alternate);
    const auto past_end = river_raid::schedule_world_object_band(
        state, 99, 255, WorldObjectBandSlots::alternate_only);
    require(at_end.placements.empty() && at_end.next_record == 12,
            "Capacity cursor was not terminal");
    require(past_end.placements.empty() && past_end.next_record == 99,
            "Past-capacity cursor was changed");
}

std::vector<int> expected_cuts(std::uint8_t phase) {
    std::vector<int> cuts;
    for (int band = 0; band <= 4; ++band) {
        cuts.push_back(55 + static_cast<int>(phase) + 32 * band);
    }
    if (phase <= 20) cuts.push_back(211);
    return cuts;
}

void test_all_phase_cut_sequences() {
    const auto state = empty_state();
    for (std::uint8_t phase = 0; phase < 32; ++phase) {
        const auto expected = expected_cuts(phase);
        std::vector<WorldObjectState> later(expected.size() - 1, state);
        const auto bands = river_raid::schedule_world_object_bands(phase, state, later);
        require(river_raid::schedule_world_object_snapshot(phase, state) == bands,
                "Diagnostic snapshot differs from explicit identical band states");
        require(bands.size() == expected.size(), "Phase produced the wrong band count");
        for (std::size_t index = 0; index < expected.size(); ++index) {
            require(bands[index].exclusive_bottom == expected[index],
                    "Phase produced the wrong band cut");
        }
    }

    bool bad_phase_rejected = false;
    try {
        (void)river_raid::schedule_world_object_bands(32, state, {});
    } catch (const std::invalid_argument&) {
        bad_phase_rejected = true;
    }
    require(bad_phase_rejected, "Out-of-range phase was accepted");

    std::array<WorldObjectState, 4> too_few{};
    bool bad_snapshot_count_rejected = false;
    try {
        (void)river_raid::schedule_world_object_bands(0, state, too_few);
    } catch (const std::invalid_argument&) {
        bad_snapshot_count_rejected = true;
    }
    require(bad_snapshot_count_rejected, "Mismatched band snapshots were accepted");
}

void test_explicit_band_snapshots_and_cursor_continuity() {
    auto top = empty_state();
    top.active_begin = 2;
    top.records[2] = record(54, 20, 0x00, 3, 4);
    top.records[3] = record(87, 21, 0x00, 4, 5);

    std::array<WorldObjectState, 5> later{};
    const std::array<int, 5> cuts{87, 119, 151, 183, 211};
    for (std::size_t band = 0; band < later.size(); ++band) {
        later[band] = empty_state();
        const auto cursor = 3 + band * 2;
        later[band].records[cursor] = record(
            static_cast<std::uint8_t>(cuts[band] - 1),
            static_cast<std::uint8_t>(30 + band), 0x08,
            static_cast<std::uint8_t>(band + 1), static_cast<std::uint8_t>(band + 6));
        if (cursor + 1 < river_raid::kWorldObjectCapacity) {
            later[band].records[cursor + 1] = record(
                static_cast<std::uint8_t>(cuts[band] - 1),
                static_cast<std::uint8_t>(40 + band), 0x00,
                static_cast<std::uint8_t>(band + 2),
                static_cast<std::uint8_t>(band + 7));
        }
    }

    const auto bands = river_raid::schedule_world_object_bands(0, top, later);
    require(bands.size() == 6, "Phase zero did not include the fixed final cut");
    require(bands[0].placements ==
                std::vector<river_raid::WorldObjectPlacement>{{
                    2, WorldObjectImageSlot::alternate, 16, 4, 3, 4}},
            "Top band did not read the explicit pre-motion snapshot");

    for (std::size_t band = 1; band < bands.size(); ++band) {
        const auto first_record = 3 + (band - 1) * 2;
        const auto expected_count = first_record + 1 < river_raid::kWorldObjectCapacity ? 2U : 1U;
        require(bands[band].placements.size() == expected_count,
                "Later band did not read its own snapshot at the retained cursor");
        require(bands[band].placements.front().record_index == first_record &&
                    bands[band].placements.front().image_slot ==
                        WorldObjectImageSlot::primary &&
                    bands[band].placements.front().left ==
                        2 * static_cast<int>(30 + band - 1) - 24,
                "Later band placement differs from its explicit snapshot");
        if (expected_count == 2) {
            require(bands[band].placements[1].record_index == first_record + 1 &&
                        bands[band].placements[1].image_slot ==
                            WorldObjectImageSlot::alternate,
                    "Later band alternate slot or cursor differs");
        }
    }
    require(bands.back().next_record == river_raid::kWorldObjectCapacity,
            "Cursor did not remain continuous across snapshots");

    auto phase_16_top = empty_state();
    phase_16_top.active_begin = 0;
    phase_16_top.records[0] = record(70, 24, 0, 1, 1);
    phase_16_top.records[1] = record(70, 25, 0, 2, 2);
    std::array<WorldObjectState, 5> phase_16_later{};
    phase_16_later.fill(empty_state());
    const auto phase_16 =
        river_raid::schedule_world_object_bands(16, phase_16_top, phase_16_later);
    require(phase_16.front().placements.size() == 2 &&
                phase_16.front().placements[0].image_slot ==
                    WorldObjectImageSlot::primary &&
                phase_16.front().placements[1].image_slot ==
                    WorldObjectImageSlot::alternate,
            "Phase-16 top band did not expose both slots in order");
}

void test_shared_later_snapshot() {
    auto top = empty_state();
    top.active_begin = 1;
    auto later = empty_state();
    for (std::size_t index = 1; index < top.records.size(); ++index) {
        const auto y = static_cast<std::uint8_t>(50 + (index / 2) * 27);
        top.records[index] = record(y, static_cast<std::uint8_t>(20 + index), 0, 1, 2);
        later.records[index] = record(y, static_cast<std::uint8_t>(90 + index), 8, 3, 4);
    }

    river_raid::DemoPresentationStep step{};
    step.before_motion = top;
    step.after_motion = later;
    for (std::uint8_t phase = 0; phase < 32; ++phase) {
        const std::vector<WorldObjectState> explicit_later(expected_cuts(phase).size() - 1, later);
        const auto expected = river_raid::schedule_world_object_bands(phase, top, explicit_later);
        require(river_raid::schedule_world_object_snapshot(phase, top, later) == expected,
                "Shared later snapshot changed band assignments or cursor continuity");
        step.scroll_phase = phase;
        require(river_raid::schedule_demo_presentation(step) == expected,
                "Demo presentation changed pre-motion or post-motion band ownership");
        require(!expected.front().placements.empty() && !expected.back().placements.empty(),
                "Shared snapshot fixture did not exercise both top and later assignments");
    }

    bool bad_phase_rejected = false;
    try {
        (void)river_raid::schedule_world_object_snapshot(32, top, later);
    } catch (const std::invalid_argument&) {
        bad_phase_rejected = true;
    }
    require(bad_phase_rejected, "Shared snapshot accepted an out-of-range phase");
}

} // namespace

int main() {
    try {
        test_single_band_selection_and_coordinates();
        test_all_phase_cut_sequences();
        test_explicit_band_snapshots_and_cursor_continuity();
        test_shared_later_snapshot();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
