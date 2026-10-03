#pragma once

#include "app/generator_setup.hpp"
#include "game/world_objects.hpp"
#include "game/river_world.hpp"

namespace river_raid {

[[nodiscard]] WorldObjectHistory make_world_object_history();
[[nodiscard]] RiverWorld make_river_world(RiverStartPreset preset);
[[nodiscard]] RiverWorld make_river_world();

} // namespace river_raid
