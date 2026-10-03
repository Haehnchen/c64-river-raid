#pragma once

#include "app/foreground_contacts.hpp"

namespace river_raid::detail {

// Internal staged scene input. Top assignments use objects; optional later
// assignments use later_objects. Null actor pointers omit unavailable sprites.
struct ContactSceneInput {
    const WorldObjectState& objects;
    std::uint8_t phase{};
    const RiverViewport* viewport{};
    const BridgeSpriteState* bridge{};
    const AuxiliaryProjectileState* auxiliary{};
    const PlayerSteeringState& player;
    FlightPose pose{FlightPose::Straight};
    LifeCycleImage image{LifeCycleImage::Straight};
    const PlayerShotResult* shot{};
    const WorldObjectState* later_objects{};
};

std::vector<WorldObjectBandSchedule> produce_contact_scene(
    ContactSamplingBuffer& samples, ContactPresentationMetadata& metadata,
    const ContactSceneInput& input);
[[nodiscard]] ForegroundContactSnapshot decode_contact_scene(
    const ContactSamplingSnapshot& samples,
    const ContactPresentationMetadata& metadata) noexcept;
[[nodiscard]] ForegroundContactSnapshot sample_contact_scene(const ContactSceneInput& input);

} // namespace river_raid::detail
