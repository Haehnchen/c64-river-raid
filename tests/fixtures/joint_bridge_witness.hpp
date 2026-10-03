#pragma once

#include "game/bridge_sprites.hpp"

#include <cstdint>

namespace river_raid::fixtures::joint_bridge {

// Shared transaction semantics; the source and native routes use different courses.
struct JointBridgeSemanticWitness {
    std::uint8_t bridge_increment;
    std::uint16_t score_award;
    bool projectile_cleared;
    bool life_end_queued;
    bool same_sample_joint_contact;
    bool published_normal_during_hit;
    BridgeSpriteImage private_joint_image_after_hit;
    bool next_published_joint_family;
};

inline constexpr JointBridgeSemanticWitness expected{
    1, 750, true, false, true, true, BridgeSpriteImage::JointExplosionA, true,
};

} // namespace river_raid::fixtures::joint_bridge
