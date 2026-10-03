#include "game/gap_contact.hpp"

namespace river_raid {

GapContactSelection select_gap_contact(std::uint8_t live_counter,
                                       const GapContactSnapshot& snapshot,
                                       const WorldObjectState& objects) noexcept {
    if (live_counter < 17 || (live_counter & 0x80U) != 0) return {};

    for (std::size_t slot_index = snapshot.slots.size(); slot_index-- > 0;) {
        const auto& sample = snapshot.slots[slot_index];
        if (!sample.gap_present) continue;
        if (sample.player_present) return {true, std::nullopt};

        const std::optional<ShotObjectIndex>* mapped_object = nullptr;
        auto hit_slot = WorldObjectHitSlot::Primary;
        if (sample.primary_present) {
            mapped_object = &sample.primary_object;
        } else if (sample.alternate_present) {
            mapped_object = &sample.alternate_object;
            hit_slot = WorldObjectHitSlot::Alternate;
        } else {
            continue;
        }

        if (!*mapped_object || (*mapped_object)->value >= objects.records.size()) return {};
        const auto record_index = static_cast<std::size_t>((*mapped_object)->value);
        if (objects.records[record_index].animated_kind < 7) continue;
        return {false, GapObjectTarget{record_index, hit_slot}};
    }
    return {};
}

} // namespace river_raid
