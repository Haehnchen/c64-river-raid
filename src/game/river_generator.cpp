#include "game/river_generator.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace river_raid {
namespace {

using Byte = std::uint8_t;

Byte wrapping_add(Byte value, int amount) {
    return static_cast<Byte>(static_cast<int>(value) + amount);
}

Byte wrapping_subtract(Byte value, int amount) {
    return static_cast<Byte>(static_cast<int>(value) - amount);
}

bool transition_side(RiverChannelMode mode) {
    return mode == RiverChannelMode::PreparingSplit || mode == RiverChannelMode::SplitOpening;
}

void advance_random(RiverRandomState& random) {
    const auto old_low = random.low;
    const auto old_middle = random.middle;
    const auto old_high = random.high;
    const auto feedback = static_cast<Byte>(((old_middle >> 7U) ^ (old_middle >> 2U)) & 1U);

    random.low = static_cast<Byte>((old_low << 1U) | feedback);
    random.high = static_cast<Byte>((old_high << 1U) | (old_low >> 7U));
    random.middle = static_cast<Byte>((old_middle << 1U) | (old_high >> 7U));
}

void validate_state(const RiverGeneratorState& state) {
    if (state.phase >= 32) {
        throw std::invalid_argument("River generator phase must be below 32");
    }
    if (state.scroll_buffer < 1 || state.scroll_buffer > 5) {
        throw std::invalid_argument("River generator scroll buffer must be in range 1..5");
    }
    if (state.width_interval == 0 || state.section_interval == 0) {
        throw std::invalid_argument("River generator intervals must be non-zero");
    }
    if (state.bank_inset_step < -2 || state.bank_inset_step > 2) {
        throw std::invalid_argument("River generator bank step is outside its supported range");
    }
    if (state.course_step < -1 || state.course_step > 1) {
        throw std::invalid_argument("River generator course step is outside its supported range");
    }
}

const TerrainStampPattern* find_stamp(std::span<const TerrainStampPattern> stamps, Byte id) {
    const auto found = std::find_if(stamps.begin(), stamps.end(), [id](const auto& stamp) {
        return stamp.id == id;
    });
    return found == stamps.end() ? nullptr : &*found;
}

void validate_tables(const RiverGeneratorTables& tables) {
    for (const auto& stamp : tables.stamps) {
        if (stamp.width == 0 || stamp.height == 0 ||
            stamp.pixels.size() != static_cast<std::size_t>(stamp.width) * stamp.height) {
            throw std::invalid_argument("River generator has a malformed terrain stamp");
        }
        const auto duplicate = std::find_if(&stamp + 1, tables.stamps.data() + tables.stamps.size(),
                                            [&stamp](const auto& candidate) {
                                                return candidate.id == stamp.id;
                                            });
        if (duplicate != tables.stamps.data() + tables.stamps.size()) {
            throw std::invalid_argument("River generator terrain stamp ids must be unique");
        }
    }

    const auto validate_choice = [&tables](const TerrainFeatureChoice& choice) {
        if (find_stamp(tables.stamps, choice.stamp_id) == nullptr) {
            throw std::invalid_argument("River generator feature refers to an unknown stamp");
        }
    };
    std::for_each(tables.regular_features.begin(), tables.regular_features.end(), validate_choice);
    std::for_each(tables.early_section_features.begin(), tables.early_section_features.end(),
                  validate_choice);
}

bool update_channel_mode(RiverGeneratorState& state) {
    if (state.section_interval == 2) {
        if (state.channel_mode == RiverChannelMode::SplitOpening) {
            state.channel_mode = RiverChannelMode::SplitClosing;
            return true;
        }
        state.channel_mode = RiverChannelMode::Single;
        return false;
    }
    const auto split = is_split_channel(state.channel_mode);
    const auto side = transition_side(state.channel_mode);
    if (split != side) {
        state.channel_mode = side ? RiverChannelMode::SplitOpening : RiverChannelMode::Single;
        return false;
    }
    if ((state.random.low & 0x30U) != 0) {
        return false;
    }
    state.channel_mode = split ? RiverChannelMode::SplitClosing
                               : RiverChannelMode::PreparingSplit;
    return true;
}

Byte select_target_width(RiverGeneratorState& state, Byte narrow_level_margin) {
    if (update_channel_mode(state)) return 0;

    auto target = static_cast<Byte>(state.random.low & 0x0FU);
    if (target < 2) {
        target = static_cast<Byte>(target + 2);
    }

    Byte maximum = 15;
    if (is_split_channel(state.channel_mode)) {
        --maximum;
    }
    if (state.section < 20) {
        --maximum;
    }
    if (narrow_level_margin != 0) {
        maximum = 8;
    }
    return std::min(target, maximum);
}

void select_bank_target(RiverGeneratorState& state, Byte target) {
    state.target_bank_inset = target;
    const auto difference = static_cast<int>(target) - static_cast<int>(state.bank_inset);
    if (difference > 0) {
        state.bank_inset_step = difference >= 32 ? 2 : 1;
    } else if (difference < 0) {
        state.bank_inset_step = difference >= -32 ? -1 : -2;
    }
}

void advance_section(RiverGeneratorState& state) {
    state.previous_section_anchor = state.section_anchor;
    state.section_anchor = {state.random, state.course};
    state.section = state.section < 80 ? static_cast<Byte>(state.section + 1) : Byte{79};
}

void restore_previous_section(RiverGeneratorState& state) {
    auto section = static_cast<Byte>(state.section - 1);
    if (section == 78) section = 80;
    state.section = section;
    state.section_anchor = state.previous_section_anchor;
}

void initialize_respawn_state(RiverGeneratorState& state,
                              const RiverGeneratorState& initial_state,
                              RiverCheckpoint checkpoint,
                              RiverSectionAnchor previous_section_anchor) {
    state = initial_state;
    state.phase = 0;
    state.scroll_buffer = 5;
    state.section = checkpoint.section;
    state.course = checkpoint.anchor.course;
    state.channel_span = initial_state.channel_span;
    state.bank_inset = initial_state.bank_inset;
    state.random = checkpoint.anchor.random;
    state.scripted_course = true;
    state.channel_mode = RiverChannelMode::Single;
    state.section_transition_pending = false;
    state.transition_bias = 0;
    state.previous_target_width_units = 0;
    state.target_bank_inset = 0;
    state.bank_inset_step = 0;
    state.course_step = 0;
    state.bridge_rows_remaining = initial_state.bridge_rows_remaining;
    state.feature = {};
    state.section_anchor = checkpoint.anchor;
    state.previous_section_anchor = previous_section_anchor;
}

Byte respawn_horizontal_position(const RiverGeneratorState& state) {
    if (state.course == 0) return wrapping_subtract(state.bank_inset, 1);
    return wrapping_add(state.course, state.bank_inset);
}

void advance_coarse_phase(RiverGeneratorState& state, Byte narrow_level_margin) {
    state.scroll_buffer = state.scroll_buffer == 5 ? Byte{1}
                                                   : static_cast<Byte>(state.scroll_buffer + 1);
    state.phase = 0;
    state.feature = {};
    state.transition_bias = 0;
    state.scripted_course = false;
    state.previous_target_width_units = state.target_width_units;

    --state.width_interval;
    if (state.width_interval != 0) {
        if (static_cast<Byte>(state.section_interval - 1) != 0) {
            advance_random(state.random);
            return;
        }
        state.section_transition_pending = false;
        state.scripted_course = true;
        advance_random(state.random);
        return;
    }

    --state.section_interval;
    if (state.section_interval == 0) {
        advance_section(state);
        state.section_interval = 16;
    }
    advance_random(state.random);

    if (static_cast<Byte>(state.section_interval - 1) == 0) {
        state.channel_mode = RiverChannelMode::Single;
        state.target_width_units = 13;
        if ((state.section & 1U) != 0) {
            --state.transition_bias;
        }
        state.width_interval = 2;
        select_bank_target(state, 54);
        return;
    }

    const auto target_width = (state.section & 1U) != 0
                                  ? Byte{7}
                                  : select_target_width(state, narrow_level_margin);
    state.target_width_units = target_width;
    state.width_interval = 2;
    select_bank_target(state, static_cast<Byte>(target_width * 4U));
}

void advance_width(RiverGeneratorState& state) {
    if (state.bank_inset_step == 0) {
        return;
    }

    const auto reached_target = state.bank_inset_step < 0
                                    ? state.target_bank_inset >= state.bank_inset
                                    : state.bank_inset >= state.target_bank_inset;
    if (reached_target) {
        state.bank_inset_step = 0;
        return;
    }

    state.bank_inset = wrapping_add(state.bank_inset, state.bank_inset_step);
    state.channel_span = wrapping_subtract(state.channel_span, state.bank_inset_step * 2);
}

struct CourseLimits {
    Byte lower;
    Byte upper;
};

CourseLimits course_limits(const RiverGeneratorState& state) {
    CourseLimits limits{1, 64};
    if (state.section < 3) {
        return {28, 36};
    }
    if ((state.section & 1U) == 0 || state.transition_bias != 0 || state.section < 10) {
        return {25, 40};
    }
    if (state.section < 20) {
        return {13, 52};
    }
    if (!state.feature.active) {
        return limits;
    }
    if (state.feature.side == RiverBankSide::Left) {
        limits.lower = 25;
    } else {
        limits.upper = 40;
    }
    return limits;
}

void reverse_course(RiverGeneratorState& state) {
    state.course_step = static_cast<std::int8_t>(-state.course_step);
}

Byte negative_object_boundary(const RiverGeneratorState& state,
                              const RiverObjectHistory& objects, Byte maximum_inset) {
    if (!is_split_channel(state.channel_mode)) {
        return wrapping_add(state.course, 128 - maximum_inset);
    }
    if (objects.split_collision_wrap) {
        return wrapping_add(state.course, 128);
    }
    return wrapping_add(state.course, 64 - maximum_inset);
}

Byte positive_object_boundary(const RiverGeneratorState& state,
                              const RiverObjectHistory& objects, Byte maximum_inset) {
    const auto split_offset = is_split_channel(state.channel_mode) && objects.split_collision_wrap
                                  ? 64
                                  : 0;
    return wrapping_add(state.course, split_offset + maximum_inset + 2);
}

void move_course(RiverGeneratorState& state, const RiverObjectHistory& objects,
                 Byte narrow_level_margin) {
    const auto limits = course_limits(state);
    const auto maximum_inset = std::max(state.bank_inset, state.target_bank_inset);
    const auto has_object = objects.cursor < objects.entries.size() &&
                            objects.entries[objects.cursor].progress >= 8;

    if (state.course_step < 0) {
        if (state.course < limits.lower) {
            reverse_course(state);
            return;
        }
        if (has_object) {
            const auto& object = objects.entries[objects.cursor];
            const auto clearance = 13 + narrow_level_margin +
                                   (object.extended_clearance ? 8 : 0);
            const auto object_edge = wrapping_add(object.horizontal_coordinate, clearance);
            if (object_edge >= negative_object_boundary(state, objects, maximum_inset)) {
                reverse_course(state);
                return;
            }
        }
        --state.course;
        return;
    }

    if (state.course >= limits.upper) {
        reverse_course(state);
        return;
    }
    if (has_object) {
        const auto& object = objects.entries[objects.cursor];
        const auto object_edge = wrapping_add(object.horizontal_coordinate,
                                              4 - narrow_level_margin);
        if (object_edge < positive_object_boundary(state, objects, maximum_inset)) {
            reverse_course(state);
            return;
        }
    }
    ++state.course;
}

void advance_free_course(RiverGeneratorState& state, const RiverObjectHistory& objects,
                         Byte narrow_level_margin) {
    const auto decision = static_cast<Byte>(state.random.high & 0x0FU);
    if (state.course_step == 0) {
        state.course_step = decision < 8 ? -1 : 1;
    } else {
        if (decision < 8) {
            return;
        }
        if (decision == 13) {
            reverse_course(state);
        } else if (decision == 15) {
            const auto threshold = is_split_channel(state.channel_mode) ? Byte{13} : Byte{14};
            if (threshold < state.target_width_units ||
                threshold < state.previous_target_width_units) {
                return;
            }
        }
    }
    move_course(state, objects, narrow_level_margin);
}

void select_feature(RiverGeneratorState& state, const RiverGeneratorTables& tables) {
    if (state.scripted_course || state.phase != 0 || (state.random.high & 0x20U) == 0) {
        return;
    }

    TerrainFeatureChoice choice{};
    if ((state.section & 1U) != 0) {
        choice = tables.early_section_features[state.random.low & 7U];
    } else {
        auto candidate = std::min(state.target_width_units, state.previous_target_width_units);
        if (candidate < 2) {
            return;
        }
        if (is_split_channel(state.channel_mode)) {
            if (candidate < 5) {
                return;
            }
            candidate = static_cast<Byte>(candidate + 11);
        }
        if (candidate >= tables.regular_features.size()) {
            throw std::out_of_range("River generator feature selection left the supported table");
        }
        choice = tables.regular_features[candidate];
    }

    const auto high_side = (state.random.high & 0x80U) != 0;
    const auto left = high_side ? state.course >= 24 : state.course >= 41;
    state.feature = {
        true,
        choice.stamp_id,
        0,
        choice.margin,
        left ? RiverBankSide::Left : RiverBankSide::Right,
    };
    if (state.feature.side == RiverBankSide::Right && state.feature.margin == 0) {
        state.feature.active = false;
    }
}

void advance_course(RiverGeneratorState& state, const RiverGeneratorTables& tables,
                    const RiverObjectHistory& objects, Byte narrow_level_margin) {
    if (state.scripted_course) {
        state.course_step = 0;
        state.bridge_rows_remaining = tables.bridge_lengths[state.phase];
        state.course = wrapping_add(state.course, tables.scripted_course_deltas[state.phase]);
        return;
    }

    if (state.transition_bias != 0) {
        if (state.course < 24) {
            ++state.course;
            return;
        }
        if (state.course >= 41) {
            --state.course;
            return;
        }
    }
    advance_free_course(state, objects, narrow_level_margin);
}

RiverRowInput make_row(RiverGeneratorState& state, const RiverGeneratorTables& tables) {
    RiverRowInput row{
        state.course,
        state.channel_span,
        state.bank_inset,
        is_split_channel(state.channel_mode),
        state.bridge_rows_remaining != 0,
        {},
    };

    if (state.feature.active) {
        const auto* pattern = find_stamp(tables.stamps, state.feature.stamp_id);
        if (pattern == nullptr) {
            throw std::logic_error("River generator lost its active terrain stamp");
        }
        const auto stamp_is_visible = state.phase < pattern->height &&
                                      state.feature.row_index < pattern->height &&
                                      (!row.split_channel || state.bank_inset >= 8);
        if (stamp_is_visible) {
            const auto offset = static_cast<std::size_t>(state.feature.row_index) * pattern->width;
            row.stamp = {
                pattern->pixels.subspan(offset, pattern->width),
                state.feature.margin,
                row.split_channel ? StampPlacement::Island
                                  : (state.feature.side == RiverBankSide::Left
                                         ? StampPlacement::LeftBank
                                         : StampPlacement::RightBank),
            };
            ++state.feature.row_index;
        }
    }

    if (state.bridge_rows_remaining != 0) {
        --state.bridge_rows_remaining;
    }
    return row;
}

} // namespace

