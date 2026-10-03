#pragma once

#include "game/contact_sampling.hpp"
#include "game/player_steering.hpp"
#include "game/river_world.hpp"
#include "game/shot_contact.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace river_raid {

enum class PlayerObjectContactEffect : std::uint8_t {
    Ignore,
    Crash,
    Bridge,
    Refuel,
};

// Bridges require destruction as well as damage; depots replenish fuel.
[[nodiscard]] PlayerObjectContactEffect player_object_contact_effect(
    std::uint8_t animated_kind) noexcept;

struct PlayerBridgeContact {
    std::size_t record_index{};
    WorldObjectHitSlot slot{WorldObjectHitSlot::Primary};
    bool joint_contact{};
};

struct PlayerEnemyContact {
    std::size_t record_index{};
    WorldObjectHitSlot slot{WorldObjectHitSlot::Primary};
    std::uint8_t animated_kind{};
};

struct PlayerContactResult {
    bool terrain_contact{};
    std::optional<PlayerEnemyContact> object_contact;
    bool refuel_contact{};
    std::optional<PlayerBridgeContact> bridge;

    [[nodiscard]] bool contacted() const noexcept {
        return terrain_contact || object_contact || bridge.has_value();
    }
};

struct PlayerContactSlotSnapshot {
    bool object_contact{};
    bool primary_present{};
    bool alternate_present{};
    std::optional<ShotObjectIndex> primary_object;
    std::optional<ShotObjectIndex> alternate_object;
    bool bridge_sprite_present{};
    bool auxiliary_projectile_present{};

    friend bool operator==(const PlayerContactSlotSnapshot&,
                           const PlayerContactSlotSnapshot&) = default;
};

struct PlayerContactSnapshot {
    std::array<PlayerContactSlotSnapshot, kShotContactSlotCount> slots{};
    std::array<bool, 2> terrain_contacts{};
    // Native playability rule, deliberately outside the source selector.
    bool player_fully_offscreen{};

    friend bool operator==(const PlayerContactSnapshot&,
                           const PlayerContactSnapshot&) = default;
};

// Decodes a published participant snapshot; offscreen policy is supplied by
// the scene owner, separately from sampled terrain and object contacts.
[[nodiscard]] PlayerContactSnapshot make_player_contact_snapshot(
    const ContactSamplingSnapshot& snapshot) noexcept;

// Captures raw geometric participation and record mappings. Object kinds are
// intentionally read later by the foreground selector after world motion.
[[nodiscard]] PlayerContactSnapshot sample_player_contact_snapshot(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose);

// Mirrors the source aircraft scan: lower slot 1 then 0, primary before
// alternate, with one semantic result selected from the current object state.
[[nodiscard]] PlayerContactResult select_player_contact(
    const PlayerContactSnapshot& snapshot, const WorldObjectState& objects) noexcept;

// Approximate one visible FlightPreview snapshot using the shipped occupancy
// masks. Raster-latch ownership and delayed contact publication are not modeled.
[[nodiscard]] PlayerContactResult sample_player_contact(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose);

} // namespace river_raid
