#include "render/contact_masks.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_monochrome_and_multicolor_classes() {
    constexpr std::array<std::uint8_t, 4> mono_roles{0, 1, 0, 1};
    const auto mono = river_raid::make_sprite_occupancy_mask(
        4, 1, mono_roles, river_raid::SpriteMode::Monochrome);
    require(mono.width == 4 && mono.height == 1 &&
                mono.pixels == std::vector<std::uint8_t>{0, 1, 0, 1},
            "Monochrome transparency/foreground classes are wrong");

    // All visible multicolor sprite pairs participate in sprite/sprite and
    // sprite/background collision bits; the background side has its own
    // priority mask, covered by the terrain test below.
    constexpr std::array<std::uint8_t, 4> multi_roles{0, 1, 2, 3};
    const auto multi = river_raid::make_sprite_occupancy_mask(
        4, 1, multi_roles, river_raid::SpriteMode::Multicolor);
    require(multi.pixels == std::vector<std::uint8_t>{0, 1, 1, 1},
            "Multicolor 00/01/10/11 collision classes are wrong");
}

void test_expansion_and_indexed_black() {
    constexpr std::array<std::uint8_t, 3> roles{0, 2, 1};
    const auto expanded = river_raid::make_sprite_occupancy_mask(
        3, 1, roles, river_raid::SpriteMode::Multicolor, true);
    require(expanded.width == 6 &&
                expanded.pixels == std::vector<std::uint8_t>{0, 0, 1, 1, 1, 1},
            "Horizontal expansion did not duplicate sprite collision classes");

    // Palette index 0 is opaque black when it came from a source role; only
    // the explicit 255 sentinel is transparent.
    constexpr std::array<std::uint8_t, 3> indexed{255, 0, 6};
    const auto image = river_raid::make_indexed_occupancy_mask(3, 1, indexed);
    require(image.pixels == std::vector<std::uint8_t>{0, 1, 1},
            "Indexed black was mistaken for transparency");
}

void test_terrain_foreground_roles() {
    // Pair roles 00,01,10,11. Each pair expands to two native pixels.
    constexpr std::array<std::uint8_t, 1> packed{0x1B};
    const auto terrain = river_raid::make_terrain_occupancy_mask(packed, 1);
    require(terrain.width == 8 && terrain.height == 1 &&
                terrain.pixels == std::vector<std::uint8_t>{0, 0, 0, 0, 1, 1, 1, 1},
            "Terrain did not preserve VIC-II foreground semantics");

    constexpr std::array<std::uint8_t, 2> rows{0x55, 0xAA};
    const auto two_rows = river_raid::make_terrain_occupancy_mask(rows, 1);
    require(two_rows.height == 2 &&
                two_rows.pixels[0] == 0 && two_rows.pixels[7] == 0 &&
                two_rows.pixels[8] == 1 && two_rows.pixels[15] == 1,
            "Water/land terrain rows were classified incorrectly");
}

void test_clipping() {
    constexpr std::array<std::uint8_t, 4> source_pixels{0, 1, 0, 1};
    const auto source = river_raid::make_indexed_occupancy_mask(2, 2, source_pixels, 0);
    river_raid::OccupancyMask destination{4, 3, std::vector<std::uint8_t>(12, 0)};
    river_raid::stamp_occupancy_mask(destination, source, -1, 1, {0, 0, 4, 3});
    require(destination.pixels == std::vector<std::uint8_t>{
                                    0, 0, 0, 0,
                                    1, 0, 0, 0,
                                    1, 0, 0, 0},
            "Negative-origin clipping changed the wrong mask pixels");

    const auto before = destination.pixels;
    river_raid::stamp_occupancy_mask(destination, source, 0, 0, {0, 0, 0, 3});
    require(destination.pixels == before, "Empty clip altered the destination mask");
}

void test_terrain_union_preserves_foreground() {
    std::array<river_raid::OccupancyMask, 256> masks;
    for (unsigned byte = 0; byte < masks.size(); ++byte) {
        const std::array packed{static_cast<std::uint8_t>(byte)};
        masks[byte] = river_raid::make_terrain_occupancy_mask(packed, 1);
    }
    // Packed-byte unions must preserve every candidate foreground pixel.
    for (unsigned first = 0; first < masks.size(); ++first) {
        for (unsigned second = 0; second < masks.size(); ++second) {
            for (std::size_t pixel = 0; pixel < 8; ++pixel) {
                require(masks[first | second].pixels[pixel] ==
                            (masks[first].pixels[pixel] | masks[second].pixels[pixel]),
                        "Packed terrain union lost or added a foreground pixel");
            }
        }
    }
}

void test_invalid_inputs() {
    constexpr std::array<std::uint8_t, 1> one{0};
    const auto expect_invalid = [](auto&& operation, const char* message) {
        bool rejected = false;
        try { operation(); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, message);
    };
    expect_invalid([&] {
        (void)river_raid::make_sprite_occupancy_mask(2, 1, one,
                                                      river_raid::SpriteMode::Monochrome);
    }, "Short sprite role span was accepted");
    constexpr std::array<std::uint8_t, 1> bad_mono{2};
    expect_invalid([&] {
        (void)river_raid::make_sprite_occupancy_mask(1, 1, bad_mono,
                                                      river_raid::SpriteMode::Monochrome);
    }, "Invalid monochrome role was accepted");
    expect_invalid([&] {
        (void)river_raid::make_terrain_occupancy_mask(one, 2);
    }, "Partial terrain row was accepted");
    expect_invalid([&] {
        (void)river_raid::make_terrain_occupancy_mask(one, 0);
    }, "Zero-width terrain row was accepted");
}

} // namespace

int main() {
    try {
        test_monochrome_and_multicolor_classes();
        test_expansion_and_indexed_black();
        test_terrain_foreground_roles();
        test_terrain_union_preserves_foreground();
        test_clipping();
        test_invalid_inputs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
