#include "app/contact_asset_setup.hpp"
#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "app/generator_setup.hpp"
#include "app/player_contact.hpp"
#include "player_data.hpp"

#include <array>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool terrain_overlap(const OccupancyMask& aircraft, int left, const OccupancyMask& terrain) {
    for (int y = 0; y < aircraft.height; ++y) {
        for (int x = 0; x < aircraft.width; ++x) {
            if (aircraft.occupied_at(x, y) && terrain.occupied_at(
                    left + x, assets::player::fixed_y + y - 4)) return true;
        }
    }
    return false;
}

RiverWorld object_world(std::uint8_t kind, std::uint8_t horizontal,
                       WorldObjectHitSlot slot = WorldObjectHitSlot::Primary) {
    static constexpr std::array<std::uint8_t, 16> delays{}, clearance{};
    static constexpr std::array<std::uint8_t, 8> choices{};
    WorldObjectState state{};
    state.active_begin = 0;
    const auto index = slot == WorldObjectHitSlot::Primary ? 0 : 1;
    if (index != 0) state.records[0].vertical_position = 191;
    state.records[index] = {191, horizontal, 0, kind, kind, 30, 130, 0};
    return {make_river_generator(), WorldObjectHistory(state, {delays, choices, clearance}),
            generator_edge_patterns()};
}
}

