#include "game/river_viewport.hpp"

#include <algorithm>

namespace river_raid {

void RiverViewport::push(const PackedRiverRow& row) noexcept {
    std::move_backward(rows_.begin(), rows_.end() - 1, rows_.end());
    rows_.front() = row;
}

void RiverViewport::clear() noexcept { rows_ = {}; }

std::span<const PackedRiverRow> RiverViewport::rows() const noexcept { return rows_; }

} // namespace river_raid
