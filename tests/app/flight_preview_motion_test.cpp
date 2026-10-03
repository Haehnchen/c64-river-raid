#include "app/flight_preview.hpp"
#include "app/world_setup.hpp"
#include "game/activation_sample.hpp"
#include "support/flight_start_helpers.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void advance_expected_tick(RiverWorld& world, PlayerSteeringState& steering,
                           std::uint8_t& animation_phase, std::uint8_t& scroll_fraction,
                           std::size_t& activation_sample, FlightControls controls) {
    // Independently computed SplitMix64 top-byte vectors for the default F1 seed.
    // These expected samples intentionally do not call the product source.
    constexpr std::array<std::uint8_t, 128> samples{
        0xE8, 0xF3, 0x07, 0xDC, 0x71, 0x03, 0xB0, 0xE5,
        0x7C, 0x2F, 0x77, 0x94, 0x68, 0xDF, 0x78, 0x7E,
        0xA4, 0x53, 0x22, 0x55, 0xCF, 0x0D, 0xFB, 0xAB,
        0x31, 0xD2, 0xBD, 0x19, 0xDF, 0x59, 0x75, 0xDC,
        0x15, 0xE3, 0x13, 0x10, 0x8C, 0x06, 0xF1, 0x0B,
        0x67, 0x9F, 0x07, 0x34, 0x4C, 0x08, 0xF2, 0x06,
        0x60, 0x64, 0xCC, 0xAC, 0x2E, 0xEA, 0xA4, 0x75,
        0x37, 0xA3, 0xEA, 0xCD, 0xAB, 0x40, 0x86, 0xBD,
        0x46, 0x4D, 0x43, 0x51, 0xB1, 0x93, 0xDC, 0xB2,
        0xB9, 0xD9, 0xE2, 0xF8, 0x30, 0x94, 0x76, 0xF7,
        0x89, 0x74, 0xEA, 0x43, 0x10, 0x86, 0x86, 0xBF,
        0x57, 0x9B, 0x8B, 0x8B, 0xB5, 0xD0, 0xDB, 0xAD,
        0x15, 0x30, 0x66, 0x4B, 0x6A, 0xC8, 0x44, 0xBF,
        0xAD, 0xA4, 0x0F, 0xDF, 0xEA, 0xFD, 0xAF, 0x19,
        0x71, 0x51, 0xF0, 0x6E, 0xB0, 0x67, 0x15, 0x52,
        0xCE, 0x73, 0xC9, 0x3F, 0x19, 0xA3, 0x5E, 0x5D,
    };
    std::optional<std::uint8_t> candidate;
    if (is_activation_phase(animation_phase)) {
        require(activation_sample < samples.size(),
                "Expected activation sample stream was exhausted");
        candidate = activation_candidate_from_sample(samples[activation_sample++]);
    }
    world.advance_motion({animation_phase, world.river_state().section >= 2, candidate});
    ++animation_phase;

    steering = steer_player(steering, controls).state;
    for (int attempt = 0; attempt < 2; ++attempt) {
        bool scroll = steering.scroll_speed >= 0xFE;
        if (!scroll) {
            const auto sum = static_cast<unsigned>(scroll_fraction) + steering.scroll_speed;
            scroll_fraction = static_cast<std::uint8_t>(sum);
            scroll = sum > 0xFFU;
        }
        if (scroll) (void)world.advance();
    }
}

void compare_preview_to_expected(const FlightPreview& preview, const RiverWorld& expected,
                                 const PlayerSteeringState& expected_steering) {
    require(preview.world().generated_rows() == expected.generated_rows(),
            "Preview and native tick copy generated different row counts");
    require(preview.world().river_state() == expected.river_state(),
            "Preview and native tick copy diverged in generator state");
    require(preview.world().object_state() == expected.object_state(),
            "Preview and native tick copy diverged in object state");
    require(preview.steering() == expected_steering,
            "Preview and native tick copy diverged in steering state");
}

void advance_one_tick(FlightPreview& preview) {
    using namespace std::chrono_literals;
    for (std::size_t wait = 0; wait < 4; ++wait) {
        const auto completed = preview.advance_elapsed(10ms);
        require(completed <= 1, "Ten milliseconds produced more than one preview tick");
        if (completed == 1) return;
    }
    throw std::runtime_error("Bounded wait did not produce one preview tick");
}

