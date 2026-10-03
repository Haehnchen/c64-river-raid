#include "render/contact_producer.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using river_raid::ContactSectionSample;
using river_raid::ContactSpritePlacement;
using river_raid::MaskClip;
using river_raid::MaskPlacement;
using river_raid::OccupancyMask;
using river_raid::PixelContactAccumulator;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] OccupancyMask mask(int width, int height,
                                 std::initializer_list<std::uint8_t> pixels) {
    return {width, height, std::vector<std::uint8_t>(pixels)};
}

[[nodiscard]] ContactSpritePlacement sprite(const OccupancyMask& occupancy,
                                             int left, int top,
                                             std::uint8_t participant) {
    return {{&occupancy, left, top}, participant};
}

void test_exact_overlap_and_clipping() {
    const auto solid = mask(2, 2, {1, 1, 1, 1});
    const auto corners = mask(2, 2, {1, 0, 0, 1});
    const auto terrain = mask(4, 3, {
        0, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0, 0,
    });
    const std::array placements{
        sprite(solid, -1, 0, 0x01),
        sprite(corners, 0, 0, 0x80),
        sprite(corners, 3, 1, 0x04),
    };

    PixelContactAccumulator accumulator;
    accumulator.scan(placements, {&terrain, 0, 0}, {1, 0, 1, 1});
    require(accumulator.pending_sample() == ContactSectionSample{},
            "Pixels outside the scan clip produced contact");

    accumulator.scan(placements, {&terrain, 0, 0}, {0, 0, 4, 3});
    require(accumulator.pending_sample() == ContactSectionSample{0x81, 0x80},
            "Clipped negative placement produced the wrong contact union");

    const std::array reversed{placements[1], placements[0]};
    PixelContactAccumulator reversed_accumulator;
    reversed_accumulator.scan(reversed, {&terrain, 0, 0}, {0, 0, 4, 3});
    require(reversed_accumulator.pending_sample() ==
                accumulator.pending_sample(),
            "Sprite order changed the contact result");
}

void test_shared_multicolor_is_sprite_opaque_only() {
    constexpr std::array<std::uint8_t, 1> shared_role{1};
    const auto shared_sprite = river_raid::make_sprite_occupancy_mask(
        1, 1, shared_role, river_raid::SpriteMode::Multicolor);
    // First pair is role 01 (shared background color), the other pairs are 00.
    constexpr std::array<std::uint8_t, 1> packed_terrain{0x40};
    const auto terrain = river_raid::make_terrain_occupancy_mask(
        packed_terrain, 1);
    const std::array placements{
        sprite(shared_sprite, 0, 0, 0x01),
        sprite(shared_sprite, 0, 0, 0x02),
    };

    PixelContactAccumulator accumulator;
    accumulator.scan(placements, {&terrain, 0, 0}, {0, 0, 1, 1});
    require(accumulator.pending_sample() == ContactSectionSample{0x03, 0x00},
            "Shared color role was not opaque only on the sprite side");
}

void test_distinct_pairs_form_one_global_mask() {
    const auto pixel = mask(1, 1, {1});
    const auto terrain = mask(4, 1, {0, 0, 0, 0});
    const std::array placements{
        sprite(pixel, 0, 0, 0x01),
        sprite(pixel, 0, 0, 0x02),
        sprite(pixel, 3, 0, 0x04),
        sprite(pixel, 3, 0, 0x08),
    };

    PixelContactAccumulator accumulator;
    accumulator.scan(placements, {&terrain, 0, 0}, {0, 0, 4, 1});
    require(accumulator.pending_sample() == ContactSectionSample{0x0F, 0x00},
            "Independent sprite pairs did not join the global participant mask");
}

void test_accumulation_and_independent_consumption() {
    const auto pixel = mask(1, 1, {1});
    const auto terrain_hit = mask(1, 1, {1});
    const std::array first{
        sprite(pixel, 0, 0, 0x01),
        sprite(pixel, 0, 0, 0x02),
    };
    const std::array second{
        sprite(pixel, 0, 0, 0x40),
        sprite(pixel, 0, 0, 0x80),
    };

    PixelContactAccumulator accumulator;
    accumulator.scan(first, {&terrain_hit, 0, 0}, {0, 0, 1, 1});
    accumulator.scan(second, {&terrain_hit, 2, 0}, {0, 0, 1, 1});
    require(accumulator.pending_sample() == ContactSectionSample{0xC3, 0x03},
            "Successive scans did not accumulate both global latches");

    require(accumulator.consume_object_participants() == 0xC3,
            "Object participant read returned the wrong mask");
    require(accumulator.pending_sample() == ContactSectionSample{0x00, 0x03},
            "Object participant read cleared the terrain channel");

    accumulator.scan(second, {&terrain_hit, 0, 0}, {0, 0, 1, 1});
    require(accumulator.pending_sample() == ContactSectionSample{0xC0, 0xC3},
            "A cleared channel did not begin a new accumulation period");
    require(accumulator.consume_terrain_participants() == 0xC3,
            "Terrain participant read returned the wrong mask");
    require(accumulator.pending_sample() == ContactSectionSample{0xC0, 0x00},
            "Terrain participant read cleared the object channel");
}

