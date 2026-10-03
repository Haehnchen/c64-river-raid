#include "game/river_viewport.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        river_raid::RiverViewport viewport;
        for (int index = 0; index < 200; ++index) {
            river_raid::PackedRiverRow row{};
            row.fill(static_cast<std::uint8_t>(index));
            viewport.push(row);
        }
        const auto rows = viewport.rows();
        if (rows.size() != 160 || rows.front().front() != 199 || rows.back().back() != 40) {
            throw std::runtime_error("Generated rows must move downward and expire at the bottom");
        }
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i][0] != 199 - i) throw std::runtime_error("Viewport row order");
        }
        viewport.clear();
        for (const auto& row : viewport.rows()) {
            for (auto pixel : row) {
                if (pixel != 0) throw std::runtime_error("Viewport reset");
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
