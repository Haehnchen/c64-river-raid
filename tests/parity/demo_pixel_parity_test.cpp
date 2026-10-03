#include "app/demo_setup.hpp"
#include "app/world_scene_setup.hpp"
#include "fixtures/object_motion_cases.hpp"
#include "fixtures/world_pixel_cases.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc, char** argv) {
    try {
        std::filesystem::path output_directory;
        if (argc == 3 && std::string_view(argv[1]) == "--dump-dir") {
            output_directory = argv[2];
            std::filesystem::create_directories(output_directory);
        } else if (argc != 1) {
            throw std::invalid_argument("Expected optional --dump-dir PATH");
        }
        namespace data = river_raid::test_data;
        auto simulation = river_raid::make_demo_simulation();
        constexpr std::array<unsigned, 4> presentation_calls{199, 200, 399, 400};
        const auto rgb_hash = [](const river_raid::Frame& frame) {
            std::uint64_t hash = 14695981039346656037ULL;
            for (const auto pixel : frame.pixels) {
                if ((pixel >> 24) != 255) throw std::runtime_error("Nonopaque demo frame");
                for (const auto shift : {16, 8, 0}) {
                    hash ^= (pixel >> shift) & 255U;
                    hash *= 1099511628211ULL;
                }
            }
            return hash;
        };
        std::size_t checked = 0;
        for (std::size_t index = 2; index < data::object_motion_cases.size() && checked < presentation_calls.size(); ++index) {
            const auto& input = data::object_motion_cases[index];
            const auto candidate = input.activation_candidate == data::kNoObjectMotionCandidate
                ? std::nullopt : std::optional<std::uint8_t>{input.activation_candidate};
            (void)simulation.advance(candidate);
            if (input.call != presentation_calls[checked]) continue;
            const auto& step = *simulation.presentation_step();
            const auto& expected = data::world_pixel_cases[checked];
            if (step.rows_before != expected.first_row_zero_based + 1) {
                throw std::runtime_error("Presentation terrain is one step out of phase");
            }
            if (step.row_count == 0 ||
                simulation.world().generated_rows() != step.rows_before + step.row_count) {
                throw std::runtime_error("Post-scroll terrain counterfactual lacks a row advance");
            }
            const auto frame = river_raid::make_demo_scene(step);
            if (frame.width != 320 || frame.height != 200) throw std::runtime_error("Invalid demo frame dimensions");
            if (rgb_hash(frame) != expected.rgb_hash) {
                throw std::runtime_error("Native simulation/presentation differs from the original RGB frame");
            }

            // Isolate terrain stage selection: retain the same object schedule
            // and replace only the pre-scroll snapshot with the world's actual
            // post-scroll viewport from this simulation step.
            auto post_scroll_step = step;
            std::ranges::copy(simulation.world().viewport().rows(),
                              post_scroll_step.terrain_before_scroll.begin());
            if (post_scroll_step.terrain_before_scroll == step.terrain_before_scroll) {
                throw std::runtime_error("Counterfactual did not replace terrain rows");
            }
            const auto post_scroll_frame = river_raid::make_demo_scene(post_scroll_step);
            if (post_scroll_frame.pixels == frame.pixels ||
                rgb_hash(post_scroll_frame) == expected.rgb_hash) {
                throw std::runtime_error("Post-scroll terrain unexpectedly matches the pinned original frame");
            }
            if (!output_directory.empty()) {
                std::ofstream output(output_directory / (std::to_string(input.call + 1) + ".ppm"), std::ios::binary);
                output << "P6\n320 200\n255\n";
                for (const auto pixel : frame.pixels) {
                    const char bytes[]{static_cast<char>(pixel >> 16), static_cast<char>(pixel >> 8), static_cast<char>(pixel)};
                    output.write(bytes, 3);
                }
                output.close();
                if (!output) throw std::runtime_error("Cannot write demo frame");
            }
            ++checked;
        }
        if (checked != presentation_calls.size()) throw std::runtime_error("Not all original frames were checked");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
