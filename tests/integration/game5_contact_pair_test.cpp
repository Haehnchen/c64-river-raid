#include "app/contact_asset_setup.hpp"
#include "fixtures/game5_contact_pair.hpp"
#include "game/contact_sampling.hpp"
#include "game/player_shot.hpp"
#include "game/score.hpp"
#include "render/contact_producer.hpp"
#include "render/world_object_schedule.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace river_raid;
namespace fixture = test_data::game5_contact_pair;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

OccupancyMask captured_mask(const std::array<std::uint8_t, 63>& raw, bool expanded) {
    std::vector<std::uint8_t> roles;
    roles.reserve(24 * 21);
    for (const auto value : raw) {
        for (int shift = 6; shift >= 0; shift -= 2) {
            const auto role = static_cast<std::uint8_t>((value >> shift) & 3);
            roles.push_back(role);
            roles.push_back(role);
        }
    }
    return make_sprite_occupancy_mask(24, 21, roles, SpriteMode::Multicolor, expanded);
}

std::optional<ShotObjectIndex> mapped(std::uint8_t value) {
    return value == 255 ? std::nullopt : std::optional{ShotObjectIndex{value}};
}

void check_geometry(bool native_geometry) {
    WorldObjectState geometry{};
    geometry.records = fixture::producing_records;
    geometry.active_begin = fixture::active_begin;
    if (native_geometry)
        geometry.records[fixture::target_record].horizontal_coordinate = fixture::native_half_x;
    const auto bands = schedule_world_object_snapshot(fixture::generating_phase, geometry);
    WorldObjectPlacement target{};
    std::size_t target_band = bands.size();
    for (std::size_t band = 0; band < bands.size(); ++band) {
        for (const auto& placement : bands[band].placements) {
            if (placement.record_index == fixture::target_record &&
                    placement.image_slot == WorldObjectImageSlot::primary) {
                target = placement;
                target_band = band;
            }
        }
    }
    require(target_band == 2 && bands.size() == 5,
            "Reconstructed target left the expected generating band");
    const auto section = ContactSectionIndex{
        static_cast<std::uint8_t>(kShotContactSlotCount - 1 - target_band)};
    require(section.value == 3 && fixture::published_sections[section.value].primary ==
                fixture::target_record, "Reconstructed section mapping differs from source");
    require(target.left + 24 == (native_geometry ? 230 : 238) && target.top + 50 == 119,
            "Conditional target geometry changed");
    const auto object = captured_mask(fixture::object_raw, fixture::object_expanded);
    const auto projectile = captured_mask(fixture::shot_raw, false);
    const auto product_object = make_world_object_contact_mask(target);
    const auto product_projectile = make_projectile_contact_mask();
    require(object.width == product_object.width && object.height == product_object.height &&
                object.pixels == product_object.pixels, "Product target mask differs from source bytes");
    require(projectile.width == product_projectile.width &&
                projectile.height == product_projectile.height &&
                projectile.pixels == product_projectile.pixels,
            "Product projectile mask differs from source bytes");

    // Only this projectile/primary pair is reconstructed. Other channels below
    // remain captured inputs; this is not a full scene or raster/latch replay.
    const OccupancyMask empty_terrain{1, 1, {0}};
    const std::array sprites{
        ContactSpritePlacement{{&object, target.left + 24, target.top + 50},
                               kPrimaryObjectContactParticipant},
        ContactSpritePlacement{{&projectile, fixture::shot_vic_x, fixture::shot_vic_y},
                               kProjectileContactParticipant}};
    const auto cut = bands[target_band - 1].exclusive_bottom;
    const MaskClip clip{24, cut, 320, bands[target_band].exclusive_bottom - cut};
    require(cut == 109 && clip.height == 32, "Conditional band boundaries changed");
    PixelContactAccumulator producer;
    producer.scan(sprites, {&empty_terrain, 0, 0}, clip);
    const auto pair = producer.pending_sample();
    const auto expected_pair = native_geometry
        ? static_cast<std::uint8_t>(kPrimaryObjectContactParticipant | kProjectileContactParticipant)
        : fixture::published_sections[section.value].objects;
    require(pair.object_participants == expected_pair && pair.terrain_participants == 0,
            native_geometry ? "Native logged geometry does not produce the observed pair"
                            : "Source reconstructed geometry disagrees with the captured zero contact");

    ContactSamplingBuffer buffer;
    for (std::uint8_t index = 0; index < fixture::published_sections.size(); ++index) {
        const auto& source = fixture::published_sections[index];
        const ContactSectionIndex slot{index};
        buffer.replace_sample(slot, {source.objects, source.terrain});
        buffer.replace_object_mapping(slot, ShotObjectRole::Primary, mapped(source.primary));
        buffer.replace_object_mapping(slot, ShotObjectRole::Secondary, mapped(source.alternate));
    }
    buffer.replace_object_participants(section, pair.object_participants);
    buffer.rollover_for_foreground();
    ShotContactSnapshot input{};
    input.slots = make_shot_contact_slots(buffer.foreground_snapshot());
    for (std::size_t index = 0; index < input.object_kinds.size(); ++index)
        input.object_kinds[index] = geometry.records[index].animated_kind;
    const auto selected = select_shot_contact(input);
    auto shot = update_player_shot({fixture::shot_before}, true);
    auto score = fixture::score_before;
    if (selected.clear_projectile) clear_player_shot(shot);
    if (selected.ordinary_effect && selected.ordinary_effect->score_kind_write)
        score = award_object_score(score, *selected.ordinary_effect->score_kind_write).total;
    if (native_geometry) {
        require(selected.outcome == ShotContactOutcome::Ordinary && selected.slot ==
                    ShotContactSlotIndex{3} && selected.object &&
                    selected.object->index == ShotObjectIndex{fixture::target_record} &&
                    selected.object->role == ShotObjectRole::Primary &&
                    selected.ordinary_effect && selected.ordinary_effect->replacement_animated_kind == 2 &&
                    selected.ordinary_effect->animation_count == 6 &&
                    shot.state.vertical_position == 0 && score == 60,
                "Native conditional hit consequence differs from its comparator trace");
    } else {
        require(selected.outcome == ShotContactOutcome::None && !selected.clear_projectile &&
                    shot.state.vertical_position == fixture::shot_after && score == fixture::score_after,
                "Source conditional consumer does not preserve its shot and score");
    }
    std::cout << (native_geometry ? "Native comparison" : "Source conditional")
              << " pair: objects=" << static_cast<unsigned>(pair.object_participants)
              << " shot=" << static_cast<unsigned>(shot.state.vertical_position)
              << " score=" << score << '\n';
}
} // namespace

int main() {
    try {
        check_geometry(false);
        check_geometry(true);
        std::cout << "Two-sprite pass66-to67 component check; reconstructed assignments, measured other "
                     "channels, no normal-session state injection or timing/full-session parity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
