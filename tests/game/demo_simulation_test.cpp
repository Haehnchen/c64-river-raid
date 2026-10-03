#include "app/demo_setup.hpp"
#include "fixtures/object_motion_cases.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    try {
        namespace data = river_raid::test_data;
        auto simulation = river_raid::make_demo_simulation();
        const auto initial_objects = simulation.world().object_state();
        const auto initial_clock = simulation.clock_state();
        const auto initial_rows = simulation.world().generated_rows();
        for (std::size_t i = 2; i + 1 < data::object_motion_cases.size(); ++i) {
            const auto& before = data::object_motion_cases[i];
            const auto& after = data::object_motion_cases[i + 1];
            if (simulation.world().generated_rows() != before.rows_before) {
                throw std::runtime_error("Demo row schedule differs at call " + std::to_string(before.call));
            }
            const auto candidate = before.activation_candidate == data::kNoObjectMotionCandidate
                ? std::nullopt : std::optional<std::uint8_t>{before.activation_candidate};
            const auto tick = simulation.advance(candidate);
            if (tick.animation_phase != before.animation_phase ||
                simulation.world().generated_rows() != after.rows_before) {
                throw std::runtime_error("Demo phase/scroll differs at call " + std::to_string(before.call));
            }
            const auto& actual = simulation.world().object_state();
            for (std::size_t slot = 0; slot < actual.records.size(); ++slot) {
                const auto& expected = after.records[slot];
                const river_raid::WorldObjectRecord record{expected.vertical_position,
                    expected.horizontal_coordinate, expected.behavior_flags, expected.animated_kind,
                    expected.base_kind, expected.left_bound, expected.right_bound, expected.animation_count};
                if (actual.records[slot] != record) {
                    throw std::runtime_error("Demo object differs at call " + std::to_string(before.call));
                }
            }
        }
        simulation.reset();
        if (simulation.world().object_state() != initial_objects ||
            simulation.clock_state() != initial_clock || simulation.world().generated_rows() != initial_rows) {
            throw std::runtime_error("Demo reset differs");
        }
        bool rejected = false;
        try { (void)simulation.advance(12); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || simulation.clock_state() != initial_clock ||
            simulation.world().object_state() != initial_objects) {
            throw std::runtime_error("Invalid candidate changed demo state");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
