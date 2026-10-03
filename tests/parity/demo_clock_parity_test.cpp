#include "game/demo_clock.hpp"
#include "fixtures/demo_timing_cases.hpp"
#include "demo_clock_data.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
std::array<std::uint8_t, 4> fields(const river_raid::DemoClockState& state) {
    return {state.animation_phase, state.startup_ticks_remaining,
            state.scroll_speed, state.scroll_fraction};
}
}

int main() {
    try {
        namespace data = river_raid::assets::demo_clock;
        for (const auto* cases : {&river_raid::test_data::demo_timing::pal_cases,
                                  &river_raid::test_data::demo_timing::ntsc_cases}) {
            river_raid::DemoClock clock{{data::initial_animation_phase, data::initial_startup_ticks,
                                         data::initial_scroll_speed, data::initial_scroll_fraction},
                                        {data::startup_scroll_speed, data::steady_scroll_speed}};
            for (const auto& fixture : *cases) {
                if (fields(clock.state()) != fixture.before) throw std::runtime_error("Clock PRE differs");
                const auto tick = clock.advance();
                if (fields(clock.state()) != fixture.after || tick.animation_phase != fixture.motion_phase ||
                    tick.terrain_rows != fixture.terrain_rows) {
                    throw std::runtime_error("Clock tick differs from regional capture");
                }
            }
            clock.reset();
            if (fields(clock.state()) != cases->front().before) throw std::runtime_error("Clock reset differs");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
