#pragma once

#include "game/shot_contact.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace river_raid {

inline constexpr std::uint8_t kPlayerMainParticipant = 0x01;
inline constexpr std::uint8_t kPlayerDetailParticipant = 0x04;
inline constexpr std::uint8_t kAuxiliaryParticipant = 0x08;
inline constexpr std::uint8_t kProjectileContactParticipant = 0x02;
inline constexpr std::uint8_t kBridgeSpriteContactParticipant = 0x20;
inline constexpr std::uint8_t kPrimaryObjectContactParticipant = 0x80;
inline constexpr std::uint8_t kSecondaryObjectContactParticipant = 0x40;

struct ContactSectionIndex {
    std::uint8_t value{};

    friend bool operator==(const ContactSectionIndex&, const ContactSectionIndex&) = default;
};

struct ContactSectionSample {
    std::uint8_t object_participants{};
    std::uint8_t terrain_participants{};

    friend bool operator==(const ContactSectionSample&, const ContactSectionSample&) = default;
};

struct ContactSectionState {
    ContactSectionSample sample;
    std::optional<ShotObjectIndex> primary_object;
    std::optional<ShotObjectIndex> secondary_object;

    friend bool operator==(const ContactSectionState&, const ContactSectionState&) = default;
};

struct ContactSamplingSnapshot {
    std::array<ContactSectionState, kShotContactSlotCount> sections{};

    friend bool operator==(const ContactSamplingSnapshot&, const ContactSamplingSnapshot&) = default;
};

class ContactSamplingBuffer {
public:
    // Publishes the completed cycle for foreground consumption and clears every
    // sample and mapping in the new working cycle.
    void rollover_for_foreground() noexcept;

    // Keeps both generations aligned when object history shifts by one record.
    void shift_object_mappings() noexcept;

    // Boundary reads replace their destination; latch accumulation belongs to
    // the producer before this call.
    void replace_sample(ContactSectionIndex section, ContactSectionSample sample);
    void replace_object_participants(ContactSectionIndex section, std::uint8_t participants);
    void replace_terrain_participants(ContactSectionIndex section, std::uint8_t participants);

    // Scheduling a role replaces only that role's mapping in the working cycle.
    void replace_object_mapping(ContactSectionIndex section, ShotObjectRole role,
                                std::optional<ShotObjectIndex> object);

    [[nodiscard]] const ContactSamplingSnapshot& working_snapshot() const noexcept {
        return working_;
    }

    [[nodiscard]] const ContactSamplingSnapshot& foreground_snapshot() const noexcept {
        return foreground_;
    }

private:
    ContactSamplingSnapshot working_;
    ContactSamplingSnapshot foreground_;
};

// Adapts the published foreground snapshot to the projectile-hit selector.
[[nodiscard]] std::array<ShotContactSlot, kShotContactSlotCount> make_shot_contact_slots(
    const ContactSamplingSnapshot& snapshot) noexcept;

} // namespace river_raid
