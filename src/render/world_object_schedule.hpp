#pragma once

#include "game/world_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace river_raid {

struct DemoPresentationStep;

enum class WorldObjectImageSlot : std::uint8_t {
    primary,
    alternate,
};

enum class WorldObjectBandSlots : std::uint8_t {
    alternate_only,
    primary_then_alternate,
};

struct WorldObjectPlacement {
    std::size_t record_index{};
    WorldObjectImageSlot image_slot{};
    int left{};
    int top{};
    std::uint8_t image_key{};
    std::uint8_t style_key{};

    friend bool operator==(const WorldObjectPlacement&, const WorldObjectPlacement&) = default;
};

struct WorldObjectBandSchedule {
    int exclusive_bottom{};
    std::size_t next_record{};
    std::vector<WorldObjectPlacement> placements;

    friend bool operator==(const WorldObjectBandSchedule&,
                           const WorldObjectBandSchedule&) = default;
};

// Stops at the first record outside the band; records are never skipped.
WorldObjectBandSchedule schedule_world_object_band(
    const WorldObjectState& state, std::size_t record_cursor, int exclusive_bottom,
    WorldObjectBandSlots slots);

// The first band reads top_band_state. Each later cut reads its corresponding
// snapshot while retaining the cursor. The required later snapshot count is
// phase-dependent and must exactly match the returned later bands.
std::vector<WorldObjectBandSchedule> schedule_world_object_bands(
    std::uint8_t phase, const WorldObjectState& top_band_state,
    std::span<const WorldObjectState> later_band_states);

// Diagnostic single-state view, not a reconstruction of a timed frame.
std::vector<WorldObjectBandSchedule> schedule_world_object_snapshot(
    std::uint8_t phase, const WorldObjectState& state);

// Use one snapshot for the top band and a shared snapshot for every later band.
std::vector<WorldObjectBandSchedule> schedule_world_object_snapshot(
    std::uint8_t phase, const WorldObjectState& top_band_state,
    const WorldObjectState& later_band_state);

// Initialized demo: top band precedes motion; later bands precede row updates.
std::vector<WorldObjectBandSchedule> schedule_demo_presentation(const DemoPresentationStep& step);

} // namespace river_raid
