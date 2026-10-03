#include "game/shot_contact.hpp"

namespace river_raid {
namespace {

constexpr std::uint8_t minimum_destructible_kind = 7;
constexpr std::uint8_t bridge_kind = 14;
constexpr std::uint8_t primary_replacement_kind = 2;
constexpr std::uint8_t secondary_replacement_kind = 4;
constexpr std::uint8_t destruction_animation_count = 6;
constexpr std::uint8_t destruction_effect_countdown = 0x1F;

ShotContactSlotIndex slot_index(std::size_t index) noexcept {
    return {static_cast<std::uint8_t>(index)};
}

} // namespace

ShotContactResult select_shot_contact(const ShotContactSnapshot& snapshot) noexcept {
    for (std::size_t index = snapshot.slots.size(); index-- > 0;) {
        if (snapshot.slots[index].terrain_contact) {
            return {ShotContactOutcome::Terrain, slot_index(index), std::nullopt,
                    std::nullopt, true};
        }
    }

    for (std::size_t index = snapshot.slots.size(); index-- > 0;) {
        const auto& slot = snapshot.slots[index];
        if (!slot.object_contact) continue;

        ShotObjectSelection selection;
        std::uint8_t replacement_kind{};
        if (slot.primary_present) {
            selection = {slot.primary_object, ShotObjectRole::Primary};
            replacement_kind = primary_replacement_kind;
        } else if (slot.secondary_present) {
            selection = {slot.secondary_object, ShotObjectRole::Secondary};
            replacement_kind = secondary_replacement_kind;
        } else {
            continue;
        }

        if (selection.index.value >= snapshot.object_kinds.size()) continue;

        const auto kind = snapshot.object_kinds[selection.index.value];
        if (kind < minimum_destructible_kind) continue;

        if (kind == bridge_kind) {
            return {ShotContactOutcome::Bridge, slot_index(index), selection,
                    std::nullopt, true};
        }

        const auto score_kind = snapshot.suppress_score_kind_write
                                    ? std::optional<std::uint8_t>{}
                                    : std::optional<std::uint8_t>{kind};
        return {ShotContactOutcome::Ordinary,
                slot_index(index),
                selection,
                ShotOrdinaryEffect{replacement_kind, destruction_animation_count,
                                   destruction_effect_countdown, score_kind},
                true};
    }

    return {};
}

} // namespace river_raid
