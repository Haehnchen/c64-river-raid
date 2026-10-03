#include "game/contact_sampling.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using river_raid::ContactSamplingBuffer;
using river_raid::ContactSamplingSnapshot;
using river_raid::ContactSectionIndex;
using river_raid::ContactSectionSample;
using river_raid::ShotObjectIndex;
using river_raid::ShotObjectRole;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

ContactSectionIndex section(std::uint8_t value) {
    return {value};
}

void test_initial_state_and_rollover_reset() {
    ContactSamplingBuffer buffer;
    require(buffer.working_snapshot() == ContactSamplingSnapshot{},
            "Working contacts did not start empty");
    require(buffer.foreground_snapshot() == ContactSamplingSnapshot{},
            "Foreground contacts did not start empty");

    ContactSamplingSnapshot expected;
    for (std::uint8_t index = 0; index < river_raid::kShotContactSlotCount; ++index) {
        const auto sample = ContactSectionSample{
            static_cast<std::uint8_t>(0x81U + index),
            static_cast<std::uint8_t>(0x02U + index)};
        const auto primary = ShotObjectIndex{static_cast<std::uint8_t>(index + 1U)};
        const auto secondary = ShotObjectIndex{static_cast<std::uint8_t>(index + 7U)};
        buffer.replace_sample(section(index), sample);
        buffer.replace_object_mapping(section(index), ShotObjectRole::Primary, primary);
        buffer.replace_object_mapping(section(index), ShotObjectRole::Secondary, secondary);
        expected.sections[index] = {sample, primary, secondary};
    }

    buffer.rollover_for_foreground();
    require(buffer.foreground_snapshot() == expected,
            "Rollover did not preserve samples and both mappings");
    require(buffer.working_snapshot() == ContactSamplingSnapshot{},
            "Rollover did not reset every working sample and mapping");
}

void test_boundary_samples_replace_instead_of_accumulate() {
    ContactSamplingBuffer buffer;
    const auto index = section(4);
    buffer.replace_object_mapping(index, ShotObjectRole::Primary, ShotObjectIndex{9});
    buffer.replace_object_mapping(index, ShotObjectRole::Secondary, ShotObjectIndex{3});
    buffer.replace_sample(index, {0x82, 0x42});

    buffer.replace_object_participants(index, 0x41);
    auto state = buffer.working_snapshot().sections[4];
    require(state.sample == ContactSectionSample{0x41, 0x42},
            "Object-only boundary accumulated or changed the terrain sample");

    buffer.replace_terrain_participants(index, 0x04);
    state = buffer.working_snapshot().sections[4];
    require(state.sample == ContactSectionSample{0x41, 0x04},
            "Terrain-only boundary accumulated or changed the object sample");
    require(state.primary_object == ShotObjectIndex{9} &&
                state.secondary_object == ShotObjectIndex{3},
            "Boundary sampling changed the section mappings");

    buffer.replace_sample(index, {0x02, 0x80});
    state = buffer.working_snapshot().sections[4];
    require(state.sample == ContactSectionSample{0x02, 0x80},
            "Combined boundary did not replace both destination masks");
}

void test_foreground_snapshot_stays_stable_until_next_rollover() {
    ContactSamplingBuffer buffer;
    buffer.replace_sample(section(5), {0x82, 0x02});
    buffer.replace_object_mapping(
        section(5), ShotObjectRole::Primary, ShotObjectIndex{11});
    buffer.rollover_for_foreground();
    const auto published = buffer.foreground_snapshot();

    buffer.replace_sample(section(5), {0x42, 0x00});
    buffer.replace_object_mapping(
        section(5), ShotObjectRole::Primary, ShotObjectIndex{2});
    require(buffer.foreground_snapshot() == published,
            "Working-cycle writes changed the published foreground snapshot");

    buffer.rollover_for_foreground();
    require(buffer.foreground_snapshot().sections[5].sample ==
                ContactSectionSample{0x42, 0x00} &&
                buffer.foreground_snapshot().sections[5].primary_object ==
                    ShotObjectIndex{2},
            "The next rollover did not publish the replacement cycle");
}

void test_shot_adapter_keeps_section_and_role_correspondence() {
    ContactSamplingBuffer buffer;
    buffer.replace_sample(section(5), {0xC2, 0x00});
    buffer.replace_object_mapping(
        section(5), ShotObjectRole::Primary, ShotObjectIndex{10});
    buffer.replace_object_mapping(
        section(5), ShotObjectRole::Secondary, ShotObjectIndex{4});
    buffer.replace_sample(section(1), {0x40, 0x02});
    buffer.replace_object_mapping(
        section(1), ShotObjectRole::Secondary, ShotObjectIndex{7});
    buffer.replace_sample(section(0), {0x82, 0x00});
    buffer.rollover_for_foreground();

    const auto slots = river_raid::make_shot_contact_slots(
        buffer.foreground_snapshot());
    require(slots[5] == river_raid::ShotContactSlot{
                            false, true, true, true, {10}, {4}},
            "Upper section or its two object roles moved during adaptation");
    require(slots[1] == river_raid::ShotContactSlot{
                            true, false, false, true,
                            {static_cast<std::uint8_t>(river_raid::kShotContactObjectCount)},
                            {7}},
            "Terrain and object-role bits were not adapted independently");
    require(slots[0].object_contact && slots[0].primary_present &&
                slots[0].primary_object.value >= river_raid::kShotContactObjectCount,
            "Missing mapping became a valid projectile target");
}

