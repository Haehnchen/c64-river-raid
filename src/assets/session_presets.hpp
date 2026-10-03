#pragma once

#include <array>
#include <cstdint>

namespace river_raid::assets::session_presets {

using AnchorRandom = std::array<std::uint8_t, 3>;

struct SessionPreset {
    AnchorRandom anchor_random{};
    std::uint8_t course{};
    std::uint8_t section{};
    std::uint16_t bridge_number{};
};

inline constexpr std::array<SessionPreset, 4> presets{{
    {{0xE3, 0xD4, 0xF7}, 0x20, 1, 1},
    {{0x20, 0x83, 0xB5}, 0x21, 5, 5},
    {{0x80, 0xC1, 0x7F}, 0x20, 20, 20},
    {{0xB4, 0x6D, 0xC0}, 0x21, 50, 50},
}};

} // namespace river_raid::assets::session_presets
