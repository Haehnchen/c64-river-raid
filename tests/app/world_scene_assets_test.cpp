#include "app/world_scene_setup.hpp"
#include "presentation_palette.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {

using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::array<PackedRiverRow, 157> terrain() {
    std::array<PackedRiverRow, 157> rows{};
    for (std::size_t y = 0; y < rows.size(); ++y) {
        for (std::size_t column = 0; column < rows[y].size(); ++column) {
            rows[y][column] = static_cast<std::uint8_t>((column + y) & 0x55U);
        }
    }
    return rows;
}

WorldObjectPlacement placement(std::size_t index, WorldObjectImageSlot slot,
                               int left, int top, std::uint8_t image_key,
                               std::uint8_t style_key) {
    return {index, slot, left, top, image_key, style_key};
}

bool differs(const Frame& first, const Frame& second) {
    return first.width != second.width || first.height != second.height ||
        first.pixels != second.pixels;
}

void test_frame_background_and_real_sprite_mappings() {
    const auto rows = terrain();
    const auto empty = make_world_scene(rows, {}, 3);
    require(empty.width == 320 && empty.height == 200 &&
                empty.pixels.size() == 320U * 200U,
            "World frame dimensions changed");
    require(empty.pixels[0] == assets::presentation_palette[0],
            "World frame upper background color changed");
    require(empty.pixels[170U * empty.width] == assets::presentation_palette[12],
            "World frame HUD background color changed");

    const std::array placements{
        placement(0, WorldObjectImageSlot::primary, 8, 8, 11, 11),
        placement(1, WorldObjectImageSlot::alternate, 80, 32, 27, 11),
        placement(2, WorldObjectImageSlot::primary, 152, 64, 13, 13),
        placement(3, WorldObjectImageSlot::alternate, 224, 96, 29, 13),
    };
    const auto rendered = make_world_scene(rows, placements, 3);
    require(differs(empty, rendered), "Real world sprite placements had no effect");
    for (const auto& object : placements) {
        const std::array one{object};
        require(differs(empty, make_world_scene(rows, one, 3)),
                "Real primary/alternate sprite mapping had no visible pixels");
    }
}

void test_invalid_mappings_and_transparent_blank() {
    const auto rows = terrain();
    const auto empty = make_world_scene(rows, {});
    const auto blank = placement(0, WorldObjectImageSlot::primary, 120, 60, 0, 0);
    require(!differs(empty, make_world_scene(rows, std::array{blank})),
            "Transparent blank sprite changed the world frame");

    bool bad_image = false;
    try {
        const auto invalid = placement(0, WorldObjectImageSlot::primary, 0, 8, 32, 0);
        (void)make_world_scene(rows, std::array{invalid});
    } catch (const std::invalid_argument&) {
        bad_image = true;
    }
    require(bad_image, "Unknown world image key was accepted");

    bool bad_style = false;
    try {
        const auto invalid = placement(0, WorldObjectImageSlot::alternate, 0, 8, 11, 16);
        (void)make_world_scene(rows, std::array{invalid});
    } catch (const std::invalid_argument&) {
        bad_style = true;
    }
    require(bad_style, "Unknown world style key was accepted");
}

} // namespace

int main() {
    try {
        test_frame_background_and_real_sprite_mappings();
        test_invalid_mappings_and_transparent_blank();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
