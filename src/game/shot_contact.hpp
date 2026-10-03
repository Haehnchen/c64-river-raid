#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace river_raid {

inline constexpr std::size_t kShotContactSlotCount = 6;
inline constexpr std::size_t kShotContactObjectCount = 12;

struct ShotContactSlotIndex {
    std::uint8_t value{};

    friend bool operator==(const ShotContactSlotIndex&, const ShotContactSlotIndex&) = default;
};

struct ShotObjectIndex {
    std::uint8_t value{};

    friend bool operator==(const ShotObjectIndex&, const ShotObjectIndex&) = default;
};

struct ShotContactSlot {
    bool terrain_contact{};
    bool object_contact{};
    // Collision-mask roles, not scheduled sprite occupancy.
    bool primary_present{};
    bool secondary_present{};
    ShotObjectIndex primary_object{};
    ShotObjectIndex secondary_object{};
    bool bridge_sprite_present{};

    friend bool operator==(const ShotContactSlot&, const ShotContactSlot&) = default;
};

struct ShotContactSnapshot {
    std::array<ShotContactSlot, kShotContactSlotCount> slots{};
    std::array<std::uint8_t, kShotContactObjectCount> object_kinds{};
    bool suppress_score_kind_write{};

    friend bool operator==(const ShotContactSnapshot&, const ShotContactSnapshot&) = default;
};

enum class ShotObjectRole { Primary, Secondary };

struct ShotObjectSelection {
    ShotObjectIndex index;
    ShotObjectRole role{ShotObjectRole::Primary};

    friend bool operator==(const ShotObjectSelection&, const ShotObjectSelection&) = default;
};

struct ShotOrdinaryEffect {
    std::uint8_t replacement_animated_kind{};
    std::uint8_t animation_count{};
    std::uint8_t effect_countdown{};
    std::optional<std::uint8_t> score_kind_write;

    friend bool operator==(const ShotOrdinaryEffect&, const ShotOrdinaryEffect&) = default;
};

enum class ShotContactOutcome { None, Terrain, Ordinary, Bridge };

struct ShotContactResult {
    ShotContactOutcome outcome{ShotContactOutcome::None};
    std::optional<ShotContactSlotIndex> slot;
    std::optional<ShotObjectSelection> object;
    std::optional<ShotOrdinaryEffect> ordinary_effect;
    bool clear_projectile{};

    friend bool operator==(const ShotContactResult&, const ShotContactResult&) = default;
};

// Selects at most one contact from an already sampled collision snapshot.
[[nodiscard]] ShotContactResult select_shot_contact(
    const ShotContactSnapshot& snapshot) noexcept;

} // namespace river_raid
