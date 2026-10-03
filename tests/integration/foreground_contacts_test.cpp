#include "app/contact_asset_setup.hpp"
#include "app/contact_scene.hpp"
#include "app/foreground_contacts.hpp"
#include "app/auxiliary_overlay.hpp"
#include "app/bridge_overlay.hpp"
#include "app/generator_setup.hpp"
#include "game/shot_contact_resolution.hpp"
#include "player_data.hpp"
#include "render/world_object_schedule.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

WorldObjectPlacement find_placement(
    const std::vector<WorldObjectBandSchedule>& bands, std::size_t record,
    WorldObjectImageSlot role, std::size_t& band_index) {
    for (std::size_t index = 0; index < bands.size(); ++index) {
        for (const auto& placement : bands[index].placements) {
            if (placement.record_index == record && placement.image_slot == role) {
                band_index = index;
                return placement;
            }
        }
    }
    throw std::runtime_error("Fixture object was not placed in a raster band");
}

bool masks_overlap(const OccupancyMask& first, int first_left, int first_top,
                   const OccupancyMask& second, int second_left, int second_top,
                   MaskClip clip) {
    const int left = std::max({clip.left, first_left, second_left});
    const int top = std::max({clip.top, first_top, second_top});
    const int right = std::min({clip.left + clip.width,
                                first_left + first.width,
                                second_left + second.width});
    const int bottom = std::min({clip.top + clip.height,
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

struct SplitFixture {
    std::uint8_t player_horizontal{86};
    std::uint8_t shot_vertical{};
    std::uint8_t primary_vertical{};
    std::uint8_t primary_horizontal{};
    std::uint8_t alternate_vertical{};
    std::uint8_t alternate_horizontal{};
};

std::optional<SplitFixture> find_split_fixture() {
    constexpr std::uint8_t primary_kind = 9;
    constexpr std::uint8_t alternate_kind = 10;
    constexpr int first_vertical = 183;
    constexpr int last_vertical = 210;

    WorldObjectPlacement primary_template{
        0, WorldObjectImageSlot::primary, 0, 0, primary_kind, primary_kind};
    WorldObjectPlacement alternate_template{
        1, WorldObjectImageSlot::alternate, 0, 0, alternate_kind, alternate_kind};
    const auto primary_mask = make_world_object_contact_mask(primary_template);
    const auto alternate_mask = make_world_object_contact_mask(alternate_template);
    const auto aircraft_mask = make_player_contact_mask(FlightPose::Straight);
    const auto projectile_mask = make_projectile_contact_mask();
    const int player_left = 2 * 86 - 24;

    // The initialized course has phase 0. Derive this fixture's final schedule
    // strip instead of embedding a second copy of the six-band cut table.
    WorldObjectState empty_objects{};
    for (auto& record : empty_objects.records) record.vertical_position = 0xff;
    const auto bands = schedule_world_object_snapshot(0, empty_objects);
    require(bands.size() == kShotContactSlotCount,
            "Phase-zero fixture no longer has six scheduled bands");
    const auto previous_cut = bands.back().exclusive_bottom == 211
        ? bands[bands.size() - 2].exclusive_bottom : 0;
    require(previous_cut != 0,
            "Could not derive the phase-zero lower raster strip");
    const MaskClip clip{0, previous_cut - 50, 320,
                        bands.back().exclusive_bottom - previous_cut};

    for (int shot_vertical = first_vertical; shot_vertical <= last_vertical + 1;
         ++shot_vertical) {
        const int shot_top = shot_vertical - 50;
        for (int primary_vertical = first_vertical;
             primary_vertical <= last_vertical; ++primary_vertical) {
            std::optional<int> primary_horizontal;
            for (int horizontal = 12; horizontal <= 171; ++horizontal) {
                const int left = 2 * horizontal - 24;
                const int top = primary_vertical - 50;
                if (!masks_overlap(aircraft_mask, player_left, assets::player::fixed_y,
                                   primary_mask, left, top, clip) ||
                    masks_overlap(projectile_mask, player_left, shot_top,
                                  primary_mask, left, top, clip)) {
                    continue;
                }
                primary_horizontal = horizontal;
                break;
            }
            if (!primary_horizontal) continue;

            for (int alternate_vertical = primary_vertical;
                 alternate_vertical <= last_vertical; ++alternate_vertical) {
                for (int horizontal = 12; horizontal <= 171; ++horizontal) {
                    const int left = 2 * horizontal - 24;
                    const int top = alternate_vertical - 50;
                    if (!masks_overlap(projectile_mask, player_left, shot_top,
                                       alternate_mask, left, top, clip)) {
                        continue;
                    }
                    return SplitFixture{
                        86, static_cast<std::uint8_t>(shot_vertical),
                        static_cast<std::uint8_t>(primary_vertical),
                        static_cast<std::uint8_t>(*primary_horizontal),
                        static_cast<std::uint8_t>(alternate_vertical),
                        static_cast<std::uint8_t>(horizontal)};
                }
            }
        }
    }
    return std::nullopt;
}

RiverWorld make_world(const SplitFixture& fixture) {
    static constexpr std::array<std::uint8_t, 16> delays{}, clearance{};
    static constexpr std::array<std::uint8_t, 8> choices{};
    WorldObjectState objects{};
    objects.active_begin = 0;
    objects.generator_cursor = kWorldObjectCapacity;
    objects.special_cursor = kWorldObjectCapacity;
    for (auto& record : objects.records) record.vertical_position = 0xff;

    // Both ordered records reach the phase-zero lower strip. The chosen sprite
    // silhouettes separate the projectile/alternate and aircraft/primary pairs.
    objects.records[0] = {fixture.primary_vertical, fixture.primary_horizontal,
                          0, 9, 9, 30, 130, 0};
    objects.records[1] = {fixture.alternate_vertical, fixture.alternate_horizontal,
                          0, 10, 10, 30, 130, 0};
    return {make_river_generator(), WorldObjectHistory(objects, {delays, choices, clearance}),
            generator_edge_patterns()};
}

void test_shared_participant_latch_preserves_primary_priority() {
    const auto fixture = find_split_fixture();
    require(fixture.has_value(),
            "No source-asset scene separates projectile/alternate from aircraft/primary");
    auto world = make_world(*fixture);
    const PlayerSteeringState player{fixture->player_horizontal, 0, 0, 0};
    PlayerShotResult shot{{fixture->shot_vertical}, PlayerShotPose::Visible,
                          std::nullopt, std::nullopt};

    const auto bands = schedule_world_object_snapshot(
        world.river_state().phase, world.object_state());
    std::size_t primary_band{};
    std::size_t alternate_band{};
    const auto primary = find_placement(
        bands, 0, WorldObjectImageSlot::primary, primary_band);
    const auto alternate = find_placement(
        bands, 1, WorldObjectImageSlot::alternate, alternate_band);
    require(primary_band == alternate_band,
            "Fixture primary and alternate were not scheduled in one band");
    require(primary_band != 0,
            "Fixture needs a lower shared band, not the top-only scan");

    const auto primary_mask = make_world_object_contact_mask(primary);
    const auto alternate_mask = make_world_object_contact_mask(alternate);
    const auto aircraft_mask = make_player_contact_mask(FlightPose::Straight);
    const auto projectile_mask = make_projectile_contact_mask();
    const auto previous_cut = bands[primary_band - 1].exclusive_bottom;
    const MaskClip band_clip{
        0, previous_cut - 50, 320,
        bands[primary_band].exclusive_bottom - previous_cut};
    const auto player_left = 2 * static_cast<int>(player.horizontal_position) - 24;
    const auto projectile_top = static_cast<int>(shot.state.vertical_position) - 50;

    require(masks_overlap(aircraft_mask, player_left, assets::player::fixed_y,
                          primary_mask, primary.left, primary.top, band_clip),
            "Fixture aircraft did not overlap the primary object");
    require(masks_overlap(projectile_mask, player_left, projectile_top,
                          alternate_mask, alternate.left, alternate.top, band_clip),
            "Fixture projectile did not overlap the alternate object");
    require(!masks_overlap(projectile_mask, player_left, projectile_top,
                           primary_mask, primary.left, primary.top, band_clip),
            "Fixture projectile also overlapped the primary object");

    // Production composite sampling must retain the shared global participant
    // union (0xC3) for both consumers, even though the projectile itself misses
    // the primary sprite. This tests the source-style participant latch, not a
    // reconstructed raster timeline or a synthetic contact snapshot.
    const auto contacts = sample_foreground_contacts(
        world, player, FlightPose::Straight, shot);
    require(contacts.shot_presented,
            "Visible typed projectile was not marked as presented");

    std::size_t contact_slot = kShotContactSlotCount;
    for (std::size_t index = 0; index < contacts.shot.size(); ++index) {
        const auto& sampled = contacts.shot[index];
        if (sampled.primary_object == ShotObjectIndex{0} &&
            sampled.secondary_object == ShotObjectIndex{1}) {
            contact_slot = index;
            break;
        }
    }
    require(contact_slot == kShotContactSlotCount - 1U - primary_band,
            "Shared contact adapters changed the scheduled band mappings");

    const auto& shot_contact = contacts.shot[contact_slot];
    require(shot_contact.object_contact && shot_contact.primary_present &&
                shot_contact.secondary_present && !shot_contact.terrain_contact,
            "Shot adapter did not publish the shared $C3 participant union");
    const auto& aircraft_contact = contacts.player.slots[contact_slot];
    require(aircraft_contact.object_contact && aircraft_contact.primary_present &&
                aircraft_contact.alternate_present &&
                aircraft_contact.primary_object == ShotObjectIndex{0} &&
                aircraft_contact.alternate_object == ShotObjectIndex{1},
            "Aircraft adapter did not publish the same-band shared participant union");

    for (std::size_t index = 0; index < kShotContactSlotCount; ++index) {
        if (index == contact_slot) continue;
        const auto& shot_sample = contacts.shot[index];
        const auto& player_sample = contacts.player.slots[index];
        require(!shot_sample.object_contact && !shot_sample.primary_present &&
                    !shot_sample.secondary_present && !shot_sample.bridge_sprite_present,
                "Shot object participants leaked across a scheduled band boundary");
        require(!player_sample.object_contact && !player_sample.primary_present &&
                    !player_sample.alternate_present && !player_sample.bridge_sprite_present,
                "Aircraft object participants leaked across a scheduled band boundary");
    }

    const auto aircraft_selection = select_player_contact(
        contacts.player, world.object_state());
    require(aircraft_selection.object_contact &&
                aircraft_selection.object_contact->record_index == 0 &&
                aircraft_selection.object_contact->slot == WorldObjectHitSlot::Primary,
            "Aircraft selector did not retain primary priority from the shared latch");

    const auto resolved = resolve_shot_contacts(world, shot, contacts.shot, false);
    require(resolved.selection.outcome == ShotContactOutcome::Ordinary &&
                resolved.selection.slot == ShotContactSlotIndex{
                    static_cast<std::uint8_t>(contact_slot)} &&
                resolved.selection.object == ShotObjectSelection{
                    ShotObjectIndex{0}, ShotObjectRole::Primary} &&
                resolved.object_effect && resolved.object_effect->record_index == 0 &&
                world.object_state().records[0].animated_kind == 2 &&
                world.object_state().records[1].animated_kind == 10 &&
                shot.pose == PlayerShotPose::Hidden,
            "Shot selector did not prioritize and mutate the primary object");
}

RiverViewport lower_terrain_at_vic_200() {
    RiverViewport viewport;
    PackedRiverRow bank{};
    bank[20] = 0xff; // VIC X=160..167; detail occupies X=160..161.
    viewport.push(bank);
    for (int row = 0; row < 200 - 54; ++row) viewport.push({});
    return viewport;
}

std::pair<std::uint8_t, std::uint8_t> lower_primary_placement() {
    const auto aircraft = make_player_contact_mask(FlightPose::Straight);
    constexpr MaskClip direct_clip{0, 182 - 50, 320, 204 - 182};
    constexpr MaskClip after_direct_clip{0, 204 - 50, 320, 211 - 204};
    for (int y = 183; y < 204; ++y) {
        for (int x = 12; x <= 171; ++x) {
            const WorldObjectPlacement placement{
                0, WorldObjectImageSlot::primary, 2 * x - 24, y - 50, 9, 9};
            const auto object = make_world_object_contact_mask(placement);
            if (masks_overlap(aircraft, 2 * 86 - 24, assets::player::fixed_y,
                              object, placement.left, placement.top, direct_clip) &&
                !masks_overlap(aircraft, 2 * 86 - 24, assets::player::fixed_y,
                               object, placement.left, placement.top, after_direct_clip)) {
                return {static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(x)};
            }
        }
    }
    throw std::runtime_error("No lower object placement overlaps the aircraft before cut 204");
}

void test_lower_source_read_ownership() {
    const auto viewport = lower_terrain_at_vic_200();
    const PlayerSteeringState player{86, 0, 0, 0};
    const auto [object_y, object_x] = lower_primary_placement();
    WorldObjectState selected_objects{};
    selected_objects.records[0].animated_kind = 9;
    const auto sample = [&](std::uint8_t phase, std::uint8_t object_y,
                            const PlayerShotResult* shot = nullptr) {
        WorldObjectState objects{};
        objects.active_begin = 0;
        for (auto& record : objects.records) record.vertical_position = 0xff;
        objects.records[0] = {object_y, object_x, 0, 9, 9, 30, 130, 0};
        return detail::sample_contact_scene({
            objects, phase, &viewport, nullptr, nullptr, player,
            FlightPose::Straight, LifeCycleImage::Straight, shot});
    };

    // Phase 20 still schedules and samples section 0 after a complete
    // section-1 read. Terrain at line 200 belongs to section 1.
    const auto regular = sample(20, 205);
    require(regular.player.terrain_contacts[1] && !regular.player.terrain_contacts[0],
            "Phase-20 terrain did not stay in the last regular section");
    require(regular.player.slots[0].primary_object == ShotObjectIndex{0},
            "Phase-20 final scheduled object lost its section-0 mapping");

    for (const auto phase : {21U, 27U}) {
        const auto contacts = sample(static_cast<std::uint8_t>(phase), object_y);
        require(!contacts.player.terrain_contacts[1] &&
                    contacts.player.terrain_contacts[0],
                "Direct lower read incorrectly consumed section-1 terrain");
        require(contacts.player.slots[1].object_contact &&
                    contacts.player.slots[1].primary_object == ShotObjectIndex{0},
                "Direct lower object read missed its mapped section-1 contact");
        require(!contacts.player.slots[0].object_contact,
                "Direct section-1 object latch leaked into the section-0 read");
        require(!contacts.player.slots[0].primary_object,
                "Direct lower section 0 acquired an unscheduled object mapping");
        const auto selected = select_player_contact(contacts.player, selected_objects);
        require(selected.object_contact && selected.object_contact->record_index == 0,
                "Direct lower section-1 object was not selectable");
    }

    for (const auto phase : {28U, 31U}) {
        const auto contacts = sample(static_cast<std::uint8_t>(phase), object_y);
        require(!contacts.player.terrain_contacts[1] &&
                    contacts.player.terrain_contacts[0],
                "Fixed lower read placed terrain in unsampled section 1");
        require(!contacts.player.slots[1].object_contact &&
                    contacts.player.slots[1].primary_object == ShotObjectIndex{0},
                "Fixed path lost section-1 mapping or synthesized its sample");
        require(contacts.player.slots[0].object_contact &&
                    contacts.player.slots[0].primary_present &&
                    !contacts.player.slots[0].primary_object,
                "Fixed lower contact did not remain in unmapped section 0");
        const auto selected = select_player_contact(contacts.player, selected_objects);
        require(!selected.object_contact && selected.terrain_contact,
                "Unmapped fixed-lower object incorrectly displaced terrain selection");

        const PlayerShotResult shot{{200}, PlayerShotPose::Visible,
                                    std::nullopt, std::nullopt};
        const auto with_shot = sample(static_cast<std::uint8_t>(phase), object_y, &shot);
        require(!with_shot.shot[1].terrain_contact &&
                    with_shot.shot[0].terrain_contact &&
                    with_shot.shot[1].primary_object == ShotObjectIndex{0} &&
                    with_shot.shot[0].primary_object.value == kShotContactObjectCount,
                "Fixed lower shot contact acquired the unsampled section-1 mapping");
    }
}

std::pair<std::uint16_t, std::uint8_t> lower_sprite_placement(
    const OccupancyMask& sprite, const RiverViewport& viewport,
    bool require_terrain) {
    const auto aircraft = make_player_contact_mask(FlightPose::Straight);
    const auto terrain = make_river_contact_mask(std::span(viewport.rows()).first(157));
    constexpr MaskClip clip{0, 182 - 50, 320, 203 - 182};
    for (int vic_y = 182; vic_y <= 200; ++vic_y) {
        for (int vic_x = 140; vic_x <= 190; ++vic_x) {
            const auto left = vic_x - 24;
            const auto top = vic_y - 50;
            if (!masks_overlap(sprite, left, top, aircraft, 2 * 86 - 24,
                               assets::player::fixed_y, clip)) continue;
            if (require_terrain &&
                !masks_overlap(sprite, left, top, terrain, 0, 4, clip)) continue;
            return {static_cast<std::uint16_t>(vic_x),
                    static_cast<std::uint8_t>(vic_y)};
        }
    }
    throw std::runtime_error("No lower sprite placement overlaps the required pixels");
}

void test_lower_read_routing_reaches_shared_adapters() {
    const auto viewport = lower_terrain_at_vic_200();
    const PlayerSteeringState player{86, 0, 0, 0};
    WorldObjectState objects{};
    for (auto& record : objects.records) record.vertical_position = 0xff;

    const auto [aux_x, aux_y] = lower_sprite_placement(
        make_auxiliary_projectile_contact_mask(), viewport, true);
    AuxiliaryProjectileState auxiliary{};
    auxiliary.presentation = AuxiliaryProjectilePresentation{aux_x, aux_y};
    const auto [gap_x, gap_y] = lower_sprite_placement(
        make_bridge_sprite_contact_mask(BridgeSpriteImage::GapFlight), viewport, false);
    // This adapter fixture is an explicitly published gap placement; mutable
    // bridge trajectory state is exercised separately in bridge_sprites_test.
    BridgeSpriteState published_bridge{};
    published_bridge.gap = BridgeSpritePresentation{
        BridgeSpriteImage::GapFlight, gap_x, gap_y, 6};
    const PlayerShotResult shot{{200}, PlayerShotPose::Visible,
                                std::nullopt, std::nullopt};

    const auto sample = [&](std::uint8_t phase) {
        return detail::sample_contact_scene({
            objects, phase, &viewport, &published_bridge, &auxiliary, player,
            FlightPose::Straight, LifeCycleImage::Straight, &shot});
    };
    const auto regular = sample(20);
    require(regular.gap.slots[1].gap_present &&
                regular.shot[1].terrain_contact && !regular.shot[0].terrain_contact &&
                regular.auxiliary.player[1] ==
                    (kAuxiliaryParticipant | kPlayerMainParticipant) &&
                regular.auxiliary.terrain[1] == kAuxiliaryParticipant,
            "Phase-20 shared adapters lost the last regular read");

    for (const auto phase : {21U, 27U}) {
        const auto contacts = sample(static_cast<std::uint8_t>(phase));
        require(contacts.gap.slots[1].gap_present &&
                    !contacts.shot[1].terrain_contact &&
                    contacts.shot[0].terrain_contact &&
                    contacts.auxiliary.player[1] ==
                        (kAuxiliaryParticipant | kPlayerMainParticipant),
                "Direct object-only read was not shared with gap and auxiliary");
        require(contacts.auxiliary.terrain[1] == 0 &&
                    contacts.auxiliary.terrain[0] == kAuxiliaryParticipant,
                "Direct object-only read discarded pending auxiliary terrain");
    }

    for (const auto phase : {28U, 31U}) {
        const auto contacts = sample(static_cast<std::uint8_t>(phase));
        require(!contacts.gap.slots[1].gap_present &&
                    contacts.gap.slots[0].gap_present &&
                    !contacts.shot[1].terrain_contact &&
                    contacts.shot[0].terrain_contact &&
                    contacts.auxiliary.player[1] == 0 &&
                    contacts.auxiliary.player[0] ==
                        (kAuxiliaryParticipant | kPlayerMainParticipant),
                "Fixed lower object read did not reach shared section-0 adapters");
        require(contacts.auxiliary.terrain[1] == 0 &&
                    contacts.auxiliary.terrain[0] == kAuxiliaryParticipant,
                "Fixed lower terrain read did not reach the auxiliary adapter");
    }
}

void test_staged_object_generation_and_rebased_publication() {
    WorldObjectState before{};
    before.active_begin = 0;
    for (auto& record : before.records) record.vertical_position = 0xff;
    before.records[0] = {70, 86, 0, 9, 9, 30, 130, 0};
    before.records[1] = {220, 100, 0, 10, 10, 30, 130, 0};

    auto after = before;
    after.records[0].vertical_position = 220;
    after.records[0].animated_kind = 2;
    after.records[1].vertical_position = 95;
    after.records[1].animated_kind = 11;
    const PlayerSteeringState player{86, 0, 0, 0};
    const PlayerShotResult shot{{192}, PlayerShotPose::Visible,
                                std::nullopt, std::nullopt};
    ContactSamplingBuffer samples;
    ContactPresentationMetadata metadata;
    const auto bands = detail::produce_contact_scene(samples, metadata, {
        before, 24, nullptr, nullptr, nullptr, player,
        FlightPose::Straight, LifeCycleImage::Straight, &shot, &after});

    std::size_t top_band = 0;
    const auto top_placement = find_placement(
        bands, 0, WorldObjectImageSlot::primary, top_band);
    std::size_t later_band = 0;
    const auto later_placement = find_placement(
        bands, 1, WorldObjectImageSlot::primary, later_band);
    require(top_band == 0 && top_placement.top == 20 && top_placement.left == 148 &&
                top_placement.image_key == 9 && top_placement.style_key == 9,
            "Top rendered band did not retain its pre-motion object placement and image");
    require(later_band > top_band && later_placement.top == 45 &&
                later_placement.left == 176 && later_placement.image_key == 11 &&
                later_placement.style_key == 10,
            "Later rendered band did not use the post-effect object placement and image");

    const auto& working = samples.working_snapshot().sections;
    require(working[5].primary_object == ShotObjectIndex{0} &&
                working[4].primary_object == ShotObjectIndex{1},
            "Top band did not use pre-motion object while later band used post-motion object");
    require(!working[5].secondary_object && !working[4].secondary_object,
            "Staged fixture unexpectedly assigned a second object in either band");

    // The actual row-shift operation rebases the pending generation before
    // its next foreground publication; decoding must not cache old indices.
    samples.shift_object_mappings();
    require(find_placement(bands, 0, WorldObjectImageSlot::primary, top_band) ==
                top_placement &&
                find_placement(bands, 1, WorldObjectImageSlot::primary, later_band) ==
                later_placement,
            "Row-shift rebasing mutated retained rendered placements");
    samples.rollover_for_foreground();
    const auto published = detail::decode_contact_scene(
        samples.foreground_snapshot(), metadata);
    require(published.shot[5].primary_object == ShotObjectIndex{1} &&
                published.shot[4].primary_object == ShotObjectIndex{2},
            "Published contact adapters ignored row-rebased object mappings");
    require(published.player.slots[5].primary_object == ShotObjectIndex{1} &&
                published.player.slots[4].primary_object == ShotObjectIndex{2},
            "Player adapter did not share the rebased contact generation");
}

} // namespace

int main() {
    try {
        test_shared_participant_latch_preserves_primary_priority();
        test_lower_source_read_ownership();
        test_lower_read_routing_reaches_shared_adapters();
        test_staged_object_generation_and_rebased_publication();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