void test_ready_and_initial_flight_are_bounded() {
    using namespace std::chrono_literals;

    FlightPreview preview;
    const auto initial_steering = preview.steering();
    const auto initial_river = preview.world().river_state();
    const auto initial_objects = preview.world().object_state();
    const auto initial_rows = preview.world().generated_rows();
    require(preview.status() == FlightStatus::Ready && !preview.running(),
            "Flight preview did not start ready and inactive");
    require(preview.advance_elapsed(1s) == 0 && preview.steering() == initial_steering &&
                preview.world().river_state() == initial_river &&
                preview.world().object_state() == initial_objects &&
                preview.world().generated_rows() == initial_rows,
            "Inactive preview advanced native state");

    preview.start();
    require(preview.status() == FlightStatus::Preparing,
            "Starting the preview skipped course preparation");
    river_raid::test::wait_for_launch(preview);
    preview.set_controls({});
    for (int tick = 0; tick < 32; ++tick) {
        require(preview.status() == FlightStatus::Flying,
                "Initial natural flight left its active state unexpectedly");
        advance_one_tick(preview);
    }
}

void test_natural_tick_copy_during_alive_segment() {
    using namespace std::chrono_literals;

    FlightPreview preview;
    preview.start();
    river_raid::test::wait_for_launch(preview);

    auto expected = preview.world();
    auto expected_steering = preview.steering();
    // Cold reset admits one foreground entry and five phase-only work ticks;
    // the 78 remaining rebuild passes leave the normal input gate at phase 84.
    require(preview.animation_phase() == 84 && !preview.checkpoint_reset_work(),
            "Natural launch missed its bounded reset-work phase boundary");
    std::uint8_t expected_animation_phase = 84;
    std::uint8_t expected_scroll_fraction = 0;
    std::size_t section_one_ticks = 0;
    // Phases 0, 16, 32, 48, 64 and 80 sampled the unchanged approximation.
    std::size_t activation_sample = 6;
    std::size_t expected_ticks = 0;
    std::size_t calls = 0;

    compare_preview_to_expected(preview, expected, expected_steering);
    while (preview.status() == FlightStatus::Flying && calls < 64) {
        ++calls;
        const auto controls = FlightControls{false, false, false, false, calls == 1};
        if (calls == 1) expected_steering.scroll_speed = 0x40;
        preview.set_controls(controls);
        const auto before_river = preview.world().river_state();
        advance_one_tick(preview);
        if (before_river.section == 1) {
            ++section_one_ticks;
        }
        advance_expected_tick(expected, expected_steering, expected_animation_phase,
                              expected_scroll_fraction, activation_sample, controls);
        ++expected_ticks;
        if (preview.status() != FlightStatus::Flying) break;
        compare_preview_to_expected(preview, expected, expected_steering);
    }
    require(section_one_ticks > 0 && expected_ticks > 0,
            "Natural alive segment did not produce native ticks");
    require(activation_sample == (84 + expected_ticks + 15) / 16,
            "Natural run did not consume exactly one sample per 16 phases");
}

void test_inactive_lifecycle_stops_horizontal_motion_not_animation() {
    FlightPreview preview;
    preview.start(2);
    test::wait_for_launch(preview);
    const auto initial = preview.world().object_state();
    bool animated = false;
    for (int tick = 0; tick < 64; ++tick) {
        const auto before = preview.world().object_state();
        advance_one_tick(preview);
        require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput,
                "Neutral launch wait left its lifecycle gate");
        const auto& after = preview.world().object_state();
        for (std::size_t slot = 0; slot < after.records.size(); ++slot) {
            require(after.records[slot].horizontal_coordinate ==
                        initial.records[slot].horizontal_coordinate,
                    "Inactive launch wait moved an object horizontally");
            animated |= before.records[slot].animated_kind != after.records[slot].animated_kind;
        }
    }
    require(animated, "Launch wait froze object animation with horizontal motion");

    preview.set_controls({false, false, false, true, false});
    bool moved = false;
    for (int tick = 0; tick < 500 && preview.status() != FlightStatus::Crashed; ++tick) {
        const auto before = preview.world().object_state();
        const auto rows = preview.world().generated_rows();
        advance_one_tick(preview);
        const auto delta = preview.world().generated_rows() - rows;
        const auto& after = preview.world().object_state();
        for (std::size_t slot = 0; slot < after.records.size(); ++slot) {
            const auto& old = before.records[slot];
            const auto& now = after.records[slot];
            if (old.base_kind >= 7 && old.base_kind == now.base_kind &&
                now.vertical_position == static_cast<std::uint8_t>(old.vertical_position + delta))
                moved |= old.horizontal_coordinate != now.horizontal_coordinate;
        }
    }
    require(moved && preview.status() == FlightStatus::Crashed,
            "Normal launch/bank route missed resumed object motion or crash");
    preview.set_controls({});
    unsigned frozen_ticks = 0;
    animated = false;
    for (unsigned pass = 0; pass < 256 &&
                            preview.life_cycle().phase == LifeCyclePhase::Exploding; ++pass) {
        const auto before = preview.world().object_state();
        advance_one_tick(preview);
        if (preview.life_cycle().phase != LifeCyclePhase::Exploding) break;
        ++frozen_ticks;
        const auto& after = preview.world().object_state();
        for (std::size_t slot = 0; slot < after.records.size(); ++slot) {
            require(before.records[slot].horizontal_coordinate == after.records[slot].horizontal_coordinate,
                    "Life-end timer moved an object horizontally");
            animated |= before.records[slot].animated_kind != after.records[slot].animated_kind;
        }
    }
    require(frozen_ticks > 100 && animated &&
                preview.life_cycle().phase == LifeCyclePhase::Rebuilding,
            "Life-end fixture missed continuing animation during the motion gate");
    unsigned retained_records = 0;
    unsigned rebuild_ticks = 0;
    for (; rebuild_ticks < 85 &&
           preview.life_cycle().phase == LifeCyclePhase::Rebuilding; ++rebuild_ticks) {
        const auto before = preview.world().object_state();
        const auto rows = preview.world().generated_rows();
        advance_one_tick(preview);
        const auto delta = preview.world().generated_rows() - rows;
        const auto& after = preview.world().object_state();
        for (std::size_t slot = 0; slot < after.records.size(); ++slot) {
            const auto& old = before.records[slot];
            const auto& now = after.records[slot];
            if (old.base_kind < 7 || old.base_kind != now.base_kind ||
                now.vertical_position != static_cast<std::uint8_t>(old.vertical_position + delta))
                continue;
            ++retained_records;
            require(old.horizontal_coordinate == now.horizontal_coordinate,
                    "Rebuild moved a retained object horizontally");
        }
    }
    // The 78 foreground rebuild passes retain their motion gate; five earlier
    // reset-work ticks publish phase without advancing object motion.
    require(rebuild_ticks == 83 && retained_records > 0 &&
                preview.life_cycle().phase == LifeCyclePhase::AwaitInput,
            "Motion-gated rebuild did not return to the normal input wait");
}

