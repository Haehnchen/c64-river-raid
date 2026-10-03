#include "app/flight_preview_contacts.hpp"

#include "app/contact_scene.hpp"

namespace river_raid {

std::array<ShotContactSlot, kShotContactSlotCount>
sample_flight_preview_shot_contacts(const RiverWorld& world,
                                    const PlayerSteeringState& player,
                                    const PlayerShotResult& shot) {
    const auto bridge = world.bridge_presentation_state();
    return detail::sample_contact_scene({
        world.object_state(), world.river_state().phase, &world.viewport(), &bridge,
        &world.auxiliary_projectile_state(), player, FlightPose::Straight,
        LifeCycleImage::Hidden, &shot}).shot;
}

} // namespace river_raid
