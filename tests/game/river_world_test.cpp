#include "app/generator_setup.hpp"
#include "app/world_setup.hpp"
#include "fixtures/river_long_trace.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, std::size_t step, const std::string& field) {
    if (!condition) throw std::runtime_error("World differs at row " + std::to_string(step) + ": " + field);
}

void check_generator(const river_raid::RiverGeneratorState& actual,
                     const river_raid::test_data::RiverLongTraceState& expected) {
    const auto step = expected.step;
    require(actual.phase == expected.phase, step, "phase");
    require(actual.scroll_buffer == expected.buffer_index, step, "buffer");
    require(actual.section == expected.level, step, "section");
    require(actual.course == expected.course, step, "course");
    require(actual.channel_span == expected.channel_span, step, "channel span");
    require(actual.bank_inset == expected.bank_inset, step, "bank inset");
    require(actual.random == expected.random, step, "random state");
}
} // namespace

int main() {
    try {
        auto world = river_raid::make_river_world();
        const auto seed = world.object_state();
        auto isolated = river_raid::make_river_generator();
        for (int replay = 0; replay < 2; ++replay) {
            world.reset();
            isolated.reset();
            require(world.generated_rows() == 0 && world.object_state() == seed, 0, "reset");
            for (const auto& expected : river_raid::test_data::river_long_trace) {
                const auto& objects = world.object_state();
                require(objects.generator_cursor == expected.object_cursor, expected.step, "object cursor");
                require(objects.active_begin == expected.active_begin, expected.step, "active begin");
                require(objects.special_cursor == expected.special_cursor, expected.step, "special cursor");
                require(objects.spawn_cooldown == expected.cooldown, expected.step, "spawn cooldown");
                require(objects.alternating_update == expected.parity, expected.step, "slot parity");
                require(objects.split_collision_wrap == (expected.split_collision_wrap != 0),
                        expected.step, "split object side");
                for (std::size_t slot = 0; slot < objects.records.size(); ++slot) {
                    const auto& actual = objects.records[slot];
                    const auto& record = expected.objects[slot];
                    const auto prefix = "slot " + std::to_string(slot) + ' ';
                    require(actual.vertical_position == record.vertical_position, expected.step, prefix + "vertical");
                    // Frame-dependent movement of older objects is a separate module.
                    if (slot == objects.generator_cursor) {
                        require(actual.horizontal_coordinate == record.horizontal_coordinate,
                                expected.step, prefix + "generator-relevant horizontal");
                    }
                    require(actual.base_kind == record.base_kind, expected.step, prefix + "base kind");
                    require(actual.left_bound == record.left_bound, expected.step, prefix + "left bound");
                    require(actual.right_bound == record.right_bound, expected.step, prefix + "right bound");
                }
                require(world.advance() == expected.row, expected.step, "native composed row");
                check_generator(world.river_state(), expected);
                const auto row = isolated.advance(river_raid::test_data::observed_object_history(expected));
                require(river_raid::compose_river_row(row, river_raid::generator_edge_patterns()) == expected.row,
                        expected.step, "isolated generator row");
                check_generator(isolated.state(), expected);
            }
            const auto& trace = river_raid::test_data::river_long_trace;
            require(world.generated_rows() == trace.size(), trace.size(), "generated count");
            require(world.viewport().rows().front() == trace.back().row &&
                    world.viewport().rows().back() == trace[trace.size() - river_raid::RiverViewport::height].row,
                    trace.size(), "viewport ordering");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
