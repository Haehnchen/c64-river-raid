#include "app/auxiliary_contacts.hpp"

#include "app/contact_scene.hpp"

namespace river_raid {

AuxiliaryContactSnapshot sample_auxiliary_projectile_contacts(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose,
    LifeCycleImage image) {
    const auto bridge = world.bridge_presentation_state();
    return detail::sample_contact_scene({
        world.object_state(), world.river_state().phase, &world.viewport(), &bridge,
        &world.auxiliary_projectile_state(), player, pose, image, nullptr}).auxiliary;
}

} // namespace river_raid
