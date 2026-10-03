#pragma once

#include "game/shot_contact.hpp"
#include "game/world_objects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace river_raid {

struct GapContactSlotSnapshot {
    bool gap_present{};
    bool player_present{};
    bool primary_present{};
    bool alternate_present{};
    std::optional<ShotObjectIndex> primary_object;
    std::optional<ShotObjectIndex> alternate_object;

    friend bool operator==(const GapContactSlotSnapshot&,
                           const GapContactSlotSnapshot&) = default;
};

struct GapContactSnapshot {
    std::array<GapContactSlotSnapshot, kShotContactSlotCount> slots{};

    friend bool operator==(const GapContactSnapshot&, const GapContactSnapshot&) = default;
};

struct GapObjectTarget {
    std::size_t record_index{};
    WorldObjectHitSlot slot{WorldObjectHitSlot::Primary};

    friend bool operator==(const GapObjectTarget&, const GapObjectTarget&) = default;
};

struct GapContactSelection {
    bool fatal_player_contact{};
    std::optional<GapObjectTarget> object;

    friend bool operator==(const GapContactSelection&, const GapContactSelection&) = default;
};

[[nodiscard]] GapContactSelection select_gap_contact(
    std::uint8_t live_counter, const GapContactSnapshot& snapshot,
    const WorldObjectState& objects) noexcept;

} // namespace river_raid
