#include "app/shot_overlay.hpp"

#include "shot_data.hpp"
#include "presentation_palette.hpp"
#include "render/sprite_scene.hpp"

#include <array>

namespace river_raid {

void draw_shot_preview(Frame& destination, const PlayerSteeringState& player,
                       const PlayerShotResult& shot) {
    if (shot.pose == PlayerShotPose::Hidden) return;
    namespace data = assets::shot;
    const std::array layers{ImageLayer{data::pixels, data::width, data::height,
        2 * static_cast<int>(player.horizontal_position) - 24,
        static_cast<int>(shot.state.vertical_position) - 50}};
    draw_sprite_layers(destination, layers, assets::presentation_palette,
                       {0, 0, destination.width, destination.height});
}

} // namespace river_raid
