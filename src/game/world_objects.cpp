#include "game/world_objects.hpp"
#include "game/activation_sample.hpp"

#include <algorithm>
#include <stdexcept>

namespace river_raid {
namespace {

using Byte = std::uint8_t;

constexpr Byte wrapping_add(Byte value, int amount) {
    return static_cast<Byte>(static_cast<int>(value) + amount);
}

constexpr Byte wrapping_subtract(Byte value, int amount) {
    return static_cast<Byte>(static_cast<int>(value) - amount);
}

constexpr bool has_high_bit(Byte value) {
    return (value & 0x80U) != 0;
}

void validate_state(const WorldObjectState& state) {
    if (state.active_begin > kWorldObjectCapacity ||
        state.generator_cursor > kWorldObjectCapacity ||
        state.special_cursor > kWorldObjectCapacity) {
        throw std::invalid_argument("World object cursor exceeds history capacity");
    }
    for (const auto& record : state.records) {
        if (record.animated_kind >= kWorldObjectKindCount ||
            record.base_kind >= kWorldObjectKindCount) {
            throw std::invalid_argument("World object seed has an unknown kind");
        }
    }
}

void validate_tables(const WorldObjectTables& tables) {
    for (const auto kind : tables.random_kind_choices) {
        if (kind >= kWorldObjectKindCount) {
            throw std::invalid_argument("World object choice has an unknown kind");
        }
    }
}

struct SpawnChoice {
    Byte kind{};
    Byte placement{};
    bool updates_split_collision_wrap{};
    bool split_collision_wrap{};
    bool moving_left{};
};

SpawnChoice choose_spawn(const RiverGeneratorState& river,
                         const WorldObjectTables& tables) {
    const auto narrow_margin = river.section < 9 ? Byte{6} : Byte{0};
    auto kind = Byte{1};
    auto placement = Byte{0};
    auto special_padding = false;

    if (river.scripted_course) {
        if (river.phase != 0) {
            return {kind, river.phase, false, false, false};
        }
        kind = 14;
        special_padding = true;
        placement = wrapping_subtract(river.bank_inset, 3);
        if (placement != 0) {
            return {kind, placement, false, false, false};
        }
        --kind;
    } else if (river.phase != 0) {
        placement = wrapping_subtract(river.section, 8);
        if (has_high_bit(placement)) {
            return {kind, placement, false, false, false};
        }
        if (placement < 32) {
            placement = static_cast<Byte>(placement << 3U);
            if (placement < river.random.middle) {
                return {kind, placement, false, false, false};
            }
        }
        kind = tables.random_kind_choices[river.random.middle & 7U];
    } else {
        --kind;
        placement = wrapping_add(river.section_interval, river.width_interval);
        if (placement >= 18) {
            return {kind, placement, false, false, false};
        }
        const auto bounded_section = std::min(river.section, Byte{46});
        placement = static_cast<Byte>((63U - bounded_section) * 2U);
        if (placement >= river.random.middle) {
            if ((river.random.low & 0x40U) != 0) {
                return {kind, placement, false, false, false};
            }
            kind = 15;
        } else {
            kind = tables.random_kind_choices[river.random.middle & 7U];
        }
    }

    if (kind == 7 && river.section < 5) {
        kind = 13;
    }
    if (kind == 12 && (river.phase != 0 || river.section < 3)) {
        kind = 8;
    }
    special_padding = kind == 13;

    Byte width = river.target_width_units;
    if (width == river.previous_target_width_units) {
        if ((river.section & 1U) != 0) {
            const auto limit = wrapping_subtract(special_padding ? Byte{55} : Byte{63},
                                                 narrow_margin);
            auto coordinate = wrapping_add(static_cast<Byte>(river.random.low & 0x3FU),
                                           narrow_margin);
            coordinate = std::min(coordinate, limit);
            return {kind, wrapping_add(coordinate, river.bank_inset), false, false,
                    kind < 15 && has_high_bit(river.random.low)};
        }
    } else {
        width = std::max(width, river.previous_target_width_units);
    }

    const auto split = is_split_channel(river.channel_mode);
    const auto first_width_limit = split ? Byte{13} : Byte{14};
    if (first_width_limit < width && kind != 7) {
        return {1, width, false, false, false};
    }

    auto adjusted_margin = narrow_margin;
    if (river.section_interval == 1) {
        adjusted_margin = wrapping_add(adjusted_margin, 2);
        if (kind == 13) {
            kind = 8;
            special_padding = false;
        }
    } else {
        const auto second_width_limit = split ? Byte{10} : Byte{13};
        if (second_width_limit < width && kind == 13) {
            kind = 8;
            special_padding = false;
        }
    }

    const auto width_pixels = static_cast<Byte>(width * 4U);
    if (split && width_pixels != 0) {
        if (has_high_bit(river.random.low)) {
            placement = wrapping_add(static_cast<Byte>(64U + width_pixels),
                                     adjusted_margin + 1);
        } else {
            placement = wrapping_subtract(static_cast<Byte>(64U - width_pixels),
                                          (kind < 15 ? 9 : 8) + adjusted_margin +
                                              (special_padding ? 8 : 0));
        }
    } else if (has_high_bit(river.random.low)) {
        placement = wrapping_subtract(static_cast<Byte>(128U - width_pixels),
                                      (kind < 15 ? 9 : 8) + adjusted_margin +
                                          (special_padding ? 8 : 0));
    } else {
        placement = wrapping_add(width_pixels, adjusted_margin + 1);
    }
    const auto uses_split_placement = split && width_pixels != 0;
    const auto selects_high_side = has_high_bit(river.random.low);
    return {kind, placement, uses_split_placement,
            uses_split_placement && selects_high_side,
            kind < 15 && (uses_split_placement ? !selects_high_side : selects_high_side)};
}

void update_spawn_behavior(WorldObjectState& state, const RiverGeneratorState& river) {
    if (river.phase != 0) return;

    state.spawn_behavior_flags = static_cast<Byte>((state.spawn_behavior_flags & 0x04U) | 0x10U);
    if (river.scripted_course) {
        state.spawn_behavior_flags = static_cast<Byte>(0x80U | ((river.section & 1U) == 0 ? 4U : 0U));
    }
}

void advance_animation(WorldObjectRecord& record, Byte animation_phase) {
    const auto kind = record.animated_kind;
    if (kind < 2 || kind >= 12 || kind == 7) return;

    const auto phase_mask = kind < 7 ? Byte{7} : Byte{1};
    if ((animation_phase & phase_mask) != 0) return;

    record.animated_kind ^= 1U;
    if (record.animated_kind >= 6) return;

    --record.animation_count;
    if (record.animation_count == 0) record.animated_kind = 0;
}

bool moves_on_tick(Byte kind, std::size_t index, const WorldObjectMotionTick& tick,
                   bool alternating_update) {
    if (!tick.movement_enabled || kind < 7 || kind >= 14) return false;
    if (kind == 7) return true;
    if (kind == 12 && (tick.animation_phase & 2U) != 0) return false;
    return ((index ^ tick.animation_phase ^ static_cast<std::size_t>(alternating_update)) &
            1U) == 0;
}

void advance_horizontal_position(WorldObjectRecord& record, bool uses_object_bounds) {
    constexpr Byte minimum_coordinate = 2;
    constexpr Byte maximum_coordinate = 0xAC;
    constexpr Byte moving_left = 0x08;

    if (uses_object_bounds) {
        if ((record.behavior_flags & moving_left) != 0) {
            if (record.horizontal_coordinate < record.left_bound) {
                record.behavior_flags ^= moving_left;
            }
        } else if (record.horizontal_coordinate >= record.right_bound) {
            record.behavior_flags ^= moving_left;
        }
    }

    if ((record.behavior_flags & moving_left) == 0) {
        ++record.horizontal_coordinate;
        if (record.horizontal_coordinate >= maximum_coordinate) {
            record.horizontal_coordinate = minimum_coordinate;
        }
        return;
    }

    --record.horizontal_coordinate;
    if (record.horizontal_coordinate < minimum_coordinate) {
        record.horizontal_coordinate = maximum_coordinate;
    }
}

struct ObjectExplosionMutation {
    std::size_t record_index{};
    Byte destroyed_kind{};
};

std::optional<ObjectExplosionMutation> apply_object_explosion(
    WorldObjectState& state, std::size_t record_index, WorldObjectHitSlot slot) {
    if (record_index >= state.records.size() ||
        (slot != WorldObjectHitSlot::Primary && slot != WorldObjectHitSlot::Alternate)) {
        return std::nullopt;
    }
    auto& record = state.records[record_index];
    const auto kind = record.animated_kind;
    if (kind < 7) return std::nullopt;
    record.animated_kind = slot == WorldObjectHitSlot::Primary ? 2 : 4;
    record.animation_count = 6;
    return ObjectExplosionMutation{record_index, kind};
}

} // namespace

WorldObjectHistory::WorldObjectHistory(WorldObjectState initial_state, WorldObjectTables tables)
    : initial_state_(initial_state), state_(initial_state), tables_(tables) {
    validate_state(initial_state_);
    validate_tables(tables_);
}

void WorldObjectHistory::advance_motion(const WorldObjectMotionTick& tick) {
    if (tick.activation_candidate && *tick.activation_candidate >= kWorldObjectCapacity) {
        throw std::invalid_argument("World object activation candidate exceeds history capacity");
    }

    if (is_activation_phase(tick.animation_phase) && tick.activation_candidate &&
        *tick.activation_candidate != state_.generator_cursor) {
        auto& candidate = state_.records[*tick.activation_candidate];
        const auto movement_width = wrapping_subtract(candidate.right_bound,
                                                      candidate.left_bound);
        if (movement_width >= 4 && movement_width < 0xE0) {
            candidate.behavior_flags |= 0x40U;
        }
    }

    for (std::size_t index = state_.active_begin; index < state_.records.size(); ++index) {
        auto& record = state_.records[index];
        const auto kind_before_animation = record.animated_kind;
        advance_animation(record, tick.animation_phase);
        if (moves_on_tick(kind_before_animation, index, tick, state_.alternating_update) &&
            (kind_before_animation == 7 || (record.behavior_flags & 0x40U) != 0)) {
            advance_horizontal_position(record, kind_before_animation != 7);
        }
    }
}

std::optional<WorldObjectHitEffect> WorldObjectHistory::apply_projectile_hit(
    std::size_t record_index, WorldObjectHitSlot slot, bool round_event_pending) {
    const auto mutation = apply_object_explosion(state_, record_index, slot);
    if (!mutation) return std::nullopt;
    return WorldObjectHitEffect{mutation->record_index, mutation->destroyed_kind,
        round_event_pending ? std::nullopt
                            : std::optional<std::uint8_t>{mutation->destroyed_kind}, 31};
}

std::optional<GapContactHitEffect> WorldObjectHistory::apply_gap_contact_hit(
    std::size_t record_index, WorldObjectHitSlot slot) {
    const auto mutation = apply_object_explosion(state_, record_index, slot);
    if (!mutation) return std::nullopt;
    return GapContactHitEffect{mutation->record_index, mutation->destroyed_kind};
}

WorldObjectRowResult WorldObjectHistory::advance_after_row(const RiverGeneratorState& river,
                                           const RiverGuides& guides) {
    update_spawn_behavior(state_, river);

    for (auto& record : state_.records) {
        ++record.vertical_position;
    }

    const bool history_shifted = state_.records.back().vertical_position >= 0xD4 &&
                                 state_.active_begin < kWorldObjectCapacity;
    if (history_shifted) {
        if (state_.active_begin == kWorldObjectCapacity - 1) {
            state_.records.back() = state_.records[kWorldObjectCapacity - 2];
        } else {
            for (std::size_t index = kWorldObjectCapacity - 1;
                 index > state_.active_begin; --index) {
                state_.records[index] = state_.records[index - 1];
            }
        }
        ++state_.active_begin;
        state_.alternating_update = !state_.alternating_update;
        if (state_.special_cursor < kWorldObjectCapacity) {
            ++state_.special_cursor;
        }
        if (state_.generator_cursor < kWorldObjectCapacity) {
            ++state_.generator_cursor;
        }
    }

    if (state_.generator_cursor < kWorldObjectCapacity &&
        (state_.spawn_cooldown == 0 || has_high_bit(state_.spawn_cooldown))) {
        state_.generator_cursor = kWorldObjectCapacity;
    }

    bool auxiliary_selected = false;
    if (state_.generator_cursor == kWorldObjectCapacity &&
        (river.phase & 0x0FU) == 0 && state_.active_begin != 0) {
        auxiliary_selected = spawn(river, guides);
    }

    --state_.spawn_cooldown;
    return {history_shifted, auxiliary_selected};
}

RiverObjectHistory WorldObjectHistory::generator_view() const noexcept {
    RiverObjectHistory result{};
    result.cursor = state_.generator_cursor;
    result.split_collision_wrap = state_.split_collision_wrap;
    for (std::size_t index = 0; index < state_.records.size(); ++index) {
        const auto& record = state_.records[index];
        result.entries[index] = {
            record.animated_kind,
            record.base_kind,
            record.horizontal_coordinate,
            tables_.kind_clearance[index] != 0,
        };
    }
    return result;
}

const WorldObjectState& WorldObjectHistory::state() const noexcept {
    return state_;
}

void WorldObjectHistory::reset() noexcept {
    state_ = initial_state_;
}

bool WorldObjectHistory::spawn(const RiverGeneratorState& river, const RiverGuides& guides) {
    --state_.active_begin;
    state_.generator_cursor = state_.active_begin;
    auto& record = state_.records[state_.active_begin];
    record.behavior_flags = state_.spawn_behavior_flags;

    const auto choice = choose_spawn(river, tables_);
    if (choice.updates_split_collision_wrap) {
        state_.split_collision_wrap = choice.split_collision_wrap;
    }
    if (choice.moving_left) {
        record.behavior_flags |= 0x08U;
    }
    auto kind = choice.kind;
    auto horizontal = wrapping_add(river.course, wrapping_subtract(choice.placement, 4));

    record.horizontal_coordinate = horizontal;
    record.animated_kind = kind;
    record.base_kind = kind;

    const bool auxiliary_selected = kind == 8 && river.section_interval < 14 &&
        state_.special_cursor >= kWorldObjectCapacity &&
        (river.random.high & 0x70U) == 0 && river.section >= 13;
    if (auxiliary_selected) {
        kind = 10;
        record.animated_kind = record.base_kind = kind;
        state_.special_cursor = state_.active_begin;
    }

    if (kind >= 8) {
        if (is_split_channel(river.channel_mode) && guides.second_left == 0) {
            kind = river.phase == 0 ? Byte{0} : Byte{1};
            horizontal = kind;
            record.horizontal_coordinate = horizontal;
            record.animated_kind = kind;
            record.base_kind = kind;
        } else {
            const auto second_channel = is_split_channel(river.channel_mode) && horizontal >= 0x57;
            record.left_bound = second_channel ? guides.second_left : guides.first_left;
            record.right_bound = second_channel ? guides.second_right : guides.first_right;
            if (tables_.kind_clearance[kind] != 0) {
                record.right_bound = wrapping_subtract(record.right_bound, 8);
            }
        }
    }

    state_.spawn_cooldown = wrapping_add(tables_.spawn_delay_by_kind[record.animated_kind], 4);
    record.vertical_position = river.phase == 0 ? Byte{0x21} : Byte{0x2B};
    if (record.base_kind == 10 &&
        wrapping_subtract(record.right_bound, record.left_bound) >= 4) {
        record.behavior_flags = static_cast<Byte>(record.behavior_flags | 0x40U);
    }
    return auxiliary_selected;
}

} // namespace river_raid