void test_restart_is_deterministic() {
    using namespace std::chrono_literals;

    FlightPreview preview;
    preview.start();
    river_raid::test::wait_for_launch(preview);
    // Restart reproduces the same alive-flight activation prefix.
    std::array<WorldObjectState, 64> first_objects{};
    std::array<RiverGeneratorState, 64> first_rivers{};
    std::array<std::size_t, 64> first_rows{};
    for (std::size_t index = 0; index < first_objects.size(); ++index) {
        require(preview.status() == FlightStatus::Flying,
                "First deterministic run left active flight");
        preview.set_controls({false, false, false, false, index == 0});
        advance_one_tick(preview);
        first_objects[index] = preview.world().object_state();
        first_rivers[index] = preview.world().river_state();
        first_rows[index] = preview.world().generated_rows();
    }

    preview.start();
    river_raid::test::wait_for_launch(preview);
    for (std::size_t index = 0; index < first_objects.size(); ++index) {
        require(preview.status() == FlightStatus::Flying,
                "Restarted deterministic run left active flight");
        preview.set_controls({false, false, false, false, index == 0});
        advance_one_tick(preview);
        require(preview.world().object_state() == first_objects[index] &&
                    preview.world().river_state() == first_rivers[index] &&
                    preview.world().generated_rows() == first_rows[index],
                "Restart did not reproduce the natural preview sequence");
    }
}

void test_attract_retains_horizontal_motion() {
    FlightPreview preview;
    preview.show_options(2, true);
    bool moved = false;
    for (unsigned pass = 0; pass < 500 && !moved; ++pass) {
        const bool was_attract = preview.status() == FlightStatus::Attract;
        const auto before = preview.world().object_state();
        const auto rows = preview.world().generated_rows();
        advance_one_tick(preview);
        if (!was_attract) continue;
        const auto delta = preview.world().generated_rows() - rows;
        const auto& after = preview.world().object_state();
        for (std::size_t slot = 0; slot < after.records.size(); ++slot) {
            const auto& old = before.records[slot];
            const auto& now = after.records[slot];
            if (old.base_kind >= 7 && old.base_kind == now.base_kind &&
                now.vertical_position == static_cast<std::uint8_t>(old.vertical_position + delta))
                moved |= old.horizontal_coordinate != now.horizontal_coordinate;
        }
    }
    require(moved && preview.status() == FlightStatus::Attract &&
                preview.life_cycle().image == LifeCycleImage::Hidden,
            "Inactive demo aircraft incorrectly blocked horizontal object motion");
}

} // namespace

int main() {
    try {
        test_ready_and_initial_flight_are_bounded();
        test_natural_tick_copy_during_alive_segment();
        test_inactive_lifecycle_stops_horizontal_motion_not_animation();
        test_attract_retains_horizontal_motion();
        test_restart_is_deterministic();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
