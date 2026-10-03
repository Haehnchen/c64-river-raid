#include "app/world_setup.hpp"
#include "app/generator_setup.hpp"
#include "world_data.hpp"

namespace river_raid {

WorldObjectHistory make_world_object_history() {
    namespace data = assets::world;
    WorldObjectState state{};
    for (std::size_t index = 0; index < state.records.size(); ++index) {
        const auto& source = data::initial_records[index];
        state.records[index] = {source.vertical_position, source.horizontal_coordinate,
            source.flags, source.animated_kind, source.base_kind, source.left_bound,
            source.right_bound, source.animation_count};
    }
    state.active_begin = data::initial_active_begin;
    state.generator_cursor = data::initial_generator_cursor;
    state.special_cursor = data::initial_special_cursor;
    state.spawn_cooldown = data::initial_spawn_cooldown;
    state.spawn_behavior_flags = data::initial_spawn_behavior_flags;
    state.alternating_update = data::initial_parity;
    state.split_collision_wrap = data::initial_split_collision_wrap;
    return {state, {data::spawn_delay_by_kind, data::random_kind_choices, data::kind_clearance}};
}

RiverWorld make_river_world(RiverStartPreset preset) {
    return {make_river_generator(preset), make_world_object_history(), generator_edge_patterns()};
}

RiverWorld make_river_world() { return make_river_world(river_start_preset(0)); }

} // namespace river_raid
