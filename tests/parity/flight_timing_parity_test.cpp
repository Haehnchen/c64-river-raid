#include "game/contact_sampling.hpp"
#include "game/shot_contact_resolution.hpp"
#include "app/generator_setup.hpp"
#include "render/world_object_schedule.hpp"
#include "fixtures/flight_timing_cases.hpp"
#include "fixtures/flight_pixel_cases.hpp"
#include "fixtures/flight_terrain_cases.hpp"
#include "fixtures/multiplexed_pixel_cases.hpp"
#include "fixtures/lower_object_pixel_cases.hpp"
#include "fixtures/regular_terrain_cases.hpp"
#include "fixtures/lower_terrain_cases.hpp"
#include "support/contact_fixture_assets.hpp"
#include "render/contact_producer.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace river_raid;
using namespace river_raid::test_data;

std::optional<ShotObjectIndex> mapping(std::uint8_t value) {
    return value == 255 ? std::nullopt : std::optional<ShotObjectIndex>{{value}};
}

void seed(ContactSamplingBuffer& buffer, unsigned offset) {
    for (std::uint8_t section = 0; section < 6; ++section) {
        buffer.replace_sample({section}, {flight_timing_initial[offset+section],
                                         flight_timing_initial[offset+6+section]});
        buffer.replace_object_mapping({section}, ShotObjectRole::Primary,
                                      mapping(flight_timing_initial[offset+12+section]));
        buffer.replace_object_mapping({section}, ShotObjectRole::Secondary,
                                      mapping(flight_timing_initial[offset+18+section]));
    }
}

std::uint64_t state_hash(const ContactSamplingBuffer& buffer) {
    std::uint64_t value = 14695981039346656037ULL;
    for (const auto* state : {&buffer.working_snapshot(), &buffer.foreground_snapshot()}) {
        for (unsigned field = 0; field < 4; ++field) {
            for (const auto& section : state->sections) {
                const auto byte = field == 0 ? section.sample.object_participants
                    : field == 1 ? section.sample.terrain_participants
                    : field == 2 ? section.primary_object.value_or(ShotObjectIndex{255}).value
                                 : section.secondary_object.value_or(ShotObjectIndex{255}).value;
                value = (value ^ byte) * 1099511628211ULL;
            }
        }
    }
    return value;
}

