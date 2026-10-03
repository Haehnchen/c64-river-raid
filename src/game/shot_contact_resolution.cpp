#include "game/shot_contact_resolution.hpp"

#include "game/score.hpp"

#include <stdexcept>

namespace river_raid {

ShotContactResolution resolve_shot_contacts(
    RiverWorld& world, PlayerShotResult& shot,
    const std::array<ShotContactSlot, kShotContactSlotCount>& contacts,
    bool round_event_pending) {
    ShotContactSnapshot snapshot{};
    snapshot.slots = contacts;
    snapshot.suppress_score_kind_write = round_event_pending;
    for (std::size_t index = 0; index < snapshot.object_kinds.size(); ++index) {
        snapshot.object_kinds[index] = world.object_state().records[index].animated_kind;
    }
    ShotContactResolution result{select_shot_contact(snapshot), std::nullopt};
    if (result.selection.outcome == ShotContactOutcome::Ordinary ||
        result.selection.outcome == ShotContactOutcome::Bridge) {
        const auto selected = *result.selection.object;
        result.object_effect = world.apply_projectile_hit(selected.index.value,
            selected.role == ShotObjectRole::Primary ? WorldObjectHitSlot::Primary : WorldObjectHitSlot::Alternate,
            round_event_pending);
        if (!result.object_effect) throw std::logic_error("Selected object hit could not be applied");
        if (result.selection.outcome == ShotContactOutcome::Bridge) {
            result.bridge_sprite_contact = contacts[result.selection.slot->value].bridge_sprite_present;
            if (const auto write = bridge_score_kind_write(result.bridge_sprite_contact,
                                                           round_event_pending))
                result.object_effect->score_kind = write;
        }
    }
    if (result.selection.clear_projectile) {
        clear_player_shot(shot);
    }
    return result;
}

} // namespace river_raid
