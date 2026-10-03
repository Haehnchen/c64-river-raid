#include "app/foreground_contacts.hpp"

#include "app/auxiliary_overlay.hpp"
#include "app/bridge_overlay.hpp"
#include "app/contact_asset_setup.hpp"
#include "app/contact_scene.hpp"
#include "game/contact_sampling.hpp"
#include "player_data.hpp"
#include "render/contact_producer.hpp"
#include "render/world_object_schedule.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace river_raid {
namespace {

constexpr std::uint8_t kGapParticipant = 0x10;
constexpr int kViewportOriginX = 24;
constexpr int kViewportOriginY = 50;
constexpr int kFirstVisibleVicY = 54;
constexpr int kLastVisibleVicY = 211;
constexpr MaskClip kVisibleRiver{0, 4, 320, 157};
const OccupancyMask kEmptyMask{1, 1, {0}};

[[nodiscard]] bool has(std::uint8_t mask, std::uint8_t participant) noexcept {
    return (mask & participant) != 0;
}

[[nodiscard]] MaskClip band_clip(int previous_cut, int exclusive_bottom) noexcept {
    const auto top = std::max(kFirstVisibleVicY, previous_cut) - kViewportOriginY;
    const auto bottom = std::min(kLastVisibleVicY, exclusive_bottom) - kViewportOriginY;
    return {kVisibleRiver.left, top, kVisibleRiver.width, std::max(0, bottom - top)};
}

void add_sprite(std::vector<ContactSpritePlacement>& sprites,
                const OccupancyMask& mask, int left, int top,
                std::uint8_t participant) {
    sprites.push_back({{&mask, left, top}, participant});
}

} // namespace

std::vector<WorldObjectBandSchedule> detail::produce_contact_scene(
    ContactSamplingBuffer& contact_samples, ContactPresentationMetadata& metadata,
    const ContactSceneInput& input) {
    const auto shot_visible = input.shot && input.shot->pose == PlayerShotPose::Visible;
    const auto aircraft_visible = input.image != LifeCycleImage::Hidden;
    const auto player_straight = aircraft_visible && input.image == LifeCycleImage::Straight;
    const auto player_left = 2 * static_cast<int>(input.player.horizontal_position) -
                             kViewportOriginX;
    const auto aircraft = aircraft_visible
        ? make_player_contact_mask(input.pose, input.image) : kEmptyMask;
    const auto player_detail = player_straight ? make_player_detail_contact_mask() : kEmptyMask;
    const auto projectile = shot_visible ? make_projectile_contact_mask() : kEmptyMask;
    const auto terrain = input.viewport
        ? make_river_contact_mask(std::span(input.viewport->rows()).first(kVisibleRiver.height))
        : kEmptyMask;
    const MaskPlacement terrain_placement{
        &terrain, input.viewport ? kVisibleRiver.left : 0,
        input.viewport ? kVisibleRiver.top : 0};

    const auto crossing = input.bridge && input.bridge->crossing
        ? make_bridge_sprite_contact_mask(input.bridge->crossing->image) : kEmptyMask;
    const auto gap = input.bridge && input.bridge->gap
        ? make_bridge_sprite_contact_mask(input.bridge->gap->image) : kEmptyMask;
    const auto auxiliary = input.auxiliary && input.auxiliary->presentation
        ? make_auxiliary_projectile_contact_mask() : kEmptyMask;

    metadata = {shot_visible, player_straight,
                player_straight && (player_left >= kVisibleRiver.width ||
                                    player_left + aircraft.width <= kVisibleRiver.left),
                input.bridge && input.bridge->gap.has_value()};

    auto bands = schedule_world_object_snapshot(input.phase, input.objects,
        input.later_objects ? *input.later_objects : input.objects);
    const auto record_sample = [&](std::size_t slot_index, PixelContactAccumulator& producer,
                                   ContactSampleChannels channels) {
        producer.sample_into(contact_samples,
            ContactSectionIndex{static_cast<std::uint8_t>(slot_index)}, channels);
    };
    auto previous_cut = kFirstVisibleVicY;
    for (std::size_t band_index = 0; band_index < bands.size(); ++band_index) {
        const auto slot_index = kShotContactSlotCount - 1U - band_index;
        const auto section = ContactSectionIndex{static_cast<std::uint8_t>(slot_index)};
        const auto& band = bands[band_index];

        std::vector<OccupancyMask> object_masks;
        object_masks.reserve(band.placements.size());
        for (const auto& placement : band.placements) {
            object_masks.push_back(make_world_object_contact_mask(placement));
        }
        std::vector<ContactSpritePlacement> sprites;
        sprites.reserve(band.placements.size() + 6);
        for (std::size_t index = 0; index < band.placements.size(); ++index) {
            const auto& placement = band.placements[index];
            const auto primary = placement.image_slot == WorldObjectImageSlot::primary;
            add_sprite(sprites, object_masks[index], placement.left, placement.top,
                       primary ? kPrimaryObjectContactParticipant
                               : kSecondaryObjectContactParticipant);
            const auto object = ShotObjectIndex{
                static_cast<std::uint8_t>(placement.record_index)};
            contact_samples.replace_object_mapping(section,
                primary ? ShotObjectRole::Primary : ShotObjectRole::Secondary, object);
        }
        if (shot_visible) {
            add_sprite(sprites, projectile, player_left,
                       static_cast<int>(input.shot->state.vertical_position) - kViewportOriginY,
                       kProjectileContactParticipant);
        }
        if (aircraft_visible) {
            add_sprite(sprites, aircraft, player_left, assets::player::fixed_y,
                       kPlayerMainParticipant);
        }
        if (player_straight) {
            add_sprite(sprites, player_detail, player_left, assets::player::fixed_y,
                       kPlayerDetailParticipant);
        }
        if (input.auxiliary && input.auxiliary->presentation) {
            const auto& sprite = *input.auxiliary->presentation;
            add_sprite(sprites, auxiliary, sprite.vic_x - kViewportOriginX,
                       sprite.vic_y - kViewportOriginY, kAuxiliaryParticipant);
        }
        if (input.bridge && input.bridge->gap) {
            const auto& sprite = *input.bridge->gap;
            add_sprite(sprites, gap, sprite.vic_x - kBridgeViewportOriginX,
                       sprite.vic_y - kBridgeViewportOriginY, kGapParticipant);
        }
        if (input.bridge && input.bridge->crossing) {
            const auto& sprite = *input.bridge->crossing;
            add_sprite(sprites, crossing, sprite.vic_x - kBridgeViewportOriginX,
                       sprite.vic_y - kBridgeViewportOriginY,
                       kBridgeSpriteContactParticipant);
        }

        PixelContactAccumulator producer;
        // The final scheduled band has three source read paths. For phases
        // 21..27, section 1 reads objects only and terrain keeps accumulating
        // until section 0. For phases 28..31, section 1 is never read. The
        // physical placements still belong to the scheduled section 1; the
        // section-0 lower read has no newly assigned object mapping.
        const auto final_band = band_index + 1 == bands.size();
        if (final_band && input.phase >= 21 && input.phase <= 27) {
            producer.scan(sprites, terrain_placement,
                          band_clip(previous_cut, band.exclusive_bottom));
            record_sample(slot_index, producer, ContactSampleChannels::Objects);
            producer.scan(sprites, terrain_placement,
                          band_clip(band.exclusive_bottom, kLastVisibleVicY));
            record_sample(0, producer, ContactSampleChannels::Both);
        } else {
            producer.scan(sprites, terrain_placement,
                          band_clip(previous_cut, band.exclusive_bottom));
            record_sample(final_band && input.phase >= 28 ? 0 : slot_index,
                          producer, ContactSampleChannels::Both);
        }
        previous_cut = band.exclusive_bottom;
    }
    return bands;
}

