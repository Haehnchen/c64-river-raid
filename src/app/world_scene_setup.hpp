#pragma once

#include "game/river_types.hpp"
#include "render/glyph_atlas.hpp"
#include "render/world_object_schedule.hpp"

#include <cstddef>
#include <span>

namespace river_raid {

[[nodiscard]] Frame make_world_scene(
    std::span<const PackedRiverRow> rows,
    std::span<const WorldObjectPlacement> placements,
    std::size_t selected_option = 0, bool bridge_flash = false);

[[nodiscard]] Frame make_demo_scene(const DemoPresentationStep& step,
                                    std::size_t selected_option = 0);

} // namespace river_raid
