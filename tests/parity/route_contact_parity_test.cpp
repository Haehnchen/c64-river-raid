#include "app/contact_asset_setup.hpp"
#include "app/player_contact.hpp"
#include "support/contact_fixture_assets.hpp"
#include "fixtures/route_contact_cases.hpp"
#include "game/contact_sampling.hpp"
#include "render/contact_producer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace river_raid;
namespace evidence = river_raid::fixtures::route_contact;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t hash(const OccupancyMask& mask) {
    std::uint64_t value = 14695981039346656037ULL;
    for (const auto pixel : mask.pixels) value = (value ^ pixel) * 1099511628211ULL;
    return value;
}

ContactSectionSample produce(const evidence::BankContactCase& item,
                             int sprite_offset, int terrain_offset,
                             int top, int bottom,
                             ContactSamplingBuffer* buffer = nullptr) {
    std::vector<PackedRiverRow> rows;
    rows.reserve(item.height);
    for (unsigned row = 0; row < item.height; ++row) {
        rows.push_back(evidence::bank_contact_rows.at(item.rows.at(row)));
    }
    const auto terrain = make_river_contact_mask(rows);
    require(hash(terrain) == item.terrain_hash, "Captured terrain bytes differ from product mask");

    std::array<OccupancyMask, 2> masks;
    std::array<ContactSpritePlacement, 2> sprites;
    for (unsigned slot = 0; slot < sprites.size(); ++slot) {
        const auto& recorded = item.sprites[slot];
        masks[slot] = test_support::contact_fixture_asset(recorded.kind, 0, 0);
        require(hash(masks[slot]) == recorded.occupancy_hash,
                "Captured aircraft image differs from product asset");
        sprites[slot] = {{&masks[slot], recorded.x, recorded.y + sprite_offset},
                         static_cast<std::uint8_t>(recorded.participant)};
    }
    PixelContactAccumulator producer;
    producer.scan(sprites, {&terrain, 24, item.terrain_y + terrain_offset},
                  {0, top, 512, bottom - top});
    const auto sample = producer.pending_sample();
    require(sample.object_participants == 0 &&
            sample.terrain_participants == item.expected,
            "Computed lower-band participant mask differs from source sample");
    if (buffer) {
        producer.sample_into(*buffer, ContactSectionIndex{0}, ContactSampleChannels::Terrain);
        require(producer.pending_sample() == ContactSectionSample{},
                "Producer retained the consumed lower-band sample");
    }
    return sample;
}

const evidence::BankContactCase& case_at(unsigned event) {
    const auto found = std::find_if(evidence::bank_contact_cases.begin(),
                                    evidence::bank_contact_cases.end(),
                                    [event](const auto& item) { return item.event == event; });
    if (found == evidence::bank_contact_cases.end()) {
        throw std::runtime_error("Missing captured lower-band interval");
    }
    return *found;
}

void verify_geometry() {
    require(evidence::bank_contact_cases.size() == 22,
            "Source lower-band interval count differs");
    std::array<unsigned, 8> coverage{};
    for (const auto& item : evidence::bank_contact_cases) {
        require(item.expected < coverage.size(), "Unexpected participant mask in fixture");
        ++coverage[item.expected];
        for (int sprite_offset : {0, 1}) {
            for (int terrain_offset : {0, 1}) {
                for (int top : {item.top - 2, item.top + 2}) {
                    for (int bottom : {item.bottom - 2, item.bottom + 2}) {
                        produce(item, sprite_offset, terrain_offset, top, bottom);
                    }
                }
            }
        }
    }
    require(coverage[0] && coverage[1] && coverage[5],
            "Captured lower-band negatives and both positive masks are required");
}

void verify_publication_and_terrain_selection() {
    require(!evidence::terrain_contact_lineages.empty(), "No source-positive lineage fixture");
    for (const auto& lineage : evidence::terrain_contact_lineages) {
        require(lineage.sample_event == 822 && lineage.publication_event == 826 &&
                lineage.consumer_event == 837 && lineage.queued_event == 838 &&
                lineage.life_end_event == 876,
                "Source-positive contact lineage anchors differ");
        require(lineage.sample_event < lineage.publication_event &&
                lineage.publication_event < lineage.consumer_event &&
                lineage.consumer_event < lineage.queued_event &&
                lineage.queued_event < lineage.life_end_event,
                "Source contact event order is inconsistent");
        const auto& positive = case_at(lineage.sample_event);
        const evidence::BankContactCase* previous = nullptr;
        for (const auto& item : evidence::bank_contact_cases) {
            if (item.event < lineage.sample_event &&
                (!previous || item.event > previous->event)) previous = &item;
        }
        require(previous != nullptr && previous->expected == lineage.previous_mask,
                "Immediately preceding terrain sample differs from source previous mask");
        require(previous->event == 778,
                "Source-positive contact did not follow the recorded prior lower sample");
        require(positive.expected == lineage.published_mask &&
                (lineage.published_mask & kPlayerDetailParticipant) != 0 &&
                (lineage.previous_mask & kPlayerDetailParticipant) == 0,
                "Source masks do not isolate the detail sprite's positive transition");

        ContactSamplingBuffer buffer;
        require(!make_player_contact_snapshot(buffer.foreground_snapshot()).terrain_contacts[0],
                "Empty contact generation was not empty");
        const auto prior_sample = produce(*previous, 0, 0, previous->top - 2,
                                          previous->bottom + 2, &buffer);
        require(prior_sample.terrain_participants == lineage.previous_mask,
                "Computed preceding sample differs from published source byte");
        require(!make_player_contact_snapshot(buffer.foreground_snapshot()).terrain_contacts[0],
                "Working contact leaked before first rollover");
        buffer.rollover_for_foreground();
        require(buffer.foreground_snapshot().sections[0].sample.terrain_participants ==
                    lineage.previous_mask,
                "Previous contact was not published");
        const auto before = select_player_contact(
            make_player_contact_snapshot(buffer.foreground_snapshot()), WorldObjectState{});
        require(!before.contacted() && !before.refuel_contact,
                "Earlier sprite-0-only terrain mask selected a player contact");

        const auto positive_sample = produce(positive, 0, 0, positive.top - 2,
                                             positive.bottom + 2, &buffer);
        require(positive_sample.terrain_participants == lineage.published_mask,
                "Computed positive sample differs from source lower-band byte");
        require(buffer.foreground_snapshot().sections[0].sample.terrain_participants ==
                    lineage.previous_mask,
                "New working sample leaked into the preceding foreground generation");
        buffer.rollover_for_foreground();
        require(buffer.foreground_snapshot().sections[0].sample.terrain_participants ==
                    lineage.published_mask &&
                buffer.working_snapshot() == ContactSamplingSnapshot{},
                "Positive contact publication or working reset differs");
        const auto published = make_player_contact_snapshot(buffer.foreground_snapshot());
        require(published.terrain_contacts[0] && !published.terrain_contacts[1],
                "Published detail-sprite terrain bit did not reach the player adapter");
        const auto selected = select_player_contact(published, WorldObjectState{});
        require(selected.terrain_contact && !selected.object_contact &&
                !selected.refuel_contact && !selected.bridge,
                "Computed source-positive terrain mask did not select terrain contact");
    }
}
} // namespace

int main() {
    try {
        verify_geometry();
        verify_publication_and_terrain_selection();
        std::cout << evidence::bank_contact_cases.size()
                  << " natural lower intervals, sixteen variants each; "
                  << evidence::terrain_contact_lineages.size()
                  << " delayed terrain-selection lineage(s)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
