#include "app/contact_asset_setup.hpp"
#include "support/contact_fixture_assets.hpp"
#include "render/contact_producer.hpp"
#include "fixtures/flight_pixel_cases.hpp"
#include "fixtures/flight_terrain_cases.hpp"
#include "fixtures/bank_contact_cases.hpp"
#include "fixtures/multiplexed_pixel_cases.hpp"
#include "fixtures/lower_object_pixel_cases.hpp"
#include "fixtures/regular_terrain_cases.hpp"
#include "fixtures/lower_terrain_cases.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace river_raid;

using test_support::contact_fixture_asset;

std::uint64_t hash(const OccupancyMask& mask) {
    std::uint64_t value = 14695981039346656037ULL;
    for (auto pixel : mask.pixels) value = (value ^ pixel) * 1099511628211ULL;
    return value;
}

void verify_cases() {
    for (const auto& item : fixtures::flight_pixel_cases) {
        for (int offset : {0, 1}) {
            for (int bottom : {item.read_line-1, item.read_line+2}) {
                std::vector<OccupancyMask> masks;
                std::vector<ContactSpritePlacement> sprites;
                masks.reserve(item.count);
                sprites.reserve(item.count);
                for (unsigned i = 0; i < item.count; ++i) {
                    const auto& sprite = item.sprites[i];
                    masks.push_back(contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
                    const auto& mask = masks.back();
                    if (mask.width != sprite.width || mask.height != sprite.height ||
                        hash(mask) != sprite.occupancy_hash) {
                        throw std::runtime_error("Product occupancy differs from captured sprite bytes");
                    }
                    sprites.push_back({{&mask, sprite.x, sprite.y+offset},
                                       static_cast<std::uint8_t>(sprite.participant)});
                }
                const OccupancyMask empty{1, 1, {0}};
                PixelContactAccumulator producer;
                producer.scan(sprites, {&empty, 0, 0}, {0, 0, 512, bottom});
                if (producer.consume_object_participants() != item.expected) {
                    throw std::runtime_error("Top-band contact differs at event " + std::to_string(item.event));
                }
                if (producer.consume_object_participants() != 0) {
                    throw std::runtime_error("Object latch did not clear");
                }
            }
        }
    }
    std::cout << fixtures::flight_pixel_cases.size() << " top-band cases, four boundary variants each\n";
}

void verify_terrain_cases() {
    for (const auto& terrain_case : fixtures::flight_terrain_cases) {
        const auto& item = fixtures::flight_pixel_cases.at(terrain_case.source);
        std::vector<PackedRiverRow> rows;
        for (unsigned i = 0; i < terrain_case.height; ++i) {
            rows.push_back(fixtures::flight_terrain_rows.at(terrain_case.rows.at(i)));
        }
        const auto terrain = make_river_contact_mask(rows);
        if (hash(terrain) != terrain_case.occupancy_hash) {
            throw std::runtime_error("Terrain foreground differs from captured rows");
        }
        for (int sprite_offset : {0, 1}) {
            std::vector<OccupancyMask> masks;
            std::vector<ContactSpritePlacement> sprites;
            masks.reserve(item.count);
            sprites.reserve(item.count);
            for (unsigned i = 0; i < item.count; ++i) {
                const auto& sprite = item.sprites.at(i);
                masks.push_back(contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
                sprites.push_back({{&masks.back(), sprite.x, sprite.y+sprite_offset},
                                   static_cast<std::uint8_t>(sprite.participant)});
            }
            for (int terrain_top : {54, 55}) {
                for (int bottom : {item.read_line-1, item.read_line+2}) {
                    PixelContactAccumulator producer;
                    producer.scan(sprites, {&terrain, 24, terrain_top}, {0, 0, 512, bottom});
                    if (producer.consume_terrain_participants() != terrain_case.expected ||
                        producer.consume_object_participants() != item.expected ||
                        producer.consume_terrain_participants() != 0) {
                        throw std::runtime_error("Terrain contact differs at event " + std::to_string(item.event));
                    }
                }
            }
        }
    }
    std::cout << fixtures::flight_terrain_cases.size() << " natural terrain negatives, eight variants each\n";
}

template<class Backgrounds, class Rows, class Objects>
void verify_terrain_intervals(const Backgrounds& backgrounds, const Rows& terrain_rows,
                              const Objects& objects, std::string_view label) {
    for (const auto& background : backgrounds) {
        auto item = objects.at(background.source);
        if (background.event != item.event) throw std::runtime_error("Terrain interval event differs");
        if constexpr (requires { background.top; }) item.top = background.top;
        if constexpr (requires { item.hud_first; }) {
            if (item.section != 0) throw std::runtime_error("Lower terrain joined to object-only read");
            test_support::verify_hud_tail_bound(item.hud_first, item.hud_opaque_bottom, 1);
        }
        std::vector<PackedRiverRow> rows;
        for (unsigned i = 0; i < background.height; ++i) {
            rows.push_back(terrain_rows.at(background.rows.at(i)));
        }
        const auto terrain = make_river_contact_mask(rows);
        if (hash(terrain) != background.occupancy_hash) throw std::runtime_error("Regular foreground differs");
        for (int sprite_offset : {0, 1}) {
            std::vector<OccupancyMask> masks;
            std::vector<ContactSpritePlacement> sprites;
            masks.reserve(item.count);
            sprites.reserve(item.count);
            for (unsigned i = 0; i < item.count; ++i) {
                const auto& sprite = item.sprites.at(i);
                masks.push_back(contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
                if (hash(masks.back()) != sprite.occupancy_hash) {
                    throw std::runtime_error("Regular sprite occupancy differs");
                }
                sprites.push_back({{&masks.back(), sprite.x, sprite.y+sprite_offset},
                                   static_cast<std::uint8_t>(sprite.participant)});
            }
            for (int terrain_offset : {0, 1}) {
                for (int top : {item.top-1, item.top+2}) {
                    for (int bottom : {item.bottom-1, item.bottom+2}) {
                        PixelContactAccumulator producer;
                        producer.scan(sprites, {&terrain, 24, background.terrain_y+terrain_offset},
                                      {0, top, 512, bottom-top});
                        if (producer.pending_sample() != ContactSectionSample{
                                static_cast<std::uint8_t>(item.expected),
                                static_cast<std::uint8_t>(background.expected)}) {
                            throw std::runtime_error("Regular terrain contact differs at " + std::to_string(item.event));
                        }
                    }
                }
            }
        }
    }
    std::cout << backgrounds.size() << ' ' << label << " terrain intervals, sixteen variants each\n";
}

void verify_bank_cases() {
    std::array<unsigned, 8> coverage{};
    for (const auto& item : fixtures::bank_contact_cases) {
        ++coverage.at(item.expected);
        std::vector<PackedRiverRow> rows;
        for (unsigned i = 0; i < item.height; ++i) rows.push_back(fixtures::bank_contact_rows.at(item.rows.at(i)));
        const auto terrain = make_river_contact_mask(rows);
        if (hash(terrain) != item.terrain_hash) throw std::runtime_error("Bank foreground hash differs");
        for (int sprite_offset : {0, 1}) {
            std::array<OccupancyMask, 2> masks;
            std::array<ContactSpritePlacement, 2> sprites;
            for (unsigned i = 0; i < 2; ++i) {
                const auto& sprite = item.sprites[i];
                masks[i] = contact_fixture_asset(sprite.kind, 0, 0);
                if (hash(masks[i]) != sprite.occupancy_hash) throw std::runtime_error("Aircraft mask hash differs");
                sprites[i] = {{&masks[i], sprite.x, sprite.y+sprite_offset},
                              static_cast<std::uint8_t>(sprite.participant)};
            }
            for (int terrain_offset : {0, 1}) {
                for (int top : {item.top-2, item.top+2}) {
                    for (int bottom : {item.bottom-2, item.bottom+2}) {
                        PixelContactAccumulator producer;
                        producer.scan(sprites, {&terrain, 24, item.terrain_y+terrain_offset},
                                      {0, top, 512, bottom-top});
                        if (producer.consume_terrain_participants() != item.expected ||
                            producer.consume_object_participants() != 0 ||
                            producer.consume_terrain_participants() != 0) {
                            throw std::runtime_error("Bank contact differs at event " + std::to_string(item.event));
                        }
                    }
                }
            }
        }
    }
    if (!coverage[0] || !coverage[1] || !coverage[5]) throw std::runtime_error("Missing bank contact coverage");
    std::cout << fixtures::bank_contact_cases.size() << " natural aircraft bank intervals, sixteen variants each\n";
}

template<class Cases>
void verify_object_intervals(const Cases& cases) {
    for (const auto& item : cases) {
        for (int offset : {0, 1}) {
            if constexpr (requires { item.hud_first; }) {
                test_support::verify_hud_tail_bound(item.hud_first, item.hud_opaque_bottom, offset);
            }
            std::vector<OccupancyMask> masks;
            std::vector<ContactSpritePlacement> sprites;
            masks.reserve(item.count);
            sprites.reserve(item.count);
            for (unsigned i = 0; i < item.count; ++i) {
                const auto& sprite = item.sprites.at(i);
                masks.push_back(contact_fixture_asset(sprite.kind, sprite.image, sprite.style));
                const auto& mask = masks.back();
                if (hash(mask) != sprite.occupancy_hash || mask.width != sprite.width || mask.height != sprite.height) {
                    throw std::runtime_error("Object interval mask differs from captured bytes");
                }
                sprites.push_back({{&mask, sprite.x, sprite.y+offset}, static_cast<std::uint8_t>(sprite.participant)});
            }
            for (int top : {item.top-1, item.top+2}) {
                for (int bottom : {item.bottom-1, item.bottom+2}) {
                    const OccupancyMask empty{1, 1, {0}};
                    PixelContactAccumulator producer;
                    producer.scan(sprites, {&empty, 0, 0}, {0, top, 512, bottom-top});
                    if (producer.consume_object_participants() != item.expected) {
                        throw std::runtime_error("Object interval differs at event " + std::to_string(item.event));
                    }
                }
            }
        }
    }
    std::cout << cases.size() << " object intervals, eight variants each\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
            verify_cases();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-terrain") {
            verify_terrain_cases();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-regular-terrain") {
            verify_terrain_intervals(fixtures::regular_terrain_cases, fixtures::regular_terrain_rows,
                                     fixtures::multiplexed_pixel_cases, "regular");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-lower-terrain") {
            verify_terrain_intervals(fixtures::lower_terrain_cases, fixtures::lower_terrain_rows,
                                     fixtures::lower_object_pixel_cases, "lower");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-bank") {
            verify_bank_cases();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-multiplexed") {
            verify_object_intervals(fixtures::multiplexed_pixel_cases);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--self-test-lower-objects") {
            test_support::verify_hud_tail_bound(-1, -1, 0);
            test_support::verify_hud_tail_bound(211, 209, 1);
            for (const auto bounds : {std::array{211, 210}, std::array{-1, 202},
                                      std::array{211, -1}, std::array{312, 202}}) {
                bool rejected = false;
                try { test_support::verify_hud_tail_bound(bounds[0], bounds[1], 1); }
                catch (const std::invalid_argument&) { rejected = true; }
                if (!rejected) throw std::runtime_error("Unsafe HUD bound accepted");
            }
            verify_object_intervals(fixtures::lower_object_pixel_cases);
            return 0;
        }
        const bool with_terrain = argc == 2 && std::string_view(argv[1]) == "--terrain";
        if (argc != 1 && !with_terrain) throw std::invalid_argument("Unknown trace mode");
        int top, bottom;
        unsigned count;
        while (std::cin >> top >> bottom >> count) {
            if (count > 8 || top < 0 || bottom > 312 || bottom < top) {
                throw std::invalid_argument("Invalid scan interval");
            }
            std::vector<OccupancyMask> masks;
            std::vector<ContactSpritePlacement> sprites;
            masks.reserve(count);
            sprites.reserve(count);
            for (unsigned i = 0; i < count; ++i) {
                unsigned kind, image, style, participant;
                int left, y;
                if (!(std::cin >> kind >> image >> style >> left >> y >> participant) || participant > 255) {
                    throw std::invalid_argument("Invalid sprite input");
                }
                masks.push_back(test_support::contact_fixture_asset(kind, image, style));
                sprites.push_back({{&masks.back(), left, y}, static_cast<std::uint8_t>(participant)});
            }
            OccupancyMask terrain{1, 1, {0}};
            int terrain_x = 0, terrain_y = 0;
            if (with_terrain) {
                unsigned height;
                if (!(std::cin >> terrain_x >> terrain_y >> height) || height == 0 || height > 200) {
                    throw std::invalid_argument("Invalid terrain input");
                }
                std::vector<PackedRiverRow> rows(height);
                for (auto& row : rows) {
                    for (auto& byte : row) {
                        unsigned value;
                        if (!(std::cin >> value) || value > 255) throw std::invalid_argument("Invalid terrain byte");
                        byte = static_cast<std::uint8_t>(value);
                    }
                }
                terrain = make_river_contact_mask(rows);
            }
            PixelContactAccumulator producer;
            producer.scan(sprites, {&terrain, terrain_x, terrain_y}, {0, top, 512, bottom-top});
            std::cout << unsigned(producer.pending_sample().object_participants);
            if (with_terrain) std::cout << ' ' << unsigned(producer.pending_sample().terrain_participants)
                                       << ' ' << hash(terrain);
            for (const auto& mask : masks) std::cout << ' ' << mask.width << ' ' << mask.height << ' ' << hash(mask);
            std::cout << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
