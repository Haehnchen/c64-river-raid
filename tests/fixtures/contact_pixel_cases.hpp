#pragma once

#include <array>
#include <cstdint>

namespace river_raid::test_data {

struct ContactPixelCase {
    std::uint8_t sprite_a_role;
    std::uint8_t sprite_b_role;
    std::uint8_t terrain_role;
    std::uint8_t sprite_a_x;
    std::uint8_t sprite_b_x;
    std::uint8_t sprite_y;
    bool horizontal_expansion;
    std::uint8_t sprite_sprite_mask;
    std::uint8_t sprite_background_mask;
};

inline constexpr std::array<ContactPixelCase, 16> contact_pixel_cases{{
    {0, 3, 0, 100, 112, 100, false, 0, 0},
    {0, 3, 1, 100, 112, 100, false, 0, 0},
    {0, 3, 2, 100, 112, 100, false, 0, 0},
    {0, 3, 3, 100, 112, 100, false, 0, 0},
    {1, 3, 0, 100, 112, 100, false, 3, 0},
    {1, 3, 1, 100, 112, 100, false, 3, 0},
    {1, 3, 2, 100, 112, 100, false, 3, 1},
    {1, 3, 3, 100, 112, 100, false, 3, 1},
    {2, 3, 0, 100, 112, 100, false, 3, 0},
    {2, 3, 1, 100, 112, 100, false, 3, 0},
    {2, 3, 2, 100, 112, 100, false, 3, 1},
    {2, 3, 3, 100, 112, 100, false, 3, 1},
    {3, 3, 0, 100, 112, 100, false, 3, 0},
    {3, 3, 1, 100, 112, 100, false, 3, 0},
    {3, 3, 2, 100, 112, 100, false, 3, 1},
    {3, 3, 3, 100, 112, 100, false, 3, 1},
}};

inline constexpr std::array<std::uint8_t, 2> contact_terrain_screen_columns{9, 10};
inline constexpr std::array<std::uint8_t, 2> contact_terrain_vic_x_range{96, 111};

} // namespace river_raid::test_data
