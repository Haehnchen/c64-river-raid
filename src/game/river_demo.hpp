#pragma once

#include "game/river_types.hpp"

namespace river_raid {

struct RiverDemoData {
    std::span<const PackedRiverRow> terrain_rows_newest_first;
    std::size_t visible_row_count;
};

class RiverDemo {
public:
    explicit RiverDemo(RiverDemoData data);

    [[nodiscard]] std::span<const PackedRiverRow> visible_rows() const noexcept;
    [[nodiscard]] std::size_t row_offset() const noexcept;
    [[nodiscard]] std::size_t remaining_advance_count() const noexcept;

    [[nodiscard]] bool advance() noexcept;
    void reset() noexcept;

private:
    RiverDemoData data_;
    std::size_t row_offset_{};
};

} // namespace river_raid
