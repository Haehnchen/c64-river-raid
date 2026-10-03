#include "app/world_scene_setup.hpp"
#include "presentation_palette.hpp"

#include <array>
#include <cstddef>
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
            std::uint8_t packed = 0;
            for (int pair = 0; pair < 4; ++pair) {
                const auto role = static_cast<std::uint8_t>((y + column + pair) & 3U);
                packed |= static_cast<std::uint8_t>(role << (6 - pair * 2));
            }
            rows[y][column] = packed;
        }
    }
    return rows;
}

std::uint8_t terrain_role(const std::array<PackedRiverRow, 157>& rows,
                          std::size_t y, std::size_t x) {
    const auto column = x / 8;
    const auto pair = (x % 8) / 2;
    return static_cast<std::uint8_t>((rows[y][column] >> (6 - pair * 2)) & 3U);
}

std::size_t changed_pixels(const Frame& first, const Frame& second) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < first.pixels.size(); ++index) {
        if (first.pixels[index] != second.pixels[index]) ++count;
    }
    return count;
}

void test_bridge_flash_changes_only_visible_role_one_terrain() {
    const auto rows = terrain();
    const std::array placements{
        WorldObjectPlacement{0, WorldObjectImageSlot::primary, 120, 40, 0, 0},
        WorldObjectPlacement{1, WorldObjectImageSlot::primary, 80, 48, 11, 11},
    };
    constexpr std::size_t selected_option = 3;

    const auto normal_empty = make_world_scene(rows, {}, selected_option, false);
    const auto flash_empty = make_world_scene(rows, {}, selected_option, true);
    const auto normal = make_world_scene(rows, placements, selected_option, false);
    const auto flash = make_world_scene(rows, placements, selected_option, true);

    require(normal.width == 320 && normal.height == 200,
            "Bridge flash frame dimensions changed");
    require(changed_pixels(normal_empty, normal) > 0,
            "Opaque ordinary placement did not affect the frame");
    require(make_world_scene(rows, std::span(placements).first(1), selected_option, true).pixels ==
                flash_empty.pixels,
            "Blank ordinary placement changed the bridge flash frame");

    std::size_t expected_terrain_changes = 0;
    for (std::size_t y = 0; y < rows.size(); ++y) {
        for (std::size_t x = 0; x < 320; ++x) {
            const auto frame_index = (y + 4) * 320 + x;
            const auto role = terrain_role(rows, y, x);
            const auto expected = role == 1 ? assets::presentation_palette[2] :
                                             normal_empty.pixels[frame_index];
            require(flash_empty.pixels[frame_index] == expected,
                    "Bridge flash changed a non-role-one terrain pixel");
            if (role == 1) {
                require(normal_empty.pixels[frame_index] == assets::presentation_palette[14],
                        "Normal role-one terrain did not use palette 14");
                require(normal_empty.pixels[frame_index] != flash_empty.pixels[frame_index],
                        "Role-one terrain did not flash to palette 2");
                ++expected_terrain_changes;
            } else {
                require(normal_empty.pixels[frame_index] == flash_empty.pixels[frame_index],
                        "Non-role-one terrain changed during bridge flash");
            }
        }
    }

    std::size_t visible_terrain_changes = 0;
    for (std::size_t index = 0; index < normal.pixels.size(); ++index) {
        const bool opaque_sprite = normal.pixels[index] != normal_empty.pixels[index];
        if (opaque_sprite) {
            require(normal.pixels[index] == flash.pixels[index],
                    "Bridge flash recolored an opaque ordinary sprite");
        } else {
            require(normal.pixels[index] == normal_empty.pixels[index] &&
                        flash.pixels[index] == flash_empty.pixels[index],
                    "Bridge flash changed a non-sprite pixel outside terrain");
            if (normal.pixels[index] != flash.pixels[index]) ++visible_terrain_changes;
        }
    }
    require(visible_terrain_changes < expected_terrain_changes,
            "Opaque ordinary sprite did not cover any flashing terrain");

    for (std::size_t y = 0; y < 200; ++y) {
        if (y >= 4 && y < 161) continue;
        for (std::size_t x = 0; x < 320; ++x) {
            const auto index = y * 320 + x;
            require(normal.pixels[index] == flash.pixels[index],
                    "Bridge flash changed the HUD or frame background");
        }
    }

    require(make_world_scene(rows, placements, selected_option, false).pixels == normal.pixels,
            "Normal rendering did not restore after bridge flash rendering");
}

} // namespace

int main() {
    try {
        test_bridge_flash_changes_only_visible_role_one_terrain();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
