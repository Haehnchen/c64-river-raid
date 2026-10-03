#include "game/gap_contact.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;

constexpr std::array<std::uint8_t, 16> delays{};
constexpr std::array<std::uint8_t, 8> choices{};
constexpr std::array<std::uint8_t, 16> clearance{};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

WorldObjectState object_state() {
    WorldObjectState result{};
    result.active_begin = 0;
    for (auto& record : result.records) record.base_kind = 1;
    return result;
}

GapContactSnapshot target_snapshot(std::size_t slot, std::size_t record,
                                   WorldObjectHitSlot hit_slot = WorldObjectHitSlot::Primary) {
    GapContactSnapshot result{};
    auto& sample = result.slots[slot];
    sample.gap_present = true;
    if (hit_slot == WorldObjectHitSlot::Primary) {
        sample.primary_present = true;
        sample.primary_object = ShotObjectIndex{static_cast<std::uint8_t>(record)};
    } else {
        sample.alternate_present = true;
        sample.alternate_object = ShotObjectIndex{static_cast<std::uint8_t>(record)};
    }
    return result;
}

void test_counter_gate() {
    auto objects = object_state();
    objects.records[0].animated_kind = 7;
    const auto snapshot = target_snapshot(0, 0);
    for (unsigned counter = 0; counter < 256; ++counter) {
        const auto selection = select_gap_contact(static_cast<std::uint8_t>(counter),
                                                  snapshot, objects);
        const bool admitted = counter >= 17 && counter < 128;
        require(static_cast<bool>(selection.object) == admitted,
                "Gap counter admitted outside the source range");
        if (admitted) {
            require(!selection.fatal_player_contact &&
                        *selection.object == GapObjectTarget{0, WorldObjectHitSlot::Primary},
                    "Admitted gap counter selected the wrong target");
        }
    }
}

void test_scan_priority_and_continuation() {
    auto objects = object_state();
    objects.records[0].animated_kind = 7;
    objects.records[1].animated_kind = 8;
    objects.records[2].animated_kind = 9;
    objects.records[3].animated_kind = 6;
    objects.records[4].animated_kind = 10;
    objects.records[5].animated_kind = 11;

    auto descending = target_snapshot(5, 5);
    descending.slots[0].gap_present = true;
    descending.slots[0].player_present = true;
    auto selected = select_gap_contact(17, descending, objects);
    require(selected.object && *selected.object == GapObjectTarget{5, WorldObjectHitSlot::Primary},
            "Gap scan did not prioritize the highest published slot");

    auto player = target_snapshot(2, 2);
    player.slots[2].player_present = true;
    selected = select_gap_contact(17, player, objects);
    require(selected.fatal_player_contact && !selected.object,
            "Player participation did not win the gap transaction");

    auto roles = target_snapshot(3, 0);
    roles.slots[3].alternate_present = true;
    roles.slots[3].alternate_object = ShotObjectIndex{1};
    selected = select_gap_contact(17, roles, objects);
    require(selected.object && *selected.object == GapObjectTarget{0, WorldObjectHitSlot::Primary},
            "Primary role did not precede alternate role");

    roles.slots[3].primary_present = false;
    roles.slots[3].primary_object.reset();
    selected = select_gap_contact(17, roles, objects);
    require(selected.object && *selected.object == GapObjectTarget{1, WorldObjectHitSlot::Alternate},
            "Alternate role mapping was not selected when primary was absent");

    auto empty_role = target_snapshot(5, 0);
    empty_role.slots[5].primary_present = false;
    empty_role.slots[5].primary_object.reset();
    empty_role.slots[0] = target_snapshot(0, 2).slots[0];
    selected = select_gap_contact(17, empty_role, objects);
    require(selected.object && *selected.object == GapObjectTarget{2, WorldObjectHitSlot::Primary},
            "A gap sample without a role did not continue scanning");

    auto ignored = target_snapshot(5, 3);
    ignored.slots[0] = target_snapshot(0, 2).slots[0];
    selected = select_gap_contact(17, ignored, objects);
    require(selected.object && *selected.object == GapObjectTarget{2, WorldObjectHitSlot::Primary},
            "A below-threshold kind did not continue scanning");

    auto invalid = target_snapshot(5, kWorldObjectCapacity);
    invalid.slots[0] = target_snapshot(0, 2).slots[0];
    selected = select_gap_contact(17, invalid, objects);
    require(!selected.fatal_player_contact && !selected.object,
            "Invalid gap mapping did not terminate the scan");
}

void test_late_kind_lookup() {
    auto objects = object_state();
    auto snapshot = target_snapshot(0, 0);

    objects.records[0].animated_kind = 6;
    require(!select_gap_contact(17, snapshot, objects).object,
            "Gap selector accepted a non-damaging current kind");
    objects.records[0].animated_kind = 7;
    auto selected = select_gap_contact(17, snapshot, objects);
    require(selected.object && selected.object->record_index == 0,
            "Gap selector did not read the late current kind");
    objects.records[0].animated_kind = 6;
    require(!select_gap_contact(17, snapshot, objects).object,
            "Gap selector retained a stale kind from an earlier selection");
}

void test_silent_gap_mutation() {
    for (unsigned kind = 0; kind < 16; ++kind) {
        for (const auto slot : {WorldObjectHitSlot::Primary, WorldObjectHitSlot::Alternate}) {
            WorldObjectState seed = object_state();
            seed.records[4] = {80, 86, 0x48, static_cast<std::uint8_t>(kind),
                               static_cast<std::uint8_t>(kind), 30, 130, 99};
            WorldObjectHistory history(seed, {delays, choices, clearance});
            const auto effect = history.apply_gap_contact_hit(4, slot);
            if (kind < 7) {
                require(!effect && history.state() == seed,
                        "Non-damaging gap kind changed world state");
                continue;
            }

            auto expected = seed;
            expected.records[4].animated_kind =
                slot == WorldObjectHitSlot::Primary ? 2 : 4;
            expected.records[4].animation_count = 6;
            require(effect && history.state() == expected,
                    "Gap contact changed more than the contacted object");
            require(*effect == GapContactHitEffect{4, static_cast<std::uint8_t>(kind)},
                    "Gap mutation returned the wrong silent effect");
            require(!history.apply_gap_contact_hit(4, slot),
                    "Gap explosion was hit again");
        }
    }
}

} // namespace

int main() {
    try {
        test_counter_gate();
        test_scan_priority_and_continuation();
        test_late_kind_lookup();
        test_silent_gap_mutation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
