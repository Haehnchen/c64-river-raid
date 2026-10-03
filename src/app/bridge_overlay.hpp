#pragma once

#include "game/bridge_sprites.hpp"
#include "render/contact_masks.hpp"
#include "render/character_scene.hpp"

namespace river_raid {

inline constexpr int kBridgeViewportOriginX = 24;
inline constexpr int kBridgeViewportOriginY = 50;

[[nodiscard]] OccupancyMask make_bridge_sprite_contact_mask(BridgeSpriteImage image);
void draw_bridge_sprites(Frame& destination, const BridgeSpriteState& state);

} // namespace river_raid
