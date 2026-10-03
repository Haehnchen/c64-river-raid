#pragma once

#include "render/world_object_schedule.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace river_raid::test_data {

struct WorldPixelCase {
    std::size_t first_row_zero_based{};
    std::size_t placement_count{};
    std::array<WorldObjectPlacement, 8> placements{};
    std::uint64_t rgb_hash{};
};

inline constexpr std::array<WorldPixelCase, 4> world_pixel_cases{{
    {211, 8, {{{4, WorldObjectImageSlot::primary, 8, -3, 1, 1}, {5, WorldObjectImageSlot::alternate, 122, 3, 13, 13}, {6, WorldObjectImageSlot::primary, 272, 35, 0, 0}, {7, WorldObjectImageSlot::primary, 22, 61, 1, 1}, {8, WorldObjectImageSlot::alternate, 192, 67, 29, 13}, {9, WorldObjectImageSlot::primary, 8, 93, 1, 1}, {10, WorldObjectImageSlot::alternate, 106, 99, 9, 8}, {11, WorldObjectImageSlot::primary, 134, 131, 15, 15}}}, 0x9EF8D4A7387CFFD1ULL},
    {212, 8, {{{4, WorldObjectImageSlot::primary, 8, -2, 1, 1}, {5, WorldObjectImageSlot::alternate, 122, 4, 13, 13}, {6, WorldObjectImageSlot::primary, 272, 36, 0, 0}, {7, WorldObjectImageSlot::primary, 22, 62, 1, 1}, {8, WorldObjectImageSlot::alternate, 192, 68, 29, 13}, {9, WorldObjectImageSlot::primary, 8, 94, 1, 1}, {10, WorldObjectImageSlot::alternate, 106, 100, 9, 8}, {11, WorldObjectImageSlot::primary, 134, 132, 15, 15}}}, 0xE10E60FF12E79191ULL},
    {411, 7, {{{5, WorldObjectImageSlot::primary, 288, 11, 0, 0}, {6, WorldObjectImageSlot::primary, 284, 43, 0, 0}, {7, WorldObjectImageSlot::primary, 24, 69, 1, 1}, {8, WorldObjectImageSlot::alternate, 112, 75, 29, 13}, {9, WorldObjectImageSlot::primary, 136, 107, 15, 15}, {10, WorldObjectImageSlot::primary, 18, 133, 1, 1}, {11, WorldObjectImageSlot::alternate, 158, 139, 9, 8}, {}}}, 0xB52B20A8A715AA89ULL},
    {412, 7, {{{5, WorldObjectImageSlot::primary, 288, 12, 0, 0}, {6, WorldObjectImageSlot::primary, 284, 44, 0, 0}, {7, WorldObjectImageSlot::primary, 24, 70, 1, 1}, {8, WorldObjectImageSlot::alternate, 112, 76, 29, 13}, {9, WorldObjectImageSlot::primary, 136, 108, 15, 15}, {10, WorldObjectImageSlot::primary, 18, 134, 1, 1}, {11, WorldObjectImageSlot::alternate, 158, 140, 9, 8}, {}}}, 0x2F05D4F34A2360C9ULL},
}};

} // namespace river_raid::test_data
