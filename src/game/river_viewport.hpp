#pragma once

#include "game/river_types.hpp"

namespace river_raid {

class RiverViewport {
public:
    static constexpr std::size_t height = 160;

    void push(const PackedRiverRow& row) noexcept;
    void clear() noexcept;
    [[nodiscard]] std::span<const PackedRiverRow> rows() const noexcept;

private:
    std::array<PackedRiverRow, height> rows_{};
};

} // namespace river_raid