int main() {
    try {
        for (unsigned bits = 0; bits < 256; ++bits) {
            ContactSamplingSnapshot published;
            for (unsigned index = 0; index < published.sections.size(); ++index) {
                published.sections[index] = {
                    {static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits ^ 0xFF)},
                    ShotObjectIndex{static_cast<std::uint8_t>(index)},
                    ShotObjectIndex{static_cast<std::uint8_t>(index + 6)}};
            }
            const auto decoded = make_player_contact_snapshot(published);
            require(!decoded.player_fully_offscreen,
                    "Participant decoding invented an offscreen policy result");
            for (unsigned index = 0; index < decoded.slots.size(); ++index) {
                const auto& slot = decoded.slots[index];
                require(slot.primary_object == published.sections[index].primary_object &&
                            slot.alternate_object == published.sections[index].secondary_object,
                        "Participant decoding changed object mappings");
                require(slot.primary_present == bool(bits & 0x80) &&
                            slot.alternate_present == bool(bits & 0x40) &&
                            slot.object_contact == (bool(bits & 1) && bool(bits & 0xC0)) &&
                            slot.bridge_sprite_present == bool(bits & 0x20) &&
                            slot.auxiliary_projectile_present == bool(bits & 8),
                        "Published participant roles differ");
            }
            require(decoded.terrain_contacts[0] == bool((bits ^ 0xFF) & 4) &&
                        decoded.terrain_contacts[1] == bool((bits ^ 0xFF) & 4),
                    "Published terrain did not use the aircraft detail participant");
        }
        ContactSamplingBuffer publication;
        publication.replace_sample({0}, {0, kPlayerMainParticipant});
        publication.rollover_for_foreground();
        publication.replace_sample({0}, {0, kPlayerDetailParticipant});
        require(!make_player_contact_snapshot(publication.foreground_snapshot()).terrain_contacts[0],
                "Working terrain contact escaped before publication");
        publication.rollover_for_foreground();
        require(make_player_contact_snapshot(publication.foreground_snapshot()).terrain_contacts[0] &&
                    !make_player_contact_snapshot(publication.working_snapshot()).terrain_contacts[0],
                "Terrain publication failed to retain foreground and clear working state");
        require(!make_player_contact_snapshot(publication.foreground_snapshot()).slots[0].primary_object,
                "Empty published mapping became an object");
        for (unsigned kind = 0; kind < 16; ++kind) {
            const auto expected = kind < 7 ? PlayerObjectContactEffect::Ignore
                : kind < 14 ? PlayerObjectContactEffect::Crash
                : kind == 14 ? PlayerObjectContactEffect::Bridge : PlayerObjectContactEffect::Refuel;
            require(player_object_contact_effect(kind) == expected, "Object damage classification differs");
        }
        FlightPreview flight;
        flight.start();
        river_raid::test::wait_for_launch(flight);
        const auto terrain = make_river_contact_mask(std::span(flight.world().viewport().rows()).first(157));
        const auto detail = make_player_detail_contact_mask();
        bool saw_main_only = false, saw_terrain = false;
        for (const auto pose : {FlightPose::Straight, FlightPose::Left, FlightPose::Right}) {
            const auto main = make_player_contact_mask(pose);
            for (unsigned x = 12; x <= 160; ++x) {
                auto player = flight.steering();
                player.horizontal_position = x;
                const auto left = 2 * static_cast<int>(x) - 24;
                const auto expected = terrain_overlap(detail, left, terrain);
                const auto contact = sample_player_contact(flight.world(), player, pose);
                require(contact.terrain_contact == expected, "Terrain damage did not use the detail mask");
                saw_terrain |= expected;
                saw_main_only |= !expected && terrain_overlap(main, left, terrain);
            }
        }
        require(saw_main_only && saw_terrain, "Terrain test missed a damaging or main-only overlap");
        auto outside = flight.steering();
        outside.horizontal_position = 255;
        require(sample_player_contact(flight.world(), outside, FlightPose::Straight).terrain_contact,
                "Offscreen aircraft escaped the playfield");

        bool enemy_hit = false, refuel_hit = false, bridge_hit = false;
        for (unsigned x = 70; x <= 100; ++x) {
            enemy_hit |= sample_player_contact(object_world(9, x), flight.steering(),
                                               FlightPose::Straight).object_contact.has_value();
            const auto depot = sample_player_contact(object_world(15, x), flight.steering(),
                                                     FlightPose::Straight);
            refuel_hit |= depot.refuel_contact;
            const auto bridge = sample_player_contact(object_world(14, x), flight.steering(),
                                                      FlightPose::Straight);
            if (bridge.bridge) {
                bridge_hit = true;
                require(bridge.contacted() && bridge.bridge->record_index == 0,
                        "Bridge contact lost damage or object identity");
            }
            require(!sample_player_contact(object_world(9, x), flight.steering(),
                                            FlightPose::Straight).refuel_contact,
                    "Enemy overlap refueled aircraft");
            for (const auto kind : {0, 2, 4, 14, 15}) {
                require(!sample_player_contact(object_world(kind, x), flight.steering(),
                                                FlightPose::Straight).object_contact,
                        "Non-enemy overlap caused generic aircraft damage");
            }
        }
        require(enemy_hit, "Enemy overlap never damaged the aircraft");
        require(refuel_hit, "Depot overlap never supplied refuel contact");
        require(bridge_hit, "Bridge overlap never supplied a destructive contact");
        for (const auto slot : {WorldObjectHitSlot::Primary, WorldObjectHitSlot::Alternate}) {
            for (std::uint8_t kind = 7; kind < 14; ++kind) {
                bool hit = false;
                for (unsigned x = 70; x <= 100; ++x) {
                    auto world = object_world(kind, x, slot);
                    const auto contact = sample_player_contact(world, flight.steering(),
                                                               FlightPose::Straight);
                    if (!contact.object_contact) continue;
                    hit = true;
                    const auto& enemy = *contact.object_contact;
                    const auto index = slot == WorldObjectHitSlot::Primary ? 0U : 1U;
                    require(enemy.record_index == index && enemy.slot == slot &&
                                enemy.animated_kind == kind,
                            "Aircraft contact lost enemy identity, sprite slot or consumed kind");
                    const auto effect = world.apply_projectile_hit(
                        enemy.record_index, enemy.slot, true);
                    const auto& record = world.object_state().records[index];
                    require(effect && !effect->score_kind &&
                                record.animated_kind == (index == 0 ? 2 : 4) &&
                                record.animation_count == 6,
                            "Aircraft hit lost its unscored slot-specific explosion");
                    require(!sample_player_contact(world, flight.steering(),
                                                    FlightPose::Straight).object_contact,
                            "Exploding enemy damaged the aircraft again");
                }
                require(hit, "Enemy kind/slot fixture missed aircraft contact");
            }
        }
        auto destroyed_bridge = object_world(14, 86);
        const auto bridge_effect = destroyed_bridge.apply_projectile_hit(
            0, WorldObjectHitSlot::Primary, true);
        require(bridge_effect && !bridge_effect->score_kind,
                "Aircraft bridge destruction awarded points");
        const auto aftermath = sample_player_contact(destroyed_bridge, flight.steering(),
                                                     FlightPose::Straight);
        require(!aftermath.bridge && !aftermath.object_contact,
                "Destroyed bridge still damaged the aircraft");
        require(!sample_player_contact(object_world(15, 1), flight.steering(),
                                       FlightPose::Straight).refuel_contact,
                "Distant depot supplied refuel contact");
        auto destroyed_depot = object_world(15, 86);
        require(destroyed_depot.apply_projectile_hit(0, WorldObjectHitSlot::Primary, false).has_value(),
                "Depot could not be destroyed");
        require(!sample_player_contact(destroyed_depot, flight.steering(),
                                       FlightPose::Straight).refuel_contact,
                "Destroyed depot still supplied refuel contact");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
