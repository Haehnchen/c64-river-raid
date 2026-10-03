#include "app/flight_scene.hpp"

#include "app/auxiliary_overlay.hpp"
#include "app/bridge_overlay.hpp"
#include "app/flight_hud.hpp"
#include "app/player_overlay.hpp"
#include "app/shot_overlay.hpp"
#include "app/world_scene_setup.hpp"
#include "render/world_object_schedule.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace river_raid {

Frame make_flight_scene(const FlightPreview& flight) {
    if (flight.status() == FlightStatus::Blank)
        return {320, 200, std::vector<std::uint32_t>(320 * 200, 0xff000000)};
    const auto& active_world = flight.world();
    std::vector<WorldObjectPlacement> placements;
    for (const auto& band : flight.presented_object_bands()) {
        placements.insert(placements.end(), band.placements.begin(), band.placements.end());
    }
    auto image = make_world_scene(std::span(active_world.viewport().rows()).first(157), placements,
                                  flight.selected_option(), flight.bridge_flash());
    draw_bridge_sprites(image, active_world.bridge_presentation_state());
    draw_auxiliary_projectile(image, active_world.auxiliary_projectile_state());
    draw_shot_preview(image, flight.presented_steering(), flight.shot());
    draw_player_preview(image, flight.presented_steering(), flight.presented_pose(),
                         flight.presented_image(),
                         static_cast<std::uint8_t>(6 + flight.life_cycle().active_player));
    draw_flight_hud(image, flight.displayed_player_score(0), flight.lives(), flight.fuel(),
                    flight.bridge_number(),
                    flight.two_players() ? flight.displayed_player_score(1) : flight.high_score(),
                    flight.two_players());
    return image;
}

} // namespace river_raid
