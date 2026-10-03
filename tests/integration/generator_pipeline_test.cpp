#include "app/generator_setup.hpp"
#include "game/river_viewport.hpp"
#include "fixtures/river_row_cases.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    try {
        auto generator = river_raid::make_river_generator();
        const auto initial = generator.state();
        river_raid::RiverViewport view;
        for (int replay = 0; replay < 2; ++replay) {
            generator.reset();
            view.clear();
            if (generator.state() != initial) throw std::runtime_error("Seed reset changed");
            std::size_t step = 0;
            for (const auto& expected : river_raid::test_data::river_row_cases) {
                const auto row = river_raid::compose_river_row(generator.advance(),
                                                              river_raid::generator_edge_patterns());
                if (row != expected.expected) {
                    throw std::runtime_error("Native generator pipeline differs at row " + std::to_string(step + 1));
                }
                view.push(row);
                ++step;
            }
            if (view.rows().front() != river_raid::test_data::river_row_cases[199].expected ||
                view.rows().back() != river_raid::test_data::river_row_cases[40].expected) {
                throw std::runtime_error("Native generated viewport order");
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
