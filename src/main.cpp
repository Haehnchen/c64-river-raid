#include "platform/sdl_preview.hpp"
#include "render/glyph_atlas.hpp"
#include "render/character_scene.hpp"
#include "render/options_scene.hpp"
#include "app/world_setup.hpp"
#include "app/object_sheet.hpp"
#include "app/world_scene_setup.hpp"
#include "app/flight_start.hpp"
#include "app/flight_scene.hpp"
#include "game/river_viewport.hpp"
#include "startup_glyphs.hpp"
#include "title_data.hpp"
#include "presentation_palette.hpp"
#include "game_data.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
std::size_t parse_steps(std::string_view value) {
    std::size_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("Steps must be a nonnegative integer");
    }
    return result;
}

river_raid::Frame title_frame() {
    using namespace river_raid;
    using namespace river_raid::assets;
    const std::array layers{ImageLayer{title_logo_pixels, title_logo_width,
        title_logo_height, title_logo_origin_x, title_logo_origin_y}};
    return render_character_scene({startup_glyphs, title_screen, title_colors,
                                   presentation_palette, layers});
}

void save_ppm(const river_raid::Frame& frame, const char* filename) {
    constexpr std::size_t channels = 3;
    if (frame.pixels.size() > std::numeric_limits<std::size_t>::max() / channels) {
        throw std::length_error("Image is too large to export");
    }
    std::vector<std::uint8_t> rgb(frame.pixels.size() * channels);
    for (std::size_t index = 0; index < frame.pixels.size(); ++index) {
        const auto pixel = frame.pixels[index];
        rgb[index * channels] = static_cast<std::uint8_t>((pixel >> 16U) & 0xffU);
        rgb[index * channels + 1] = static_cast<std::uint8_t>((pixel >> 8U) & 0xffU);
        rgb[index * channels + 2] = static_cast<std::uint8_t>(pixel & 0xffU);
    }
    if (!std::in_range<std::streamsize>(rgb.size())) {
        throw std::length_error("Image is too large to write");
    }

    std::ofstream output(filename, std::ios::binary);
    output << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(rgb.data()),
                 static_cast<std::streamsize>(rgb.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Cannot write image");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        if (mode == "--help") {
            if (argc != 2) throw std::invalid_argument("Unexpected help arguments");
            std::cout << "Standalone native River Raid title, options and gameplay\n"
                         "  --dump-title FILE.ppm  Export the embedded title scene\n"
                         "  --dump-atlas FILE.ppm  Export the embedded glyph atlas\n"
                         "  --dump-sprites FILE.ppm  Export the object sprite specimen sheet\n"
                         "  --sprites             Open the sprite specimen sheet\n"
                         "  --smoke-sprites       Render three hidden specimen frames\n"
                         "  --dump-options FILE.ppm [STEPS]  Export the terrain/HUD scene\n"
                         "  --dump-world FILE.ppm [STEPS]  Export a native world snapshot\n"
                         "  --dump-flight FILE.ppm Export the initial flight frame\n"
                         "  --world               Open manual world snapshot view (not timed gameplay)\n"
                         "  --smoke-world         Render three hidden world snapshots\n"
                         "  --options             Open the options screen directly\n"
                         "  --smoke-test          Render three hidden frames\n"
                         "  --smoke-options       Render three hidden options frames\n"
                         "  --smoke-flight        Render three hidden flight frames\n"
                         "  F1                    Title -> options -> flight; restart while not flying\n"
                         "  F3                    Select option while not flying\n"
                         "  Arrow keys            Steer / change speed in flight\n"
                         "  Space                 Fire in flight\n"
                         "  Joysticks 1/2         Player ports; axes 0/1 or hat 0, button 0 fires\n"
                         "  F11                   Toggle fullscreen / window (4:3 gameplay)\n"
                         "  Escape                Close River Raid\n";
            return 0;
        }
        if (mode == "--dump-atlas" && argc == 3) {
            save_ppm(river_raid::render_glyph_atlas(river_raid::assets::startup_glyphs), argv[2]);
            return 0;
        }
        if (mode == "--dump-sprites" && argc == 3) {
            save_ppm(river_raid::make_object_sprite_sheet(), argv[2]);
            return 0;
        }
        if ((mode == "--sprites" || mode == "--smoke-sprites") && argc == 2) {
            const auto sheet = river_raid::make_object_sprite_sheet();
            const river_raid::PreviewCallbacks callbacks{
                [&] { return sheet; }, [](auto, bool) {}, [] {}};
            return river_raid::show_preview(callbacks, mode == "--smoke-sprites");
        }
        const auto frame = title_frame();
        if (mode == "--dump-title" && argc == 3) {
            save_ppm(frame, argv[2]);
            return 0;
        }
        using namespace river_raid;
        using namespace river_raid::assets;
        auto world = make_river_world();
        // Keep the preview inside the range verified against original execution.
        constexpr std::size_t validated_rows = 2048;
        const auto advance_terrain = [&] {
            if (world.generated_rows() == validated_rows) return;
            (void)world.advance();
        };
        for (std::size_t i = 0; i < RiverViewport::height; ++i) advance_terrain();
        constexpr std::array options{StartOption{"1"}, StartOption{"2"},
            StartOption{"3"}, StartOption{"4"}, StartOption{"5"},
            StartOption{"6"}, StartOption{"7"}, StartOption{"8"}};
        StartFlow flow(options);
        FlightPreview flight;
        FlightStart launch(flow, flight);
        const OptionsSceneData scene{hud_tiles, hud_cells, hud_option_glyphs,
            hud_option_cell, river_color_roles, presentation_palette,
            {hud_fuel_marker, 4, 12, hud_fuel_marker_x, hud_fuel_marker_y}};
        const auto render_options = [&] {
            return render_options_scene(scene, world.viewport().rows(), flow.selected_option_index());
        };
        const auto render_world = [&] {
            std::vector<WorldObjectPlacement> placements;
            for (const auto& band : schedule_world_object_snapshot(world.river_state().phase, world.object_state())) {
                placements.insert(placements.end(), band.placements.begin(), band.placements.end());
            }
            return make_world_scene(std::span(world.viewport().rows()).first(157), placements,
                                    flow.selected_option_index());
        };
        const auto render_flight = [&] {
            return make_flight_scene(flight);
        };
        const auto start_from_title = [&] {
            for (int press = 0; press < 2; ++press) {
                launch.set_button(StartButton::Proceed, true);
                launch.set_button(StartButton::Proceed, false);
            }
        };
        if (mode == "--dump-flight" && argc == 3) {
            start_from_title();
            constexpr std::size_t max_launch_slices = 512;
            std::size_t slices = 0;
            while ((flight.life_cycle().phase != LifeCyclePhase::AwaitInput ||
                    flight.presented_image() != LifeCycleImage::Straight) &&
                   slices++ < max_launch_slices) {
                (void)launch.advance_elapsed(std::chrono::milliseconds{10});
                (void)flight.take_audio();
            }
            if (flight.status() != FlightStatus::Flying ||
                flight.life_cycle().phase != LifeCyclePhase::AwaitInput ||
                flight.presented_image() != LifeCycleImage::Straight) {
                throw std::runtime_error("Initial flight preview did not reach aircraft reveal");
            }
            save_ppm(render_flight(), argv[2]);
            return 0;
        }
        if (mode == "--world" || mode == "--smoke-world") {
            if (argc != 2) throw std::invalid_argument("Unexpected world view arguments");
            flow.set_button(StartButton::Proceed, true);
            flow.set_button(StartButton::Proceed, false);
            const PreviewCallbacks callbacks{
                [&] { return flight.status() != FlightStatus::Ready ? render_flight() : render_world(); },
                [&](StartButton button, bool pressed) { launch.set_button(button, pressed); },
                [&] { if (flight.status() == FlightStatus::Ready) advance_terrain(); },
                [&](auto elapsed) { launch.advance_elapsed(elapsed); },
                [&](auto controls) { launch.set_controls(controls); },
                [&] { return flight.take_audio(); },
                [&](auto controls) { launch.set_joystick_controls(controls); },
                [&] { return launch.start_generation(); }};
            return show_preview(callbacks, mode == "--smoke-world");
        }
        if (mode == "--dump-world" && (argc == 3 || argc == 4)) {
            const auto steps = argc == 4 ? parse_steps(argv[3]) : 0;
            if (steps > validated_rows - world.generated_rows()) {
                throw std::invalid_argument("Steps exceed the validated generator range");
            }
            for (std::size_t i = 0; i < steps; ++i) advance_terrain();
            save_ppm(render_world(), argv[2]);
            return 0;
        }
        if (mode == "--dump-options" && (argc == 3 || argc == 4)) {
            const auto steps = argc == 4 ? parse_steps(argv[3]) : 0;
            if (steps > validated_rows - world.generated_rows()) {
                throw std::invalid_argument("Steps exceed the validated generator range");
            }
            for (std::size_t i = 0; i < steps; ++i) advance_terrain();
            save_ppm(render_options(), argv[2]);
            return 0;
        }
        if (argc > 2 || (!mode.empty() && mode != "--smoke-test" &&
                        mode != "--smoke-options" && mode != "--options" && mode != "--smoke-flight")) {
            throw std::invalid_argument("Unknown arguments; use --help");
        }
        if (mode == "--options" || mode == "--smoke-options") {
            launch.set_button(StartButton::Proceed, true);
            launch.set_button(StartButton::Proceed, false);
        }
        if (mode == "--smoke-flight") {
            start_from_title();
        }
        const PreviewCallbacks callbacks{
            [&] { return flow.stage() == StartStage::Title ? frame : render_flight(); },
            [&](StartButton button, bool pressed) { launch.set_button(button, pressed); },
            [] {},
            [&](auto elapsed) { launch.advance_elapsed(elapsed); },
            [&](auto controls) { launch.set_controls(controls); },
            [&] { return flight.take_audio(); },
            [&](auto controls) { launch.set_joystick_controls(controls); },
            [&] { return launch.start_generation(); },
        };
        return show_preview(callbacks, mode == "--smoke-test" || mode == "--smoke-options" || mode == "--smoke-flight");
    } catch (const std::exception& error) {
        std::cerr << "River Raid: " << error.what() << '\n';
        return 1;
    }
}
