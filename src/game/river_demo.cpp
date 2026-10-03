#include "game/river_demo.hpp"

#include <stdexcept>

namespace river_raid {

RiverDemo::RiverDemo(RiverDemoData data) : data_(data) {
    if (data_.visible_row_count == 0) {
        throw std::invalid_argument("RiverDemo needs a visible row");
    }
    if (data_.terrain_rows_newest_first.size() < data_.visible_row_count) {
        throw std::invalid_argument("RiverDemo needs enough terrain rows for its view");
    }
    reset();
}

std::span<const PackedRiverRow> RiverDemo::visible_rows() const noexcept {
    return data_.terrain_rows_newest_first.subspan(row_offset_, data_.visible_row_count);
}

std::size_t RiverDemo::row_offset() const noexcept {
    return row_offset_;
}

std::size_t RiverDemo::remaining_advance_count() const noexcept {
    return row_offset_;
}

bool RiverDemo::advance() noexcept {
    if (row_offset_ == 0) {
        return false;
    }
    --row_offset_;
    return true;
}

void RiverDemo::reset() noexcept {
    row_offset_ = data_.terrain_rows_newest_first.size() - data_.visible_row_count;
}

} // namespace river_raid
