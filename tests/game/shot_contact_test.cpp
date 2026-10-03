#include "game/shot_contact.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace river_raid;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

ShotContactSnapshot ordinary_snapshot(std::size_t slot_index, std::uint8_t object_index,
                                      std::uint8_t kind, ShotObjectRole role) {
    ShotContactSnapshot snapshot;
    auto& slot = snapshot.slots[slot_index];
    slot.object_contact = true;
    if (role == ShotObjectRole::Primary) {
        slot.primary_present = true;
        slot.primary_object = {object_index};
    } else {
        slot.secondary_present = true;
        slot.secondary_object = {object_index};
    }
    snapshot.object_kinds[object_index] = kind;
    return snapshot;
}

void test_empty_and_terrain_priority() {
    ShotContactSnapshot snapshot;
    require(select_shot_contact(snapshot) == ShotContactResult{},
            "Empty snapshot selected a contact");

    snapshot.slots[1].terrain_contact = true;
    snapshot.slots[5].terrain_contact = true;
    auto result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::Terrain &&
                result.slot == ShotContactSlotIndex{5} && !result.object &&
                !result.ordinary_effect && result.clear_projectile,
            "Terrain did not win in reverse slot order");

    snapshot.slots[5].terrain_contact = false;
    snapshot.slots[5].object_contact = true;
    snapshot.slots[5].primary_present = true;
    snapshot.slots[5].primary_object = {0};
    snapshot.object_kinds[0] = 9;
    result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::Terrain &&
                result.slot == ShotContactSlotIndex{1},
            "Lower terrain contact did not precede a higher object contact");
}

void test_object_order_roles_and_effect() {
    auto snapshot = ordinary_snapshot(1, 3, 9, ShotObjectRole::Secondary);
    auto higher = ordinary_snapshot(4, 7, 12, ShotObjectRole::Primary);
    snapshot.slots[4] = higher.slots[4];
    snapshot.object_kinds[7] = 12;

    auto result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::Ordinary &&
                result.slot == ShotContactSlotIndex{4} &&
                result.object == ShotObjectSelection{{7}, ShotObjectRole::Primary} &&
                result.ordinary_effect ==
                    ShotOrdinaryEffect{2, 6, 0x1F, std::uint8_t{12}} &&
                result.clear_projectile,
            "Highest ordinary primary contact produced the wrong effect");

    snapshot.slots[4].primary_present = false;
    snapshot.slots[4].secondary_present = true;
    snapshot.slots[4].secondary_object = {7};
    result = select_shot_contact(snapshot);
    require(result.object == ShotObjectSelection{{7}, ShotObjectRole::Secondary} &&
                result.ordinary_effect ==
                    ShotOrdinaryEffect{4, 6, 0x1F, std::uint8_t{12}},
            "Secondary contact did not select its distinct replacement kind");

    snapshot.suppress_score_kind_write = true;
    result = select_shot_contact(snapshot);
    require(result.ordinary_effect && !result.ordinary_effect->score_kind_write,
            "Suppressed score kind was still written");
}

void test_role_and_index_corner_cases() {
    ShotContactSnapshot snapshot;
    auto& highest = snapshot.slots[5];
    highest.primary_present = true;
    highest.primary_object = {2};
    highest.secondary_object = {3};
    snapshot.object_kinds[2] = 9;
    snapshot.object_kinds[3] = 10;

    auto result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::None,
            "Object role without a projectile contact was accepted");

    highest.object_contact = true;
    highest.primary_present = false;
    result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::None,
            "Contact without an object role reused a stale index");

    highest.primary_present = true;
    highest.secondary_present = true;
    result = select_shot_contact(snapshot);
    require(result.object == ShotObjectSelection{{2}, ShotObjectRole::Primary} &&
                result.ordinary_effect &&
                result.ordinary_effect->replacement_animated_kind == 2,
            "Primary role did not win when both roles were present");

    highest.primary_object = {12};
    auto& lower = snapshot.slots[4];
    lower.object_contact = true;
    lower.secondary_present = true;
    lower.secondary_object = {3};
    result = select_shot_contact(snapshot);
    require(result.slot == ShotContactSlotIndex{4} &&
                result.object == ShotObjectSelection{{3}, ShotObjectRole::Secondary},
            "Invalid primary index fell back within its slot or stopped the scan");

    highest.primary_object = {2};
    snapshot.object_kinds[2] = 6;
    result = select_shot_contact(snapshot);
    require(result.slot == ShotContactSlotIndex{4},
            "Non-destructible primary object fell back within its slot or stopped the scan");
}

void test_bridge_selection() {
    auto snapshot = ordinary_snapshot(3, 11, 14, ShotObjectRole::Primary);
    const auto result = select_shot_contact(snapshot);
    require(result.outcome == ShotContactOutcome::Bridge &&
                result.slot == ShotContactSlotIndex{3} &&
                result.object == ShotObjectSelection{{11}, ShotObjectRole::Primary} &&
                !result.ordinary_effect && result.clear_projectile,
            "Bridge contact was treated as ordinary destruction");
}

void test_all_kinds_and_indices() {
    for (std::uint16_t kind = 0; kind <= 0xFF; ++kind) {
        for (std::uint8_t index = 0; index < kShotContactObjectCount; ++index) {
            auto snapshot = ordinary_snapshot(0, index, static_cast<std::uint8_t>(kind),
                                              ShotObjectRole::Primary);
            const auto result = select_shot_contact(snapshot);
            const auto expected = kind < 7
                                      ? ShotContactOutcome::None
                                      : kind == 14 ? ShotContactOutcome::Bridge
                                                   : ShotContactOutcome::Ordinary;
            require(result.outcome == expected,
                    "Object kind boundary differs at kind " + std::to_string(kind));
        }
    }

    for (std::uint16_t index = kShotContactObjectCount; index <= 0xFF; ++index) {
        ShotContactSnapshot snapshot;
        auto& slot = snapshot.slots[0];
        slot.object_contact = true;
        slot.primary_present = true;
        slot.primary_object = {static_cast<std::uint8_t>(index)};
        require(select_shot_contact(snapshot).outcome == ShotContactOutcome::None,
                "Out-of-range object index was accepted");
    }
}

} // namespace

int main() {
    try {
        test_empty_and_terrain_priority();
        test_object_order_roles_and_effect();
        test_role_and_index_corner_cases();
        test_bridge_selection();
        test_all_kinds_and_indices();
        std::cout << "Shot contact selection and ordinary effects match bounded rules\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
