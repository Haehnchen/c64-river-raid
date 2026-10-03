#include "game/river_row.hpp"
#include "game_data.hpp"
#include "fixtures/river_row_cases.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        using namespace river_raid;
        const auto& edges = assets::river_edge_patterns;
        std::size_t checked = 0;
        for (const auto& fixture : test_data::river_row_cases) {
            const RiverRowInput input{fixture.course, fixture.channel_span, fixture.bank_inset,
                fixture.split, fixture.bridge,
                {std::span(fixture.stamp).first(fixture.stamp_width), fixture.margin, fixture.placement}};
            if (compose_river_row(input, edges) != fixture.expected) {
                throw std::runtime_error("Original row mismatch at case " + std::to_string(checked + 1));
            }
            ++checked;
        }
        require(checked == 200, "Expected 200 original row cases");
        struct GuideCase { std::size_t step; RiverGuides expected; };
        constexpr std::array guide_cases{
            GuideCase{16, {80, 88, 0, 0}}, GuideCase{32, {80, 92, 0, 0}},
            GuideCase{48, {66, 106, 0, 0}}, GuideCase{64, {52, 112, 0, 0}},
            GuideCase{80, {52, 112, 0, 0}}, GuideCase{96, {50, 110, 0, 0}},
            GuideCase{112, {52, 112, 0, 0}}, GuideCase{128, {60, 120, 0, 0}},
            GuideCase{144, {58, 118, 0, 0}}, GuideCase{160, {50, 110, 0, 0}},
            GuideCase{176, {52, 112, 0, 0}}, GuideCase{192, {54, 114, 0, 0}},
        };
        for (const auto& fixture : guide_cases) {
            const auto& row = test_data::river_row_cases[fixture.step - 1];
            require(river_guides({row.course, row.channel_span, row.bank_inset,
                                 row.split, row.bridge, {}}) == fixture.expected,
                    "Original river guide mismatch");
        }
        require(river_guides({32, 64, 32, true, false, {}}) == RiverGuides{26, 46, 122, 142},
                "Synthetic split guides");
        require(river_guides({32, 120, 4, true, false, {}}) == RiverGuides{26, 142, 0, 0},
                "Synthetic merged channel guides");
        const RiverRowInput first{31, 20, 54, false, true, {}};
        const auto first_row = compose_river_row(first, edges);
        require(first_row == test_data::river_row_cases.front().expected, "First generated row differs");
        const RiverRowInput wrapped{255, 20, 54, false, false, {}};
        require(compose_river_row(wrapped, edges) ==
                    compose_river_row({53, 20, 0, false, false, {}}, edges),
                "Checkpoint first edge did not wrap to 37");
        require(river_guides(wrapped) == RiverGuides{48, 56, 0, 0},
                "Checkpoint river guides did not use wrapped geometry");
        const RiverRowInput open{32, 64, 32, false, false, {}};
        const auto plain = compose_river_row(open, edges);
        require(plain.front() == 0xaa && plain.back() == 0xaa, "Land fill");
        require(plain[20] == 0x55, "Water fill");
        constexpr std::array<std::uint8_t, 2> stamp{0x12, 0x34};
        auto decorated = open;
        decorated.stamp = {stamp, 2, StampPlacement::LeftBank};
        const auto left = compose_river_row(decorated, edges);
        require(left[2] == 0x12 && left[3] == 0x34, "Left bank stamp order");
        decorated.stamp.placement = StampPlacement::RightBank;
        const auto right = compose_river_row(decorated, edges);
        require(right[37] == 0x12 && right[38] == 0x34 && right[39] == 0xaa, "Right bank margin");
        decorated = {32, 64, 32, true, false, {}};
        const auto split = compose_river_row(decorated, edges);
        require(split[8] == 0x55 && split[20] == 0xaa && split[30] == 0x55, "Split channel");
        bool rejected = false;
        try { (void)compose_river_row({0, 255, 0, false, false, {}}, edges); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid geometry accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