bool is_split_channel(RiverChannelMode mode) noexcept {
    return mode == RiverChannelMode::SplitOpening || mode == RiverChannelMode::SplitClosing;
}

RiverGenerator::RiverGenerator(RiverGeneratorState initial_state, RiverGeneratorTables tables)
    : initial_state_(initial_state), state_(initial_state), tables_(tables) {
    validate_state(initial_state_);
    validate_tables(tables_);
    if (initial_state_.feature.active) {
        (void)stamp(initial_state_.feature.stamp_id);
    }
}

RiverRowInput RiverGenerator::advance() {
    return advance(RiverObjectHistory{});
}

RiverRowInput RiverGenerator::advance(const RiverObjectHistory& objects) {
    const auto narrow_level_margin = state_.section < 9 ? Byte{6} : Byte{0};
    ++state_.phase;
    if (state_.phase >= 32) {
        advance_coarse_phase(state_, narrow_level_margin);
    } else if (!state_.scripted_course) {
        advance_random(state_.random);
    }

    advance_width(state_);
    advance_course(state_, tables_, objects, narrow_level_margin);
    select_feature(state_, tables_);
    return make_row(state_, tables_);
}

const RiverGeneratorState& RiverGenerator::state() const noexcept {
    return state_;
}

void RiverGenerator::begin_life() noexcept {
    state_.section_transition_pending = true;
}

