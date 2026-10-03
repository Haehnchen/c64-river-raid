#include "app/world_setup.hpp"
#include "game/world_objects.hpp"
#include "game/contact_sampling.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

constexpr std::array<std::uint8_t, 16> delays{
    23, 5, 5, 5, 5, 5, 5, 5, 9, 9, 9, 9, 19, 7, 19, 23,
};
constexpr std::array<std::uint8_t, 8> choices{13, 8, 13, 8, 7, 13, 8, 12};
constexpr std::array<std::uint8_t, 16> clearance{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
};

river_raid::WorldObjectTables tables() {
    return {delays, choices, clearance};
}

void test_auxiliary_owner_selection() {
    using namespace river_raid;
    RiverGeneratorState river{};
    river.section = 13;
    river.section_interval = 5;
    river.course = 32;
    river.target_width_units = river.previous_target_width_units = 7;
    river.random.middle = 129;
    const RiverGuides guides{20, 100, 0, 0};
    for (unsigned high = 0; high < 256; ++high) {
        river.random.high = static_cast<std::uint8_t>(high);
        WorldObjectHistory history({}, tables());
        const auto row = history.advance_after_row(river, guides);
        const bool selected = (high & 0x70U) == 0;
        require(row.auxiliary_selected == selected && !row.history_shifted,
                "Auxiliary selection mask differs");
        require(history.state().records[11].animated_kind == (selected ? 10 : 8) &&
                history.state().records[11].base_kind == (selected ? 10 : 8) &&
                history.state().special_cursor == (selected ? 11 : 12),
                "Auxiliary selection did not retain its promoted owner");
    }
    river.random.high = 0;
    for (const auto interval : {13, 14}) {
        river.section_interval = static_cast<std::uint8_t>(interval);
        WorldObjectHistory history({}, tables());
        require(history.advance_after_row(river, guides).auxiliary_selected == (interval == 13),
                "Auxiliary interval boundary differs");
    }
    river.section_interval = 5;
    river.section = 12;
    WorldObjectHistory early({}, tables());
    require(!early.advance_after_row(river, guides).auxiliary_selected,
            "Auxiliary owner selected before section thirteen");
    river.section = 13;
    WorldObjectState occupied;
    occupied.special_cursor = 11;
    WorldObjectHistory reserved(occupied, tables());
    require(!reserved.advance_after_row(river, guides).auxiliary_selected,
            "Auxiliary owner replaced before leaving the history");
    occupied.active_begin = 11;
    occupied.records.back().vertical_position = 0xd3;
    WorldObjectHistory expired(occupied, tables());
    const auto replacement = expired.advance_after_row(river, guides);
    require(replacement.history_shifted && replacement.auxiliary_selected &&
            expired.state().special_cursor == 11,
            "Expired auxiliary owner was not replaced after the history shift");
    river.channel_mode = RiverChannelMode::SplitOpening;
    WorldObjectHistory blanked({}, tables());
    require(blanked.advance_after_row(river, {20, 100, 0, 0}).auxiliary_selected &&
            blanked.state().special_cursor == 11 && blanked.state().records[11].animated_kind == 0,
            "Split blanking lost an already selected auxiliary owner");
    river.channel_mode = RiverChannelMode::Single;
    river.phase = 16;
    river.random.middle = 1;
    WorldObjectHistory secondary({}, tables());
    require(secondary.advance_after_row(river, guides).auxiliary_selected &&
            secondary.state().records[11].animated_kind == 10,
            "Secondary row phase cannot select an auxiliary owner");
}

