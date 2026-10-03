#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace river_raid {

inline constexpr std::size_t kPackedRiverRowBytes = 40;
using PackedRiverRow = std::array<std::uint8_t, kPackedRiverRowBytes>;

} // namespace river_raid
