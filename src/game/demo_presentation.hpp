#pragma once

#include "game/river_viewport.hpp"
#include "game/world_objects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace river_raid {

// Semantic stages of one simulation step, before any display-band selection.
struct DemoPresentationStep {
    std::uint8_t animation_phase{};
    std::uint8_t scroll_phase{};
    std::size_t rows_before{};
    std::uint8_t row_count{};
    WorldObjectState before_motion;
    WorldObjectState after_motion;
    std::array<WorldObjectState, 2> after_rows{};
    std::array<PackedRiverRow, RiverViewport::height> terrain_before_scroll{};

    friend bool operator==(const DemoPresentationStep&, const DemoPresentationStep&) = default;
};

} // namespace river_raid
