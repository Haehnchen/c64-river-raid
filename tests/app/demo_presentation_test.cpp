#include "app/demo_setup.hpp"
#include "app/world_setup.hpp"
#include "fixtures/object_motion_cases.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash_records(const river_raid::WorldObjectState& state) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& record : state.records) {
        for (const auto value : {record.vertical_position, record.horizontal_coordinate,
             record.behavior_flags, record.animated_kind, record.base_kind,
             record.left_bound, record.right_bound, record.animation_count}) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void check_long_handoff() {
    namespace data = river_raid::test_data;
    auto simulation = river_raid::make_demo_simulation();
    auto independent = river_raid::make_river_world();
    (void)independent.advance();
    (void)independent.advance();
    require(!simulation.presentation_step(), "Presentation exists before the first step");
    for (std::size_t index = 2; index + 1 < data::object_motion_cases.size(); ++index) {
        const auto& original = data::object_motion_cases[index];
        const auto before = independent.object_state();
        const auto phase = independent.river_state().phase;
        std::array<river_raid::PackedRiverRow, river_raid::RiverViewport::height> terrain;
        std::ranges::copy(independent.viewport().rows(), terrain.begin());
        const auto candidate = original.activation_candidate == data::kNoObjectMotionCandidate
            ? std::nullopt : std::optional<std::uint8_t>{original.activation_candidate};
        const auto tick = simulation.advance(candidate);
        const auto& step = *simulation.presentation_step();
        require(step.animation_phase == original.animation_phase && step.scroll_phase == phase &&
                step.rows_before == original.rows_before && step.row_count == tick.terrain_rows,
                "Presentation metadata differs from the step");
        require(step.before_motion == before, "Pre-motion presentation was overwritten");
        for (std::size_t slot = 0; slot < before.records.size(); ++slot) {
            const auto& record = original.records[slot];
            require(step.before_motion.records[slot] == river_raid::WorldObjectRecord{
                record.vertical_position, record.horizontal_coordinate, record.behavior_flags,
                record.animated_kind, record.base_kind, record.left_bound, record.right_bound,
                record.animation_count}, "Pre-motion record differs from original");
        }
        independent.advance_motion({tick.animation_phase, independent.river_state().section >= 2, candidate});
        require(step.after_motion == independent.object_state(), "Post-motion presentation differs");
        require(hash_records(step.after_motion) == original.expected_post_fnv1a,
                "Post-motion presentation differs from original");
        require(step.terrain_before_scroll == terrain, "Presentation terrain was scrolled too early");
        for (unsigned row = 0; row < tick.terrain_rows; ++row) {
            (void)independent.advance();
            require(step.after_rows[row] == independent.object_state(), "Intermediate row state differs");
        }
        require(independent.object_state() == simulation.world().object_state(), "Presentation altered simulation");
    }
    const auto saved = simulation.presentation_step();
    const auto clock = simulation.clock_state();
    const auto state = simulation.world().object_state();
    bool rejected = false;
    try { (void)simulation.advance(12); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && simulation.presentation_step() == saved && simulation.clock_state() == clock &&
            simulation.world().object_state() == state, "Rejected input changed presentation or simulation");
    simulation.reset();
    require(!simulation.presentation_step(), "Reset retained a stale presentation step");
    require(saved.has_value(), "Retained presentation copy was lost");
}

void check_scroll_counts() {
    for (const auto speed : {0, 128, 254}) {
        river_raid::DemoSimulation simulation{river_raid::make_river_world(),
            river_raid::DemoClock{{255, 0, static_cast<std::uint8_t>(speed), 0}, {254, 128}}, 2};
        const auto tick = simulation.advance();
        const auto& step = *simulation.presentation_step();
        require(step.row_count == (speed == 0 ? 0 : speed == 128 ? 1 : 2), "Unexpected scroll count");
        require(step.row_count == tick.terrain_rows, "Presentation count differs from clock");
        const auto& final = step.row_count ? step.after_rows[step.row_count - 1] : step.after_motion;
        require(final == simulation.world().object_state(), "Presentation final stage differs");
    }
}
}

int main() {
    try {
        check_long_handoff();
        check_scroll_counts();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