void test_extreme_coordinates_do_not_overflow_scan_bounds() {
    const auto pixel = mask(1, 1, {1});
    const auto terrain = mask(1, 1, {1});
    constexpr auto maximum = std::numeric_limits<int>::max();
    constexpr auto minimum = std::numeric_limits<int>::min();
    const std::array placements{
        sprite(pixel, maximum, maximum, 0x01),
        sprite(pixel, maximum, maximum, 0x02),
    };

    PixelContactAccumulator accumulator;
    accumulator.scan(placements, {&terrain, maximum, maximum},
                     {maximum, maximum, maximum, maximum});
    require(accumulator.pending_sample() == ContactSectionSample{0x03, 0x03},
            "Positive extreme coordinates overflowed contact bounds");

    const std::array negative{
        sprite(pixel, minimum, minimum, 0x04),
        sprite(pixel, minimum, minimum, 0x08),
    };
    accumulator.scan(negative, {&terrain, minimum, minimum},
                     {minimum, minimum, maximum, maximum});
    require(accumulator.pending_sample() == ContactSectionSample{0x0F, 0x0F},
            "Negative extreme coordinates overflowed contact bounds");

    // The implementation intersects placement bounds before walking pixels;
    // this enormous empty region therefore performs no area-sized scan.
    accumulator.scan(placements, {&terrain, maximum, maximum},
                     {minimum, minimum, maximum, maximum});
    require(accumulator.pending_sample() == ContactSectionSample{0x0F, 0x0F},
            "An enormous empty clip changed pending contact");
}

template <typename Operation>
void require_invalid_unchanged(PixelContactAccumulator& accumulator,
                               Operation&& operation, const char* message) {
    const auto before = accumulator.pending_sample();
    bool rejected = false;
    try {
        operation();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && accumulator.pending_sample() == before, message);
}

void test_invalid_inputs_are_atomic() {
    const auto pixel = mask(1, 1, {1});
    const auto terrain = mask(1, 1, {1});
    const std::array seed{
        sprite(pixel, 0, 0, 0x01),
        sprite(pixel, 0, 0, 0x02),
    };
    PixelContactAccumulator accumulator;
    accumulator.scan(seed, {&terrain, 0, 0}, {0, 0, 1, 1});
    require(accumulator.pending_sample() == ContactSectionSample{0x03, 0x03},
            "Invalid-input test did not seed both channels");

    const std::array duplicate{
        sprite(pixel, 0, 0, 0x04),
        sprite(pixel, 0, 0, 0x04),
    };
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(duplicate, {&terrain, 0, 0}, {0, 0, 1, 1});
    }, "Duplicate participant was accepted or changed pending contact");

    const std::array non_one_hot{sprite(pixel, 0, 0, 0x03)};
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(non_one_hot, {&terrain, 0, 0}, {0, 0, 1, 1});
    }, "Non-one-hot participant was accepted or changed pending contact");

    const std::array zero_participant{sprite(pixel, 0, 0, 0x00)};
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(zero_participant, {&terrain, 0, 0}, {0, 0, 1, 1});
    }, "Zero participant was accepted or changed pending contact");

    const std::array no_mask{
        ContactSpritePlacement{{nullptr, 0, 0}, 0x04},
    };
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(no_mask, {&terrain, 0, 0}, {0, 0, 1, 1});
    }, "Null sprite mask was accepted or changed pending contact");

    const auto malformed = mask(2, 1, {1});
    const std::array malformed_sprite{sprite(malformed, 0, 0, 0x04)};
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(malformed_sprite, {&terrain, 0, 0}, {0, 0, 1, 1});
    }, "Malformed sprite mask was accepted or changed pending contact");

    const auto non_binary = mask(1, 1, {2});
    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(seed, {&non_binary, 0, 0}, {0, 0, 1, 1});
    }, "Non-binary terrain was accepted or changed pending contact");

    require_invalid_unchanged(accumulator, [&] {
        accumulator.scan(seed, {&terrain, 0, 0}, {0, 0, -1, 1});
    }, "Negative scan dimensions were accepted or changed pending contact");
}

} // namespace

int main() {
    try {
        test_exact_overlap_and_clipping();
        test_shared_multicolor_is_sprite_opaque_only();
        test_distinct_pairs_form_one_global_mask();
        test_accumulation_and_independent_consumption();
        test_extreme_coordinates_do_not_overflow_scan_bounds();
        test_invalid_inputs_are_atomic();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