void test_expired_record_shifts_every_semantic_field() {
    river_raid::WorldObjectState seed{};
    for (std::size_t index = 0; index < seed.records.size(); ++index) {
        const auto value = static_cast<std::uint8_t>(index + 1);
        seed.records[index] = {value, static_cast<std::uint8_t>(value + 16),
                               static_cast<std::uint8_t>(value + 32),
                               static_cast<std::uint8_t>(index & 15U),
                               static_cast<std::uint8_t>((index + 1) & 15U),
                               static_cast<std::uint8_t>(value + 48),
                               static_cast<std::uint8_t>(value + 64),
                               static_cast<std::uint8_t>(value + 80)};
    }
    seed.records.back().vertical_position = 0xD3;
    seed.active_begin = 10;
    seed.generator_cursor = 10;
    seed.special_cursor = 11;
    seed.spawn_cooldown = 5;
    const auto shifted = seed.records[10];

    river_raid::WorldObjectHistory history(seed, tables());
    river_raid::RiverGeneratorState river{};
    river.phase = 1;
    history.advance_after_row(river, {});

    const auto& state = history.state();
    auto expected = shifted;
    ++expected.vertical_position;
    require(state.records[11] == expected, "Record shift lost a semantic object field");
    require(state.active_begin == 11 && state.generator_cursor == 11 &&
                state.special_cursor == 12,
            "Record shift advanced a cursor incorrectly");
    require(state.alternating_update && state.spawn_cooldown == 4,
            "Record shift lost parity or cooldown progression");

    const auto view = history.generator_view();
    require(view.cursor == 11 && view.entries[11].kind == expected.animated_kind &&
                view.entries[11].progress == expected.base_kind &&
                view.entries[11].horizontal_coordinate == expected.horizontal_coordinate,
            "Generator view did not preserve the selected record");
    require(!view.entries[11].extended_clearance,
            "Slot-indexed generator clearance changed unexpectedly");
}

void test_invalid_seed_and_choices_are_rejected() {
    river_raid::WorldObjectState bad_seed{};
    bad_seed.active_begin = 13;
    bool rejected = false;
    try {
        (void)river_raid::WorldObjectHistory(bad_seed, tables());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Out-of-range world object cursor was accepted");

    auto bad_choices = choices;
    bad_choices[0] = 16;
    rejected = false;
    try {
        (void)river_raid::WorldObjectHistory({}, {delays, bad_choices, clearance});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Unknown world object choice was accepted");
}

void test_natural_options_world_drives_2048_native_rows() {
    auto world = river_raid::make_river_world();
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t step = 1; step <= 2048; ++step) {
        const auto row = world.advance();
        for (const auto value : row) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }

        if (step == 585) {
            require(world.river_state().course == 29,
                    "Object coupling changed the first long-run course decision");
        }
        if (step == 1184) {
            require(world.object_state().split_collision_wrap,
                    "Split-channel side did not follow the object decision");
        }
        if (step == 1218) {
            require(world.river_state().course == 32,
                    "Split-channel object boundary changed the native course");
        }
        if (step == 2016) {
            const auto& objects = world.object_state();
            require(objects.generator_cursor == 5 && objects.records[5].base_kind == 8 &&
                        objects.spawn_cooldown == 12,
                    "Late interval transition selected the wrong object kind");
        }
    }

    const auto& final = world.river_state();
    require(hash == 0x64865D8620C0495FULL,
            "Natural options world differs from the verified 2048-row excerpt");
    require(final.phase == 0 && final.scroll_buffer == 4 && final.section == 2 &&
                final.course == 34 && final.channel_span == 20 && final.bank_inset == 54 &&
                final.random == river_raid::RiverRandomState{0x5C, 0x4E, 0xC4},
            "Natural options world ended in the wrong generator state");

    world.reset();
    require(world.generated_rows() == 0 && world.object_state().active_begin == 11,
            "World reset did not restore the injected object seed");
}

void test_world_remaps_contacts_only_on_history_shift() {
    auto world = river_raid::make_river_world();
    auto baseline = river_raid::make_river_world();
    river_raid::ContactSamplingBuffer contacts;
    river_raid::ContactSamplingBuffer expected;
    unsigned shifts = 0;
    for (unsigned row = 0; row < 2048; ++row) {
        const auto before = world.object_state();
        const bool shift = static_cast<std::uint8_t>(
            before.records.back().vertical_position + 1U) >= 0xD4 &&
            before.active_begin < river_raid::kWorldObjectCapacity;
        if (shift) {
            expected.shift_object_mappings();
            ++shifts;
        }
        require(world.advance(contacts) == baseline.advance(),
                "Contact remap changed the generated row");
        require(world.object_state() == baseline.object_state(),
                "Contact remap changed object state");
        require(contacts.working_snapshot() == expected.working_snapshot() &&
                    contacts.foreground_snapshot() == expected.foreground_snapshot(),
                "World remapped contacts without a shift or missed a shift");
    }
    require(shifts > 1 && shifts < 2048, "World remap test missed shift or non-shift paths");
}

} // namespace

int main() {
    try {
        test_auxiliary_owner_selection();
        test_expired_record_shifts_every_semantic_field();
        test_invalid_seed_and_choices_are_rejected();
        test_natural_options_world_drives_2048_native_rows();
        test_world_remaps_contacts_only_on_history_shift();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
