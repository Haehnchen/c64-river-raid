#include "app/world_scene_setup.hpp"
#include "app/world_setup.hpp"
#include "fixtures/world_pixel_cases.hpp"

#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

int main() {
    try {
        auto world = river_raid::make_river_world();
        for (const auto& sample : river_raid::test_data::world_pixel_cases) {
            while (world.generated_rows() < sample.first_row_zero_based + 1) (void)world.advance();
            const auto frame = river_raid::make_world_scene(
                std::span(world.viewport().rows()).first(157),
                std::span(sample.placements).first(sample.placement_count));
            if (frame.width != 320 || frame.height != 200) throw std::runtime_error("Invalid frame dimensions");
            std::uint64_t hash = 14695981039346656037ULL;
            for (const auto pixel : frame.pixels) {
                if ((pixel >> 24) != 255) throw std::runtime_error("Nonopaque world frame");
                for (const auto shift : {16, 8, 0}) {
                    hash ^= (pixel >> shift) & 255U;
                    hash *= 1099511628211ULL;
                }
            }
            if (hash != sample.rgb_hash) throw std::runtime_error("World pixels differ from the original RGB hash");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
