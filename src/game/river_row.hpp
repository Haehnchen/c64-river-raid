#pragma once

#include "game/river_types.hpp"

namespace river_raid {

enum class StampPlacement { None, LeftBank, RightBank, Island };

struct TerrainStampRow {
    std::span<const std::uint8_t> pixels;
    std::uint8_t margin{};
    StampPlacement placement{StampPlacement::None};
};

struct RiverRowInput {
    std::uint8_t course{};
    std::uint8_t channel_span{};
    std::uint8_t bank_inset{};
    bool split_channel{};
    bool bridge{};
    TerrainStampRow stamp{};
};

struct RiverGuides {
    std::uint8_t first_left{};
    std::uint8_t first_right{};
    std::uint8_t second_left{};
    std::uint8_t second_right{};

    friend bool operator==(const RiverGuides&, const RiverGuides&) = default;
};

[[nodiscard]] RiverGuides river_guides(const RiverRowInput& input);

PackedRiverRow compose_river_row(const RiverRowInput& input,
                                std::span<const std::uint8_t, 32> edge_patterns);

} // namespace river_raid
