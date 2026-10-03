#pragma once

#include <array>
#include <cstdint>

namespace river_raid::assets::audio {

inline constexpr std::array<std::uint8_t, 4> engine_voice1_selector{{
    0x00, 0x81, 0x84, 0x82,
}};

inline constexpr std::array<std::uint8_t, 4> effect_voice2_frequency_high{{
    0x40, 0x30, 0x26, 0x20,
}};

inline constexpr std::array<std::uint8_t, 17> bridge_voice3_frequency_high{{
    0xDE, 0xB9, 0x9A, 0x80, 0x6B, 0x59, 0x4A, 0x3E, 0x34, 0x2B, 0x24, 0x1E, 0x19, 0x15, 0x11, 0x0E, 0x0C,
}};

} // namespace river_raid::assets::audio
