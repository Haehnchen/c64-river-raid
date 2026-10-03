#include "app/player_contact.hpp"

#include "app/contact_scene.hpp"

namespace river_raid {
namespace {

[[nodiscard]] PlayerContactResult terrain_result(
    const PlayerContactSnapshot& snapshot) noexcept {
    PlayerContactResult result;
    result.terrain_contact = snapshot.terrain_contacts[1] ||
                             snapshot.terrain_contacts[0];
    return result;
}

} // namespace

PlayerContactSnapshot make_player_contact_snapshot(
    const ContactSamplingSnapshot& snapshot) noexcept {
    PlayerContactSnapshot result;
    for (std::size_t index = 0; index < snapshot.sections.size(); ++index) {
        const auto& section = snapshot.sections[index];
        const auto participants = section.sample.object_participants;
        auto& slot = result.slots[index];
        slot.primary_object = section.primary_object;
        slot.alternate_object = section.secondary_object;
        slot.primary_present = (participants & kPrimaryObjectContactParticipant) != 0;
        slot.alternate_present = (participants & kSecondaryObjectContactParticipant) != 0;
        slot.object_contact = (participants & kPlayerMainParticipant) != 0 &&
                              (slot.primary_present || slot.alternate_present);
        slot.bridge_sprite_present = (participants & kBridgeSpriteContactParticipant) != 0;
        slot.auxiliary_projectile_present = (participants & kAuxiliaryParticipant) != 0;
        if (index < result.terrain_contacts.size()) {
            result.terrain_contacts[index] =
                (section.sample.terrain_participants & kPlayerDetailParticipant) != 0;
        }
    }
    return result;
}

PlayerObjectContactEffect player_object_contact_effect(
    std::uint8_t animated_kind) noexcept {
    if (animated_kind >= 7 && animated_kind < 14) {
        return PlayerObjectContactEffect::Crash;
    }
    if (animated_kind == 14) return PlayerObjectContactEffect::Bridge;
    if (animated_kind == 15) return PlayerObjectContactEffect::Refuel;
    return PlayerObjectContactEffect::Ignore;
}

PlayerContactSnapshot sample_player_contact_snapshot(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose) {
    const auto bridge = world.bridge_presentation_state();
    return detail::sample_contact_scene({
        world.object_state(), world.river_state().phase, &world.viewport(), &bridge,
        &world.auxiliary_projectile_state(), player, pose, LifeCycleImage::Straight,
        nullptr}).player;
}

PlayerContactResult select_player_contact(
    const PlayerContactSnapshot& snapshot, const WorldObjectState& objects) noexcept {
    for (std::size_t slot_index = snapshot.terrain_contacts.size(); slot_index-- > 0;) {
        const auto& slot = snapshot.slots[slot_index];
        if (slot.auxiliary_projectile_present) return terrain_result(snapshot);
        if (!slot.object_contact) continue;

        std::optional<ShotObjectIndex> selected;
        auto hit_slot = WorldObjectHitSlot::Primary;
        if (slot.primary_present) {
            selected = slot.primary_object;
        } else if (slot.alternate_present) {
            selected = slot.alternate_object;
            hit_slot = WorldObjectHitSlot::Alternate;
        } else {
            continue;
        }
        if (!selected || selected->value >= objects.records.size()) continue;

        const auto record_index = static_cast<std::size_t>(selected->value);
        switch (player_object_contact_effect(
            objects.records[record_index].animated_kind)) {
        case PlayerObjectContactEffect::Crash: {
            PlayerContactResult result;
            result.object_contact = PlayerEnemyContact{
                record_index, hit_slot, objects.records[record_index].animated_kind};
            return result;
        }
        case PlayerObjectContactEffect::Bridge: {
            PlayerContactResult result;
            result.bridge = PlayerBridgeContact{
                record_index, hit_slot, slot.bridge_sprite_present};
            return result;
        }
        case PlayerObjectContactEffect::Refuel: {
            PlayerContactResult result;
            result.refuel_contact = true;
            return result;
        }
        case PlayerObjectContactEffect::Ignore:
            return terrain_result(snapshot);
        }
    }
    return terrain_result(snapshot);
}

PlayerContactResult sample_player_contact(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose) {
    const auto snapshot = sample_player_contact_snapshot(world, player, pose);
    auto result = select_player_contact(snapshot, world.object_state());
    // A byte-wrapped aircraft cannot use the native viewport edge as an escape
    // route. This playability policy is not part of the source selector above.
    result.terrain_contact |= snapshot.player_fully_offscreen;
    return result;
}

} // namespace river_raid
