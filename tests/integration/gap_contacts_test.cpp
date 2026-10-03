#include "app/bridge_overlay.hpp"
#include "app/contact_asset_setup.hpp"
#include "app/gap_contacts.hpp"
#include "player_data.hpp"
#include "render/contact_producer.hpp"
#include "render/world_object_schedule.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace river_raid;

constexpr MaskClip kVisibleRiver{0, 4, 320, 157};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

WorldObjectState empty_objects() {
    WorldObjectState result{};
    result.active_begin = 0;
    for (auto& record : result.records) record.vertical_position = 0xff;
    return result;
}

WorldObjectState one_object(std::uint8_t vertical, std::uint8_t horizontal,
                            std::uint8_t kind = 7) {
    auto result = empty_objects();
    result.records[0] = {vertical, horizontal, 0, kind, kind, 30, 130, 0};
    return result;
}

bool masks_overlap(const OccupancyMask& first, int first_left, int first_top,
                   const OccupancyMask& second, int second_left, int second_top,
                   MaskClip clip) {
    const auto left = std::max({clip.left, first_left, second_left});
    const auto top = std::max({clip.top, first_top, second_top});
    const auto right = std::min({clip.left + clip.width,
                                 first_left + first.width,
                                 second_left + second.width});
    const auto bottom = std::min({clip.top + clip.height,
                                  first_top + first.height,
                                  second_top + second.height});
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (first.occupied_at(x - first_left, y - first_top) &&
                second.occupied_at(x - second_left, y - second_top)) {
                return true;
            }
        }
    }
    return false;
}

BridgeSpriteState gap_at(int left, int top,
                         BridgeSpriteImage image = BridgeSpriteImage::GapFlight) {
    BridgeSpriteState result{};
    result.gap = BridgeSpritePresentation{
        image, static_cast<std::uint16_t>(left + kBridgeViewportOriginX),
        static_cast<std::uint8_t>(top + kBridgeViewportOriginY), 1};
    return result;
}

std::optional<std::pair<int, int>> find_gap_origin(
    const OccupancyMask& gap_mask,
    const std::vector<std::pair<const OccupancyMask*, MaskPlacement>>& others,
    MaskClip clip = kVisibleRiver, int min_left = -24, int max_left = 500,
    int min_top = -49, int max_top = 205) {
    for (int top = min_top; top <= max_top; ++top) {
        for (int left = min_left; left <= max_left; ++left) {
            bool overlaps = true;
            for (const auto& [mask, placement] : others) {
                overlaps &= masks_overlap(gap_mask, left, top, *mask,
                                          placement.left, placement.top, clip);
            }
            if (overlaps) return std::pair{left, top};
        }
    }
    return std::nullopt;
}

WorldObjectPlacement placement_for(const WorldObjectState& objects,
                                   std::size_t record_index,
                                   WorldObjectImageSlot role,
                                   std::uint8_t phase = 0) {
    for (const auto& band : schedule_world_object_snapshot(phase, objects)) {
        for (const auto& placement : band.placements) {
            if (placement.record_index == record_index &&
                placement.image_slot == role) {
                return placement;
            }
        }
    }
    throw std::runtime_error("Synthetic object was not scheduled");
}

