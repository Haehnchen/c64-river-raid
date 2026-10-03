#pragma once

#include "app/flight_scene.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace river_raid::test {

// Optional whole-scene output from existing normal-input session regressions.
class SessionSceneCapture {
public:
    void configure(int argc, char** argv) {
        if (argc == 1) return;
        if (argc != 3 || std::string_view(argv[1]) != "--capture-dir" ||
            std::string_view(argv[2]).empty()) {
            throw std::runtime_error("Usage: session test [--capture-dir DIRECTORY]");
        }
        directory_ = argv[2];
        std::filesystem::create_directories(directory_);
    }

    [[nodiscard]] bool enabled() const noexcept { return !directory_.empty(); }

    void record(std::string_view label, const FlightPreview& flight) {
        if (!enabled()) return;
        if (label.empty() || label.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string_view::npos) {
            throw std::runtime_error("Invalid session scene label");
        }
        const auto image_path = directory_ / (std::string(label) + ".ppm");
        const auto state_path = directory_ / (std::string(label) + ".txt");
        if (std::filesystem::exists(image_path) || std::filesystem::exists(state_path)) {
            throw std::runtime_error("Session scene output already exists");
        }
        const auto frame = make_flight_scene(flight);
        std::ofstream image;
        image.exceptions(std::ios::failbit | std::ios::badbit);
        image.open(image_path, std::ios::binary);
        image << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
        for (const auto pixel : frame.pixels) {
            for (const auto shift : {16, 8, 0})
                image.put(static_cast<char>((pixel >> shift) & 255));
        }
        image.close();
        std::ofstream state;
        state.exceptions(std::ios::failbit | std::ios::badbit);
        state.open(state_path);
        state << "scope=native_normal_input_event_boundary\n"
              << "original_frame_parity=false\n"
              << "ordinal=" << ordinal_++ << '\n'
              << "label=" << label << '\n'
              << "option=" << flight.selected_option() << '\n'
              << "status=" << static_cast<unsigned>(flight.status()) << '\n'
              << "life_phase=" << static_cast<unsigned>(flight.life_cycle().phase) << '\n'
              << "rows=" << flight.world().generated_rows() << '\n'
              << "score=" << flight.score() << '\n'
              << "bridge=" << flight.bridge_number() << '\n'
              << "fuel=" << flight.fuel() << '\n'
              << "reserves=" << static_cast<unsigned>(flight.lives()) << '\n'
              << "private_image=" << static_cast<unsigned>(flight.life_cycle().image) << '\n'
              << "presented_image=" << static_cast<unsigned>(flight.presented_image()) << '\n';
        state.close();
    }

private:
    std::filesystem::path directory_;
    unsigned ordinal_{};
};

} // namespace river_raid::test
