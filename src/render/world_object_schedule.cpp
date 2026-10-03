#include "render/world_object_schedule.hpp"
#include "game/demo_presentation.hpp"

#include <stdexcept>

namespace river_raid {
namespace {

constexpr int kFirstBandCut = 55;
constexpr int kBandHeight = 32;
constexpr int kFinalBandCut = 211;
constexpr int kFixedCutThreshold = 204;
constexpr std::uint8_t kDirectionFlag = 0x08;
constexpr std::uint8_t kDirectionImageOffset = 16;
constexpr int kViewportLeft = 24;
constexpr int kViewportTop = 50;

std::vector<int> band_cuts(std::uint8_t phase) {
    if (phase >= kBandHeight) {
        throw std::invalid_argument("World object schedule phase exceeds 31");
    }

    std::vector<int> cuts;
    cuts.reserve(6);
    const auto first_cut = kFirstBandCut + static_cast<int>(phase);
    cuts.push_back(first_cut);

    for (int band = 1; band <= 4; ++band) {
        const auto cut = first_cut + kBandHeight * band;
        cuts.push_back(cut);
        if (cut >= kFinalBandCut) return cuts;
    }

    if (cuts.back() < kFixedCutThreshold) cuts.push_back(kFinalBandCut);
    return cuts;
}

WorldObjectPlacement placement_for(const WorldObjectRecord& record,
                                   std::size_t record_index,
                                   WorldObjectImageSlot image_slot) {
    const auto direction_offset = (record.behavior_flags & kDirectionFlag) != 0
                                      ? kDirectionImageOffset
                                      : std::uint8_t{0};
    return {
        record_index,
        image_slot,
        2 * static_cast<int>(record.horizontal_coordinate) - kViewportLeft,
        static_cast<int>(record.vertical_position) - kViewportTop,
        static_cast<std::uint8_t>(direction_offset + record.animated_kind),
        record.base_kind,
    };
}

template <typename LaterBandState>
std::vector<WorldObjectBandSchedule> schedule_bands(
    std::uint8_t phase, std::span<const int> cuts,
    const WorldObjectState& top_band_state, LaterBandState later_band_state) {
    std::vector<WorldObjectBandSchedule> result;
    result.reserve(cuts.size());

    auto cursor = static_cast<std::size_t>(top_band_state.active_begin);
    const auto top_slots = phase < 16 ? WorldObjectBandSlots::alternate_only
                                      : WorldObjectBandSlots::primary_then_alternate;
    result.push_back(schedule_world_object_band(top_band_state, cursor, cuts.front(),
                                                top_slots));
    cursor = result.back().next_record;

    for (std::size_t index = 1; index < cuts.size(); ++index) {
        result.push_back(schedule_world_object_band(
            later_band_state(index - 1), cursor, cuts[index],
            WorldObjectBandSlots::primary_then_alternate));
        cursor = result.back().next_record;
    }
    return result;
}

} // namespace

WorldObjectBandSchedule schedule_world_object_band(
    const WorldObjectState& state, std::size_t record_cursor, int exclusive_bottom,
    WorldObjectBandSlots slots) {
    WorldObjectBandSchedule result{exclusive_bottom, record_cursor, {}};
    result.placements.reserve(slots == WorldObjectBandSlots::alternate_only ? 1 : 2);

    const auto offer = [&](WorldObjectImageSlot image_slot) {
        if (result.next_record >= state.records.size()) return;
        const auto& record = state.records[result.next_record];
        if (static_cast<int>(record.vertical_position) >= exclusive_bottom) return;
        result.placements.push_back(
            placement_for(record, result.next_record, image_slot));
        ++result.next_record;
    };

    if (slots == WorldObjectBandSlots::primary_then_alternate) {
        offer(WorldObjectImageSlot::primary);
    }
    offer(WorldObjectImageSlot::alternate);
    return result;
}

std::vector<WorldObjectBandSchedule> schedule_world_object_bands(
    std::uint8_t phase, const WorldObjectState& top_band_state,
    std::span<const WorldObjectState> later_band_states) {
    const auto cuts = band_cuts(phase);
    const auto later_count = cuts.size() - 1;
    if (later_band_states.size() != later_count) {
        throw std::invalid_argument("World object schedule snapshot count does not match phase");
    }

    return schedule_bands(phase, cuts, top_band_state,
        [&](std::size_t index) -> const WorldObjectState& { return later_band_states[index]; });
}

std::vector<WorldObjectBandSchedule> schedule_world_object_snapshot(
    std::uint8_t phase, const WorldObjectState& state) {
    return schedule_world_object_snapshot(phase, state, state);
}

std::vector<WorldObjectBandSchedule> schedule_world_object_snapshot(
    std::uint8_t phase, const WorldObjectState& top_band_state,
    const WorldObjectState& later_band_state) {
    const auto cuts = band_cuts(phase);
    return schedule_bands(phase, cuts, top_band_state,
        [&](std::size_t) -> const WorldObjectState& { return later_band_state; });
}

std::vector<WorldObjectBandSchedule> schedule_demo_presentation(const DemoPresentationStep& step) {
    return schedule_world_object_snapshot(step.scroll_phase, step.before_motion, step.after_motion);
}

} // namespace river_raid