int slot_for_primary(const GapContactSnapshot& snapshot, ShotObjectIndex object) {
    for (std::size_t index = 0; index < snapshot.slots.size(); ++index) {
        if (snapshot.slots[index].primary_object == object) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

PlayerSteeringState offscreen_player() {
    return {255, 0, 0, 0};
}

void test_empty_gap_and_sampling_gate_independence() {
    const auto objects = one_object(80, 86);
    const auto empty = sample_gap_contacts(0, objects, {}, offscreen_player(),
                                           FlightPose::Straight);
    require(empty == GapContactSnapshot{}, "Absent gap produced contacts");

    const auto placement = placement_for(objects, 0, WorldObjectImageSlot::primary);
    const auto object_mask = make_world_object_contact_mask(placement);
    const auto gap_mask = make_bridge_sprite_contact_mask(BridgeSpriteImage::GapFlight);
    const auto origin = find_gap_origin(
        gap_mask, {{&object_mask, {&object_mask, placement.left, placement.top}}});
    require(origin.has_value(), "Could not construct a visible gap/object overlap");
    const auto sampled = sample_gap_contacts(
        0, objects, gap_at(origin->first, origin->second), offscreen_player(),
        FlightPose::Straight);
    const auto slot = slot_for_primary(sampled, ShotObjectIndex{0});
    require(slot >= 0 && sampled.slots[slot].gap_present &&
                sampled.slots[slot].primary_present,
            "Visible gap/object overlap was not sampled");

    WorldObjectState late = objects;
    late.records[0].animated_kind = 6;
    const auto live = select_gap_contact(17, sampled, late);
    require(!live.object, "Selector accepted a non-damaging current kind");
    late.records[0].animated_kind = 7;
    require(select_gap_contact(17, sampled, late).object.has_value(),
            "Selector counter gate or late kind was applied during sampling");
    require(!select_gap_contact(0, sampled, late).object,
            "Selector admitted a disabled live counter");
}

void test_shared_mask_truth_and_mapping() {
    auto objects = empty_objects();
    objects.records[0] = {80, 86, 0, 14, 14, 30, 130, 0};
    objects.records[1] = {80, 86, 0, 14, 14, 30, 130, 0};
    const auto primary = placement_for(objects, 0, WorldObjectImageSlot::primary);
    const auto alternate = placement_for(objects, 1, WorldObjectImageSlot::alternate);
    const auto primary_mask = make_world_object_contact_mask(primary);
    const auto alternate_mask = make_world_object_contact_mask(alternate);
    const auto gap_mask = make_bridge_sprite_contact_mask(BridgeSpriteImage::GapFlight);
    const auto origin = find_gap_origin(
        gap_mask, {{&primary_mask, {&primary_mask, primary.left, primary.top}},
                   {&alternate_mask, {&alternate_mask, alternate.left, alternate.top}}});
    require(origin.has_value(), "Could not align both scheduled object roles with the gap");
    const auto bridge = gap_at(origin->first, origin->second);
    const auto sampled = sample_gap_contacts(0, objects, bridge, offscreen_player(),
                                             FlightPose::Straight);
    const auto slot = slot_for_primary(sampled, ShotObjectIndex{0});
    require(slot >= 0 && sampled.slots[slot].gap_present &&
                sampled.slots[slot].primary_present && sampled.slots[slot].alternate_present &&
                sampled.slots[slot].primary_object == ShotObjectIndex{0} &&
                sampled.slots[slot].alternate_object == ShotObjectIndex{1},
            "Scheduled primary/alternate mappings were not preserved");

    const auto bands = schedule_world_object_snapshot(0, objects);
    const auto band_index = kShotContactSlotCount - 1U - static_cast<std::size_t>(slot);
    const auto previous_cut = band_index == 0 ? 54 : bands[band_index - 1].exclusive_bottom;
    const auto strip_bottom = std::min(211, bands[band_index].exclusive_bottom);
    const MaskClip strip{0, previous_cut - 50, 320, strip_bottom - previous_cut};
    const OccupancyMask empty_terrain{1, 1, {0}};

    for (const auto [dx, dy] : std::array<std::pair<int, int>, 3>{
             std::pair{0, 0}, std::pair{gap_mask.width + 2, 0},
             std::pair{0, gap_mask.height + 2}}) {
        const auto left = origin->first + dx;
        const auto top = origin->second + dy;
        const std::array sprites{
            ContactSpritePlacement{{&gap_mask, left, top}, 0x10},
            ContactSpritePlacement{{&primary_mask, primary.left, primary.top}, 0x80},
            ContactSpritePlacement{{&alternate_mask, alternate.left, alternate.top}, 0x40},
        };
        PixelContactAccumulator producer;
        producer.scan(sprites, {&empty_terrain, 0, 0}, strip);
        const auto participants = producer.consume_object_participants();
        const auto result = sample_gap_contacts(
            0, objects, gap_at(left, top), offscreen_player(), FlightPose::Straight);
        const auto result_slot = slot_for_primary(result, ShotObjectIndex{0});
        require(result_slot == slot &&
                    result.slots[result_slot].gap_present ==
                        ((participants & 0x10U) != 0) &&
                    result.slots[result_slot].primary_present ==
                        ((participants & 0x80U) != 0) &&
                    result.slots[result_slot].alternate_present ==
                        ((participants & 0x40U) != 0),
                "Gap adapter contact flags differ from the shared strip latch");
    }
}

void test_lower_player_aggregation() {
    auto objects = empty_objects();
    const auto player = PlayerSteeringState{80, 0, 0, 0};
    const auto player_mask = make_player_contact_mask(FlightPose::Straight);
    const auto player_left = 2 * static_cast<int>(player.horizontal_position) - 24;
    const auto player_top = static_cast<int>(assets::player::fixed_y);
    const auto gap_mask = make_bridge_sprite_contact_mask(BridgeSpriteImage::GapFlight);
    const auto origin = find_gap_origin(
        gap_mask, {{&player_mask, {&player_mask, player_left, player_top}}});
    require(origin.has_value(), "Could not align the lower player with the gap");
    const auto result = sample_gap_contacts(
        0, objects, gap_at(origin->first, origin->second), player, FlightPose::Straight);
    require(result.slots[0].gap_present && result.slots[0].player_present,
            "Player/gap contact was not aggregated into lower slot zero");
    for (std::size_t index = 1; index < result.slots.size(); ++index) {
        require(!result.slots[index].gap_present && !result.slots[index].player_present,
                "Lower player contact leaked into an upper gap slot");
    }
}

void test_hud_and_offscreen_clipping() {
    const auto gap_mask = make_bridge_sprite_contact_mask(BridgeSpriteImage::GapFlight);

    auto hud_objects = one_object(30, 86);
    const auto hud_placement = placement_for(
        hud_objects, 0, WorldObjectImageSlot::primary, 16);
    const auto hud_mask = make_world_object_contact_mask(hud_placement);
    const auto all_pixels = MaskClip{-1000, -1000, 2000, 2000};
    const auto hud_origin = find_gap_origin(
        gap_mask, {{&hud_mask, {&hud_mask, hud_placement.left, hud_placement.top}}},
        all_pixels);
    require(hud_origin.has_value() && hud_origin->second < kVisibleRiver.top,
            "Could not construct an overlap in the HUD-clipped area");
    const auto hud = sample_gap_contacts(
        16, hud_objects, gap_at(hud_origin->first, hud_origin->second),
        offscreen_player(), FlightPose::Straight);
    require(std::ranges::all_of(hud.slots, [](const auto& slot) {
                return !slot.gap_present && !slot.player_present &&
                       !slot.primary_present && !slot.alternate_present;
            }), "HUD-clipped gap contact leaked into the scene");

    auto offscreen_objects = one_object(80, 172);
    const auto offscreen_placement = placement_for(
        offscreen_objects, 0, WorldObjectImageSlot::primary);
    const auto offscreen_mask = make_world_object_contact_mask(offscreen_placement);
    const auto offscreen_origin = find_gap_origin(
        gap_mask, {{&offscreen_mask,
                    {&offscreen_mask, offscreen_placement.left, offscreen_placement.top}}},
        all_pixels, 320, 500, 4, 156);
    require(offscreen_origin.has_value(),
            "Could not construct an overlap outside the right scene edge");
    const auto offscreen = sample_gap_contacts(
        0, offscreen_objects, gap_at(offscreen_origin->first, offscreen_origin->second),
        offscreen_player(), FlightPose::Straight);
    require(std::ranges::all_of(offscreen.slots, [](const auto& slot) {
                return !slot.gap_present && !slot.player_present &&
                       !slot.primary_present && !slot.alternate_present;
            }), "Offscreen gap contact leaked into the scene");
}

} // namespace

int main() {
    try {
        test_empty_gap_and_sampling_gate_independence();
        test_shared_mask_truth_and_mapping();
        test_lower_player_aggregation();
        test_hud_and_offscreen_clipping();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