void RiverGenerator::mark_bridge_destroyed() noexcept {
    state_.section_transition_pending = true;
}

void RiverGenerator::set_checkpoint(RiverCheckpoint checkpoint) noexcept {
    state_.section = checkpoint.section;
    state_.section_anchor = checkpoint.anchor;
}

RiverCheckpointRestore RiverGenerator::restore_checkpoint() noexcept {
    const auto choice = checkpoint_choice();
    (void)commit_checkpoint_choice(choice);
    auto restore = restore_checkpoint(choice.checkpoint);
    restore.selection = choice.selection;
    return restore;
}

RiverCheckpoint RiverGenerator::selected_checkpoint() const noexcept {
    return checkpoint_choice().checkpoint;
}

RiverCheckpoint RiverGenerator::commit_checkpoint() noexcept {
    return commit_checkpoint_choice(checkpoint_choice());
}

RiverCheckpoint RiverGenerator::commit_checkpoint_choice(const CheckpointChoice& choice) noexcept {
    state_.section = choice.checkpoint.section;
    state_.section_anchor = choice.checkpoint.anchor;
    state_.previous_section_anchor = choice.previous_section_anchor;
    return choice.checkpoint;
}

RiverGenerator::CheckpointChoice RiverGenerator::checkpoint_choice() const noexcept {
    auto selected_state = state_;
    auto selection = RiverCheckpointSelection::Current;
    const auto checkpoint_window = state_.section_interval >= 11 ||
                                   state_.section_interval == 1;
    if (checkpoint_window && state_.section_transition_pending && state_.scripted_course) {
        advance_section(selected_state);
        selection = RiverCheckpointSelection::Advanced;
    } else if (checkpoint_window && !state_.section_transition_pending &&
               !state_.scripted_course) {
        restore_previous_section(selected_state);
        selection = RiverCheckpointSelection::Previous;
    }
    return {selection,
            {selected_state.section, selected_state.section_anchor},
            selected_state.previous_section_anchor};
}

RiverCheckpointRestore RiverGenerator::restore_checkpoint(RiverCheckpoint checkpoint) noexcept {
    initialize_respawn_state(state_, initial_state_, checkpoint,
                             state_.previous_section_anchor);
    return {RiverCheckpointSelection::Current, respawn_horizontal_position(state_)};
}

void RiverGenerator::reset() noexcept {
    state_ = initial_state_;
}

const TerrainStampPattern& RiverGenerator::stamp(std::uint8_t id) const {
    const auto* result = find_stamp(tables_.stamps, id);
    if (result == nullptr) {
        throw std::invalid_argument("River generator state refers to an unknown stamp");
    }
    return *result;
}

} // namespace river_raid