void produce_top(PixelContactAccumulator& producer, std::size_t source) {
    const auto& item = fixtures::flight_pixel_cases.at(source);
    const auto& background = fixtures::flight_terrain_cases.at(source);
    if (background.source != source) throw std::runtime_error("Pixel/terrain fixture join differs");
    std::vector<PackedRiverRow> rows;
    for (unsigned i = 0; i < background.height; ++i) {
        rows.push_back(fixtures::flight_terrain_rows.at(background.rows.at(i)));
    }
    const auto terrain = make_river_contact_mask(rows);
    std::vector<OccupancyMask> masks;
    std::vector<ContactSpritePlacement> sprites;
    masks.reserve(item.count);
    sprites.reserve(item.count);
    for (unsigned i = 0; i < item.count; ++i) {
        const auto& sprite = item.sprites.at(i);
        masks.push_back(test_support::contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
        sprites.push_back({{&masks.back(), sprite.x, sprite.y}, static_cast<std::uint8_t>(sprite.participant)});
    }
    producer.scan(sprites, {&terrain, 24, 54}, {0, 0, 512, item.read_line+2});
}

template<class PixelCase>
void produce_interval(PixelContactAccumulator& producer, const PixelCase& item, MaskPlacement terrain) {
    if constexpr (requires { item.hud_first; }) {
        test_support::verify_hud_tail_bound(item.hud_first, item.hud_opaque_bottom, 1);
    }
    std::vector<OccupancyMask> masks;
    std::vector<ContactSpritePlacement> sprites;
    masks.reserve(item.count);
    sprites.reserve(item.count);
    for (unsigned i = 0; i < item.count; ++i) {
        const auto& sprite = item.sprites.at(i);
        masks.push_back(test_support::contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
        sprites.push_back({{&masks.back(), sprite.x, sprite.y}, static_cast<std::uint8_t>(sprite.participant)});
    }
    producer.scan(sprites, terrain, {0, item.top, 512, item.bottom-item.top});
}

template<class PixelCase>
void produce_object_interval(PixelContactAccumulator& producer, const PixelCase& item) {
    const OccupancyMask empty{1, 1, {0}};
    produce_interval(producer, item, {&empty, 0, 0});
}

void produce_regular_terrain(PixelContactAccumulator& producer, std::size_t source) {
    const auto& background = fixtures::regular_terrain_cases.at(source);
    const auto& item = fixtures::multiplexed_pixel_cases.at(background.source);
    if (background.event != item.event) throw std::runtime_error("Regular terrain event differs");
    std::vector<PackedRiverRow> rows;
    for (unsigned i = 0; i < background.height; ++i) {
        rows.push_back(fixtures::regular_terrain_rows.at(background.rows.at(i)));
    }
    const auto terrain = make_river_contact_mask(rows);
    produce_interval(producer, item, {&terrain, 24, background.terrain_y});
}

void produce_lower_terrain(ContactSamplingBuffer& buffer, std::size_t source) {
    const auto& background = fixtures::lower_terrain_cases.at(source);
    auto item = fixtures::lower_object_pixel_cases.at(background.source);
    if (background.event != item.event || item.section != 0) {
        throw std::runtime_error("Lower terrain event differs");
    }
    item.top = background.top;
    std::vector<PackedRiverRow> rows;
    for (unsigned i = 0; i < background.height; ++i) {
        rows.push_back(fixtures::lower_terrain_rows.at(background.rows.at(i)));
    }
    const auto terrain = make_river_contact_mask(rows);
    PixelContactAccumulator producer;
    produce_interval(producer, item, {&terrain, 24, background.terrain_y});
    producer.sample_into(buffer, {0}, ContactSampleChannels::Terrain);
}
}

int main() {
    try {
        ContactSamplingBuffer buffer;
        PixelContactAccumulator producer;
        seed(buffer, 24);
        buffer.rollover_for_foreground();
        seed(buffer, 0);
        std::array<unsigned, 7> counts{};
        unsigned index = 0;
        std::size_t pixel_source = 0;
        std::size_t multiplexed_source = 0;
        std::size_t lower_source = 0;
        std::size_t regular_terrain_source = 0;
        std::size_t lower_terrain_source = 0;
        std::vector<unsigned> measured_object_events;
        unsigned computed_terrain_samples = 0;
        for (const auto& event : flight_timing_events) {
            using Operation = FlightTimingOperation;
            ++counts.at(static_cast<unsigned>(event.operation));
            const bool lower_sample = lower_source < fixtures::lower_object_pixel_cases.size() &&
                fixtures::lower_object_pixel_cases[lower_source].event == index;
            if (lower_sample) {
                const auto& item = fixtures::lower_object_pixel_cases.at(lower_source++);
                if (item.section != event.section ||
                    (event.operation != Operation::Sample && event.operation != Operation::ObjectsOnly)) {
                    throw std::runtime_error("Lower sample joined to another read");
                }
                produce_object_interval(producer, item);
                producer.sample_into(buffer, {event.section}, ContactSampleChannels::Objects);
            }
            switch (event.operation) {
            case Operation::Checkpoint: break;
            case Operation::Publish: producer.publish_into(buffer); break;
            case Operation::Primary:
                buffer.replace_object_mapping({event.section}, ShotObjectRole::Primary, mapping(event.first));
                break;
            case Operation::Secondary:
                buffer.replace_object_mapping({event.section}, ShotObjectRole::Secondary, mapping(event.first));
                break;
            case Operation::Sample:
                if (lower_sample) {
                    if (lower_source-1 == 86 || lower_source-1 == 94) {
                        // HUD idle-fetch timing remains unresolved for these intervals.
                        buffer.replace_terrain_participants({event.section}, event.second);
                        break;
                    }
                    if (lower_terrain_source >= fixtures::lower_terrain_cases.size() ||
                        fixtures::lower_terrain_cases[lower_terrain_source].event != index ||
                        fixtures::lower_terrain_cases[lower_terrain_source].source != lower_source-1) {
                        throw std::runtime_error("Lower terrain interval lacks its native sample");
                    }
                    produce_lower_terrain(buffer, lower_terrain_source++);
                    ++computed_terrain_samples;
                } else if (pixel_source < fixtures::flight_pixel_cases.size() &&
                    fixtures::flight_pixel_cases[pixel_source].event == index) {
                    if (event.section != 5) throw std::runtime_error("Pixel sample joined to another section");
                    produce_top(producer, pixel_source++);
                    producer.sample_into(buffer, {event.section});
                    ++computed_terrain_samples;
                } else if (multiplexed_source < fixtures::multiplexed_pixel_cases.size() &&
                           fixtures::multiplexed_pixel_cases[multiplexed_source].event == index) {
                    if (regular_terrain_source >= fixtures::regular_terrain_cases.size() ||
                        fixtures::regular_terrain_cases[regular_terrain_source].event != index ||
                        fixtures::regular_terrain_cases[regular_terrain_source].source != multiplexed_source) {
                        throw std::runtime_error("Regular terrain interval lacks its native sample");
                    }
                    produce_regular_terrain(producer, regular_terrain_source++);
                    producer.sample_into(buffer, {event.section});
                    ++computed_terrain_samples;
                    ++multiplexed_source;
                } else {
                    // Other intervals still use measured samples.
                    buffer.replace_sample({event.section}, {event.first, event.second});
                    measured_object_events.push_back(index);
                }
                break;
            case Operation::ObjectsOnly:
                if (!lower_sample) {
                    buffer.replace_object_participants({event.section}, event.first);
                    measured_object_events.push_back(index);
                }
                break;
            case Operation::HistoryShift: buffer.shift_object_mappings(); break;
            }
            if (state_hash(buffer) != event.expected_hash) {
                throw std::runtime_error("Contact timeline differs at event " + std::to_string(index));
            }
            for (const auto& hit : flight_timing_hits) {
                if (hit.consume_event != index) continue;
                WorldObjectState state{};
                state.records = hit.before;
                constexpr std::array<std::uint8_t, 16> delays{}, clearance{};
                constexpr std::array<std::uint8_t, 8> choices{};
                RiverWorld world(make_river_generator(), WorldObjectHistory(state, {delays, choices, clearance}),
                                 generator_edge_patterns());
                PlayerShotResult shot{{hit.shot_before}, PlayerShotPose::Visible, std::nullopt, std::nullopt};
                const auto result = resolve_shot_contacts(world, shot,
                    make_shot_contact_slots(buffer.foreground_snapshot()), hit.score_pending != 0);
                if (!result.object_effect || world.object_state().records != hit.after ||
                    shot.state.vertical_position != hit.shot_after ||
                    result.object_effect->score_kind.value_or(hit.score_before) != hit.score_after ||
                    result.object_effect->sound_countdown != hit.sound_after) {
                    throw std::runtime_error("Continuous contact lineage selected the wrong hit at " + std::to_string(index));
                }
            }
            ++index;
        }
        for (const auto count : counts) {
            if (count == 0) throw std::runtime_error("Missing contact-timeline operation coverage");
        }
        if (pixel_source != fixtures::flight_pixel_cases.size()) {
            throw std::runtime_error("Not all native pixel samples reached the timeline");
        }
        if (multiplexed_source != fixtures::multiplexed_pixel_cases.size()) {
            throw std::runtime_error("Not all multiplexed samples reached the timeline");
        }
        if (lower_source != fixtures::lower_object_pixel_cases.size()) {
            throw std::runtime_error("Not all lower samples reached the timeline");
        }
        if (regular_terrain_source != fixtures::regular_terrain_cases.size() ||
            lower_terrain_source != fixtures::lower_terrain_cases.size() ||
            computed_terrain_samples != pixel_source + regular_terrain_source + lower_terrain_source) {
            throw std::runtime_error("Not all terrain samples reached the timeline");
        }
        // Only the first sample lacks its preceding captured interval.
        if (measured_object_events != std::vector<unsigned>{6}) {
            throw std::runtime_error("A complete object interval still uses a measured mask");
        }
        auto cursor = static_cast<std::size_t>(flight_timing_bands.front().cursor);
        for (const auto& band : flight_timing_bands) {
            if (band.section == 5) cursor = band.active_begin;
            if (cursor != band.cursor) throw std::runtime_error("Cross-band cursor differs");
            WorldObjectState state{};
            state.records = band.records;
            const auto scheduled = schedule_world_object_band(state, cursor, band.cut,
                band.alternate_only ? WorldObjectBandSlots::alternate_only
                                    : WorldObjectBandSlots::primary_then_alternate);
            if (scheduled.next_record != band.next_cursor ||
                scheduled.placements.size() != band.assignment_count) {
                throw std::runtime_error("Band scheduling differs at event " + std::to_string(band.entry_event));
            }
            for (unsigned i = 0; i < band.assignment_count; ++i) {
                if (scheduled.placements[i].record_index != band.assignments[i].record ||
                    static_cast<unsigned>(scheduled.placements[i].image_slot) != band.assignments[i].role) {
                    throw std::runtime_error("Natural assignment selected another record or role");
                }
            }
            cursor = scheduled.next_record;
        }
        std::cout << index << " continuous contact-buffer checkpoints and "
                  << flight_timing_hits.size() << " ordinary hits; "
                  << flight_timing_bands.size() << " native bands, " << pixel_source << " full pixel samples, "
                  << multiplexed_source << " regular and " << lower_source << " lower object samples match; "
                  << measured_object_events.size() << " measured object inputs remain; "
                  << computed_terrain_samples << " computed terrain samples\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
