#include "game/contact_sampling.hpp"

#include <cstddef>
#include <stdexcept>

namespace river_raid {
namespace {

std::size_t checked_index(ContactSectionIndex section) {
    if (section.value >= kShotContactSlotCount) {
        throw std::out_of_range("Contact section index out of range");
    }
    return section.value;
}

constexpr bool contains(std::uint8_t participants, std::uint8_t participant) noexcept {
    return (participants & participant) != 0;
}

constexpr ShotObjectIndex object_or_invalid(
    const std::optional<ShotObjectIndex>& object) noexcept {
    return object.value_or(ShotObjectIndex{
        static_cast<std::uint8_t>(kShotContactObjectCount)});
}

} // namespace

void ContactSamplingBuffer::rollover_for_foreground() noexcept {
    foreground_ = working_;
    working_ = {};
}

void ContactSamplingBuffer::shift_object_mappings() noexcept {
    const auto shift = [](std::optional<ShotObjectIndex>& object) {
        const auto next = static_cast<std::uint8_t>(
            object.value_or(ShotObjectIndex{255}).value + 1U);
        object = next == 255 ? std::nullopt
                            : std::optional<ShotObjectIndex>{ShotObjectIndex{next}};
    };
    for (auto* snapshot : {&working_, &foreground_}) {
        for (auto& section : snapshot->sections) {
            shift(section.primary_object);
            shift(section.secondary_object);
        }
    }
}

void ContactSamplingBuffer::replace_sample(ContactSectionIndex section,
                                           ContactSectionSample sample) {
    working_.sections[checked_index(section)].sample = sample;
}

void ContactSamplingBuffer::replace_object_participants(
    ContactSectionIndex section, std::uint8_t participants) {
    working_.sections[checked_index(section)].sample.object_participants = participants;
}

void ContactSamplingBuffer::replace_terrain_participants(
    ContactSectionIndex section, std::uint8_t participants) {
    working_.sections[checked_index(section)].sample.terrain_participants = participants;
}

void ContactSamplingBuffer::replace_object_mapping(
    ContactSectionIndex section, ShotObjectRole role,
    std::optional<ShotObjectIndex> object) {
    auto& state = working_.sections[checked_index(section)];
    switch (role) {
    case ShotObjectRole::Primary:
        state.primary_object = object;
        break;
    case ShotObjectRole::Secondary:
        state.secondary_object = object;
        break;
    default:
        throw std::invalid_argument("Invalid contact object role");
    }
}

std::array<ShotContactSlot, kShotContactSlotCount> make_shot_contact_slots(
    const ContactSamplingSnapshot& snapshot) noexcept {
    std::array<ShotContactSlot, kShotContactSlotCount> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto& source = snapshot.sections[index];
        auto& destination = result[index];
        destination.terrain_contact = contains(
            source.sample.terrain_participants, kProjectileContactParticipant);
        destination.object_contact = contains(
            source.sample.object_participants, kProjectileContactParticipant);
        destination.primary_present = contains(
            source.sample.object_participants, kPrimaryObjectContactParticipant);
        destination.secondary_present = contains(
            source.sample.object_participants, kSecondaryObjectContactParticipant);
        destination.primary_object = object_or_invalid(source.primary_object);
        destination.secondary_object = object_or_invalid(source.secondary_object);
        destination.bridge_sprite_present = contains(
            source.sample.object_participants, kBridgeSpriteContactParticipant);
    }
    return result;
}

} // namespace river_raid
