#include "app/world_setup.hpp"
#include "fixtures/object_motion_cases.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

int main() {
    try {
        auto world = river_raid::make_river_world();
        for (const auto& fixture : river_raid::test_data::object_motion_cases) {
            if (fixture.rows_before < world.generated_rows()) {
                throw std::runtime_error("Motion sequence moved backwards");
            }
            while (world.generated_rows() < fixture.rows_before) (void)world.advance();
            const auto candidate = fixture.activation_candidate ==
                    river_raid::test_data::kNoObjectMotionCandidate
                ? std::nullopt : std::optional<std::uint8_t>{fixture.activation_candidate};
            world.advance_motion({fixture.animation_phase, fixture.movement_enabled, candidate});
            std::uint64_t hash = 14695981039346656037ULL;
            for (const auto& record : world.object_state().records) {
                for (const auto value : {record.vertical_position, record.horizontal_coordinate,
                        record.behavior_flags, record.animated_kind, record.base_kind,
                        record.left_bound, record.right_bound, record.animation_count}) {
                    hash ^= value;
                    hash *= 1099511628211ULL;
                }
            }
            if (hash != fixture.expected_post_fnv1a) {
                throw std::runtime_error("Coupled world differs at motion call " +
                                         std::to_string(fixture.call));
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
