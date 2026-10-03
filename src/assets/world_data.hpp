#pragma once

#include <array>
#include <cstdint>

namespace river_raid::assets::world {

struct ObjectSeedRecord {
    std::uint8_t vertical_position, horizontal_coordinate, flags;
    std::uint8_t animated_kind, base_kind;
    std::uint8_t left_bound, right_bound, animation_count;
};

inline constexpr std::array<ObjectSeedRecord, 12> initial_records{{
    {224, 0, 0, 0, 0, 0, 0, 0},
    {224, 0, 0, 0, 0, 0, 0, 0},
    {224, 255, 0, 0, 0, 0, 0, 0},
    {224, 255, 0, 0, 0, 0, 0, 0},
    {224, 255, 0, 0, 0, 0, 0, 0},
    {224, 255, 0, 0, 0, 0, 0, 0},
    {224, 0, 0, 0, 0, 0, 0, 0},
    {224, 0, 0, 0, 0, 0, 0, 0},
    {224, 0, 0, 0, 0, 0, 0, 0},
    {224, 128, 0, 0, 0, 0, 0, 0},
    {224, 255, 0, 0, 0, 0, 0, 0},
    {224, 255, 132, 0, 0, 0, 0, 0},
}};

inline constexpr std::uint8_t initial_active_begin = 11;
inline constexpr std::uint8_t initial_generator_cursor = 11;
inline constexpr std::uint8_t initial_special_cursor = 12;
inline constexpr std::uint8_t initial_spawn_cooldown = 19;
inline constexpr std::uint8_t initial_spawn_behavior_flags = 132;
inline constexpr bool initial_parity = false;
inline constexpr bool initial_split_collision_wrap = false;

inline constexpr std::array<std::uint8_t, 16> spawn_delay_by_kind{{
    0x17, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x09, 0x09, 0x09, 0x09, 0x13, 0x07, 0x13, 0x17,
}};

inline constexpr std::array<std::uint8_t, 8> random_kind_choices{{
    0x0D, 0x08, 0x0D, 0x08, 0x07, 0x0D, 0x08, 0x0C,
}};

inline constexpr std::array<std::uint8_t, 16> kind_clearance{{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
}};

} // namespace river_raid::assets::world