void test_mapping_replacement_and_invalid_section_rejection() {
    ContactSamplingBuffer buffer;
    buffer.replace_object_mapping(
        section(2), ShotObjectRole::Primary, ShotObjectIndex{5});
    buffer.replace_object_mapping(
        section(2), ShotObjectRole::Primary, ShotObjectIndex{8});
    buffer.replace_object_mapping(section(2), ShotObjectRole::Secondary, std::nullopt);
    require(buffer.working_snapshot().sections[2].primary_object == ShotObjectIndex{8} &&
                !buffer.working_snapshot().sections[2].secondary_object,
            "Role mapping did not use replacement semantics");

    bool rejected = false;
    try {
        buffer.replace_sample(
            section(static_cast<std::uint8_t>(river_raid::kShotContactSlotCount)), {});
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    require(rejected, "Out-of-range contact section was accepted");

    const auto before_invalid_role = buffer.working_snapshot();
    rejected = false;
    try {
        buffer.replace_object_mapping(
            section(2), static_cast<ShotObjectRole>(0xff), ShotObjectIndex{1});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && buffer.working_snapshot() == before_invalid_role,
            "Invalid contact role was accepted or partially mutated state");
}

void test_mapping_shift_wraps_both_generations() {
    for (unsigned value = 0; value < 256; ++value) {
        ContactSamplingBuffer buffer;
        const auto raw = static_cast<std::uint8_t>(value);
        const auto mapping = raw == 255 ? std::nullopt
            : std::optional<ShotObjectIndex>{ShotObjectIndex{raw}};
        for (int generation = 0; generation < 2; ++generation) {
            for (std::uint8_t index = 0; index < 6; ++index) {
                buffer.replace_sample(section(index), {raw, index});
                buffer.replace_object_mapping(section(index), ShotObjectRole::Primary, mapping);
                buffer.replace_object_mapping(section(index), ShotObjectRole::Secondary, mapping);
            }
            if (generation == 0) buffer.rollover_for_foreground();
        }
        buffer.shift_object_mappings();
        const auto next = static_cast<std::uint8_t>(value + 1U);
        const auto expected = next == 255 ? std::nullopt
            : std::optional<ShotObjectIndex>{ShotObjectIndex{next}};
        for (const auto* snapshot : {&buffer.working_snapshot(), &buffer.foreground_snapshot()}) {
            for (std::uint8_t index = 0; index < 6; ++index) {
                const auto& actual = snapshot->sections[index];
                require(actual.primary_object == expected && actual.secondary_object == expected,
                        "History shift lost byte wrap or a mapping generation");
                require(actual.sample == ContactSectionSample{raw, index},
                        "History shift changed contact participants");
            }
        }
    }
    ContactSamplingBuffer buffer;
    buffer.replace_object_mapping(section(0), ShotObjectRole::Primary, ShotObjectIndex{255});
    buffer.shift_object_mappings();
    require(buffer.working_snapshot().sections[0].primary_object == ShotObjectIndex{0},
            "Explicit sentinel did not wrap to record zero");

    buffer = {};
    buffer.replace_object_mapping(section(2), ShotObjectRole::Primary, ShotObjectIndex{11});
    buffer.replace_object_mapping(section(2), ShotObjectRole::Secondary, ShotObjectIndex{4});
    buffer.rollover_for_foreground();
    buffer.replace_object_mapping(section(2), ShotObjectRole::Primary, ShotObjectIndex{9});
    buffer.replace_object_mapping(section(2), ShotObjectRole::Secondary, ShotObjectIndex{254});
    buffer.shift_object_mappings();
    require(buffer.foreground_snapshot().sections[2].primary_object == ShotObjectIndex{12} &&
                buffer.foreground_snapshot().sections[2].secondary_object == ShotObjectIndex{5} &&
                buffer.working_snapshot().sections[2].primary_object == ShotObjectIndex{10} &&
                !buffer.working_snapshot().sections[2].secondary_object,
            "Mapping shift mixed roles or generations");
}

void test_bridge_sprite_participation_survives_adapter() {
    for (unsigned mask = 0; mask < 256; ++mask) {
        ContactSamplingBuffer buffer;
        buffer.replace_sample(section(2), {static_cast<std::uint8_t>(mask), 0});
        buffer.rollover_for_foreground();
        const auto slots = river_raid::make_shot_contact_slots(buffer.foreground_snapshot());
        require(slots[2].bridge_sprite_present == ((mask & 0x20U) != 0),
                "Shot adapter lost independent bridge sprite participation");
    }
}

} // namespace

int main() {
    try {
        test_initial_state_and_rollover_reset();
        test_boundary_samples_replace_instead_of_accumulate();
        test_foreground_snapshot_stays_stable_until_next_rollover();
        test_shot_adapter_keeps_section_and_role_correspondence();
        test_mapping_replacement_and_invalid_section_rejection();
        test_mapping_shift_wraps_both_generations();
        test_bridge_sprite_participation_survives_adapter();
        std::cout << "Contact sampling rollover and boundary semantics match bounded rules\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
