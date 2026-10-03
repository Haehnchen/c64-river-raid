#pragma once

#include "game/player_shot.hpp"
#include "game/river_world.hpp"
#include "game/shot_contact.hpp"

namespace river_raid {

struct ShotContactResolution {
    ShotContactResult selection;
    std::optional<WorldObjectHitEffect> object_effect;
    bool bridge_sprite_contact{};
};

// Contacts must refer to the caller's admitted collision-consumption boundary.
[[nodiscard]] ShotContactResolution resolve_shot_contacts(
    RiverWorld& world, PlayerShotResult& shot,
    const std::array<ShotContactSlot, kShotContactSlotCount>& contacts,
    bool round_event_pending);

} // namespace river_raid
