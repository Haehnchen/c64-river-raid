#include "fixtures/object_motion_cases.hpp"
#include "game/world_objects.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

constexpr std::array<std::uint8_t, 16> delays{};
constexpr std::array<std::uint8_t, 8> choices{};
constexpr std::array<std::uint8_t, 16> clearance{};
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;

river_raid::WorldObjectTables tables() {
    return {delays, choices, clearance};
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) {
    hash ^= value;
    hash *= fnv_prime;
}

void hash_records(std::uint64_t& hash,
                  const std::array<river_raid::WorldObjectRecord,
                                   river_raid::kWorldObjectCapacity>& records) {
    for (const auto& record : records) {
        hash_byte(hash, record.vertical_position);
        hash_byte(hash, record.horizontal_coordinate);
        hash_byte(hash, record.behavior_flags);
        hash_byte(hash, record.animated_kind);
        hash_byte(hash, record.base_kind);
        hash_byte(hash, record.left_bound);
        hash_byte(hash, record.right_bound);
        hash_byte(hash, record.animation_count);
    }
}

river_raid::WorldObjectState initial_state(
    const river_raid::test_data::ObjectMotionCase& fixture) {
    river_raid::WorldObjectState state{};
    for (std::size_t index = 0; index < state.records.size(); ++index) {
        const auto& source = fixture.records[index];
        state.records[index] = {
            source.vertical_position,
            source.horizontal_coordinate,
            source.behavior_flags,
            source.animated_kind,
            source.base_kind,
            source.left_bound,
            source.right_bound,
            source.animation_count,
        };
    }
    state.spawn_cooldown = fixture.spawn_cooldown;
    state.active_begin = fixture.active_begin;
    state.generator_cursor = fixture.generator_cursor;
    state.special_cursor = fixture.special_cursor;
    state.alternating_update = fixture.alternating_update;
    state.split_collision_wrap = fixture.split_collision_wrap;
    return state;
}

} // namespace

int main() {
    try {
        std::uint64_t post_stream_hash = fnv_offset;
        std::size_t changed_calls = 0;
        std::uint16_t previous_rows = 0;
        for (const auto& fixture : river_raid::test_data::object_motion_cases) {
            if (fixture.rows_before < previous_rows) {
                throw std::runtime_error("Object motion row coupling moved backwards at call " +
                                         std::to_string(fixture.call));
            }
            previous_rows = fixture.rows_before;
            const auto before = initial_state(fixture);
            river_raid::WorldObjectHistory history(before, tables());
            const auto candidate = fixture.activation_candidate ==
                                           river_raid::test_data::kNoObjectMotionCandidate
                                       ? std::nullopt
                                       : std::optional<std::uint8_t>{fixture.activation_candidate};
            history.advance_motion(
                {fixture.animation_phase, fixture.movement_enabled, candidate});
            const auto& after = history.state();

            std::uint64_t call_hash = fnv_offset;
            hash_records(call_hash, after.records);
            if (call_hash != fixture.expected_post_fnv1a) {
                throw std::runtime_error("Object motion differs from original at call " +
                                         std::to_string(fixture.call));
            }
            hash_records(post_stream_hash, after.records);
            changed_calls += before.records != after.records;
            if (after.active_begin != before.active_begin ||
                after.generator_cursor != before.generator_cursor ||
                after.special_cursor != before.special_cursor ||
                after.spawn_cooldown != before.spawn_cooldown ||
                after.spawn_behavior_flags != before.spawn_behavior_flags ||
                after.alternating_update != before.alternating_update ||
                after.split_collision_wrap != before.split_collision_wrap) {
                throw std::runtime_error("Object motion changed row-history state at call " +
                                         std::to_string(fixture.call));
            }
        }

        if (post_stream_hash != river_raid::test_data::object_motion_post_stream_fnv1a) {
            throw std::runtime_error("Object motion POST stream order differs from original");
        }
        if (changed_calls != 1189) {
            throw std::runtime_error("Object motion changed-call count differs from original");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
