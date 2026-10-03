#pragma once

#include "game/world_objects.hpp"

#include <array>
#include <cstdint>

namespace river_raid::test_data::game5_contact_pair {

// Conditional captured component inputs, never normal-session initialization.
// Target band placement is reconstructed; IRQ assignment history is not captured.
struct SourceSection { std::uint8_t objects, terrain, primary, alternate; };

inline constexpr std::array<WorldObjectRecord, 12> producing_records{{
    {182, 0, 0, 0, 0, 0, 0, 0},
    {182, 0, 0, 0, 0, 0, 0, 0},
    {182, 255, 0, 0, 0, 0, 0, 0},
    {182, 255, 0, 0, 0, 0, 0, 0},
    {182, 255, 0, 0, 0, 0, 0, 0},
    {55, 65, 20, 15, 15, 24, 60, 0},
    {55, 65, 20, 15, 15, 24, 60, 0},
    {81, 17, 28, 7, 7, 0, 0, 0},
    {87, 44, 28, 13, 13, 20, 42, 0},
    {119, 119, 84, 12, 12, 106, 134, 0},
    {151, 0, 20, 0, 0, 0, 0, 0},
    {183, 122, 20, 0, 0, 0, 0, 0},
}};

inline constexpr std::array<SourceSection, 6> published_sections{{
    {0, 0, 255, 255},
    {0, 0, 11, 255},
    {0, 0, 10, 255},
    {0, 0, 9, 255},
    {0, 128, 7, 8},
    {0, 128, 6, 255},
}};

inline constexpr std::array<std::uint8_t, 63> object_raw{{
    15, 255, 0, 42, 170, 128, 42, 170, 128, 85, 85, 80, 85, 85, 80, 85, 85, 80, 255, 255, 240,
    255, 255, 240, 255, 255, 240, 42, 170, 128, 42, 170, 128, 5, 85, 0, 9, 86, 0, 8, 2, 0,
    8, 2, 0, 8, 2, 0, 8, 2, 0, 8, 2, 0, 2, 168, 0, 2, 168, 0, 2, 168, 0,
}};

inline constexpr std::array<std::uint8_t, 63> shot_raw{{
    0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
}};

inline constexpr unsigned generating_phase = 22;
inline constexpr unsigned active_begin = 6;
inline constexpr unsigned target_record = 9;
inline constexpr unsigned native_half_x = 115;
inline constexpr unsigned shot_vic_x = 222;
inline constexpr unsigned shot_vic_y = 128;
inline constexpr unsigned shot_before = 128;
inline constexpr unsigned shot_after = 124;
inline constexpr unsigned score_before = 0;
inline constexpr unsigned score_after = 0;
inline constexpr bool object_expanded = false;

} // namespace river_raid::test_data::game5_contact_pair
