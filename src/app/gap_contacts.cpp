#include "app/gap_contacts.hpp"

#include "app/contact_scene.hpp"

namespace river_raid {

GapContactSnapshot sample_gap_contacts(
    std::uint8_t phase, const WorldObjectState& objects, const BridgeSpriteState& bridge,
    const PlayerSteeringState& player, FlightPose pose, LifeCycleImage image) {
    return detail::sample_contact_scene({
        objects, phase, nullptr, &bridge, nullptr, player, pose, image, nullptr}).gap;
}

} // namespace river_raid
