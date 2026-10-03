#include "game/river_demo.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

constexpr river_raid::PackedRiverRow row(std::uint8_t value) {
    river_raid::PackedRiverRow result{};
    result.fill(value);
    return result;
}

// Newer generated rows come first. A new row enters at the top of the view.
constexpr std::array terrain_rows{row(0x50), row(0x40), row(0x30), row(0x20), row(0x10)};

void test_initial_view_uses_the_oldest_complete_window() {
    river_raid::RiverDemo demo({terrain_rows, 3});
    const auto visible = demo.visible_rows();

    require(visible.size() == 3, "Initial view has the wrong height");
    require(visible[0][0] == 0x30, "Initial view must begin at its newest generated row");
    require(visible[1][0] == 0x20, "Initial view must preserve newest-first row order");
    require(visible[2][0] == 0x10, "Initial view must end at its oldest generated row");
    require(demo.row_offset() == 2, "Initial row offset must leave later rows available");
    require(demo.remaining_advance_count() == 2, "Initial remaining advance count is wrong");
}

void test_advance_adds_a_row_at_the_top_and_moves_existing_rows_down() {
    river_raid::RiverDemo demo({terrain_rows, 3});

    require(demo.advance(), "First terrain advance must succeed");
    require(demo.row_offset() == 1, "First terrain advance has the wrong offset");
    require(demo.visible_rows()[0][0] == 0x40, "First terrain advance omitted the new top row");
    require(demo.visible_rows()[1][0] == 0x30, "Existing top row must move down");
    require(demo.visible_rows()[2][0] == 0x20, "First terrain advance kept the oldest row");
    require(demo.remaining_advance_count() == 1, "First remaining advance count is wrong");

    require(demo.advance(), "Second terrain advance must succeed");
    require(demo.visible_rows().front()[0] == 0x50, "Second advance has the wrong new top row");
    require(demo.visible_rows().back()[0] == 0x30, "Second advance has the wrong final row");
    require(demo.remaining_advance_count() == 0, "Exhausted terrain must report no advances");
}

void test_exhaustion_does_not_loop_or_move() {
    river_raid::RiverDemo demo({terrain_rows, terrain_rows.size()});

    require(!demo.advance(), "An exact-sized terrain view must already be exhausted");
    require(demo.row_offset() == 0, "Exhausted terrain must not move");
    require(demo.visible_rows().front()[0] == 0x50, "Exhausted terrain must not loop");
}

void test_reset_restores_the_first_view() {
    river_raid::RiverDemo demo({terrain_rows, 3});
    require(demo.advance(), "Reset setup advance must succeed");
    require(demo.advance(), "Reset setup second advance must succeed");

    demo.reset();
    require(demo.row_offset() == 2, "Reset must restore the initial row offset");
    require(demo.visible_rows().front()[0] == 0x30, "Reset must restore the first visible row");
    require(demo.remaining_advance_count() == 2, "Reset must restore the advance budget");
}

void test_invalid_views_are_rejected() {
    bool rejected_zero = false;
    try {
        (void)river_raid::RiverDemo({terrain_rows, 0});
    } catch (const std::invalid_argument&) {
        rejected_zero = true;
    }
    require(rejected_zero, "A zero-height terrain view must be rejected");

    bool rejected_oversized = false;
    try {
        (void)river_raid::RiverDemo({terrain_rows, terrain_rows.size() + 1});
    } catch (const std::invalid_argument&) {
        rejected_oversized = true;
    }
    require(rejected_oversized, "A terrain view taller than its source must be rejected");
}

} // namespace

int main() {
    try {
        test_initial_view_uses_the_oldest_complete_window();
        test_advance_adds_a_row_at_the_top_and_moves_existing_rows_down();
        test_exhaustion_does_not_loop_or_move();
        test_reset_restores_the_first_view();
        test_invalid_views_are_rejected();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
