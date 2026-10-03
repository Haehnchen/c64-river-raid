#pragma once

#include "game/river_generator.hpp"

#include <cstddef>

namespace river_raid {

struct RiverStartPreset {
    RiverSectionAnchor anchor{};
    std::uint8_t section{};
};

RiverStartPreset river_start_preset(std::size_t preset_index);
RiverGenerator make_river_generator(RiverStartPreset preset);
RiverGenerator make_river_generator();
std::span<const std::uint8_t, 32> generator_edge_patterns();

} // namespace river_raid