ForegroundContactSnapshot detail::decode_contact_scene(
    const ContactSamplingSnapshot& published,
    const ContactPresentationMetadata& metadata) noexcept {
    ForegroundContactSnapshot result{};
    result.shot_presented = metadata.shot_presented;
    if (metadata.player_straight) {
        result.player = make_player_contact_snapshot(published);
        result.player.player_fully_offscreen = metadata.player_fully_offscreen;
    }
    if (metadata.shot_presented) result.shot = make_shot_contact_slots(published);
    for (std::size_t index = 0; index < published.sections.size(); ++index) {
        const auto& section = published.sections[index];
        const auto& sample = section.sample;
        result.auxiliary.terrain[index] = static_cast<std::uint8_t>(
            sample.terrain_participants & kAuxiliaryParticipant);
        if (index < result.auxiliary.player.size()) {
            result.auxiliary.player[index] = static_cast<std::uint8_t>(
                sample.object_participants &
                (kAuxiliaryParticipant | kPlayerMainParticipant));
        }
        if (metadata.gap_presented) {
            auto& gap_slot = result.gap.slots[index];
            gap_slot.gap_present = has(sample.object_participants, kGapParticipant);
            gap_slot.player_present = has(sample.object_participants, kPlayerMainParticipant);
            gap_slot.primary_present = has(sample.object_participants,
                                           kPrimaryObjectContactParticipant);
            gap_slot.alternate_present = has(sample.object_participants,
                                             kSecondaryObjectContactParticipant);
            gap_slot.primary_object = section.primary_object;
            gap_slot.alternate_object = section.secondary_object;
        }
    }
    return result;
}

ForegroundContactSnapshot detail::sample_contact_scene(const ContactSceneInput& input) {
    ContactSamplingBuffer contact_samples;
    ContactPresentationMetadata metadata;
    (void)produce_contact_scene(contact_samples, metadata, input);
    contact_samples.rollover_for_foreground();
    return decode_contact_scene(contact_samples.foreground_snapshot(), metadata);
}

ForegroundContactSnapshot ForegroundContactPipeline::publish() noexcept {
    samples_.rollover_for_foreground();
    published_ = working_;
    working_ = {};
    return detail::decode_contact_scene(samples_.foreground_snapshot(), published_);
}

std::vector<WorldObjectBandSchedule> ForegroundContactPipeline::produce(
    const RiverWorld& world, const WorldObjectState& top_objects, std::uint8_t phase,
    const PlayerSteeringState& player, FlightPose pose, const PlayerShotResult& shot,
    LifeCycleImage image) {
    const auto bridge = world.bridge_presentation_state();
    return detail::produce_contact_scene(samples_, working_, {
        top_objects, phase, &world.viewport(), &bridge,
        &world.auxiliary_projectile_state(), player, pose, image, &shot,
        &world.object_state()});
}

void ForegroundContactPipeline::reset() noexcept {
    samples_ = {};
    working_ = {};
    published_ = {};
}

ForegroundContactSnapshot sample_foreground_contacts(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose,
    const PlayerShotResult& shot, LifeCycleImage image) {
    const auto bridge = world.bridge_presentation_state();
    return detail::sample_contact_scene({
        world.object_state(), world.river_state().phase, &world.viewport(), &bridge,
        &world.auxiliary_projectile_state(), player, pose, image, &shot});
}

} // namespace river_raid
