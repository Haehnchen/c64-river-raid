#include "game/demo_clock.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_state(const river_raid::DemoClock& clock,
                  river_raid::DemoClockState expected,
                  const char* message) {
    expect(clock.state() == expected, message);
}

void test_startup_countdown_and_phase_wrap() {
    using river_raid::DemoClock;

    DemoClock clock({255, 15, 0, 0}, {254, 128});
    const auto first = clock.advance();
    expect(first == river_raid::DemoClockTick{0, 2}, "phase wrap or startup rows");
    expect_state(clock, {0, 14, 254, 0}, "countdown 15 did not select startup speed");

    for (int tick = 0; tick < 13; ++tick) {
        const auto result = clock.advance();
        expect(result.terrain_rows == 2, "startup speed must produce two rows");
    }
    expect_state(clock, {13, 1, 254, 0}, "countdown 1 boundary not reached");

    const auto final_startup = clock.advance();
    expect(final_startup == river_raid::DemoClockTick{14, 1},
           "countdown 1 must use steady fractional scrolling");
    expect_state(clock, {14, 0, 128, 0}, "countdown did not settle at steady speed");

    const auto steady = clock.advance();
    expect(steady == river_raid::DemoClockTick{15, 1}, "steady speed row cadence");
    expect_state(clock, {15, 0, 128, 0}, "steady fraction did not carry to zero");
}

void test_scroll_speed_boundaries_and_fraction_carry() {
    using river_raid::DemoClock;
    using river_raid::DemoClockState;

    for (const auto speed : {std::uint8_t{0}, std::uint8_t{64}, std::uint8_t{128},
                             std::uint8_t{253}}) {
        DemoClock clock({0, 0, speed, 0}, {0, speed});
        const auto first = clock.advance();
        const auto second = clock.advance();
        if (speed == 0) {
            expect(first.terrain_rows == 0 && second.terrain_rows == 0,
                   "zero speed generated terrain");
            expect_state(clock, {2, 0, 0, 0}, "zero speed state");
        } else if (speed == 64) {
            expect(first.terrain_rows == 0 && second.terrain_rows == 1,
                   "64-speed fraction carry");
            expect_state(clock, {2, 0, 64, 0}, "64-speed fraction reset");
        } else if (speed == 128) {
            expect(first.terrain_rows == 1 && second.terrain_rows == 1,
                   "128-speed two-attempt cadence");
            expect_state(clock, {2, 0, 128, 0}, "128-speed fraction reset");
        } else {
            expect(first.terrain_rows == 1 && second.terrain_rows == 2,
                   "253-speed carry and overflow");
            expect_state(clock, {2, 0, 253, 244}, "253-speed wrapped fraction");
        }
    }

    for (const auto speed : {std::uint8_t{254}, std::uint8_t{255}}) {
        DemoClock clock({0, 0, speed, 77}, {0, speed});
        const auto result = clock.advance();
        expect(result.terrain_rows == 2, "unconditional speed must produce two rows");
        expect_state(clock, {1, 0, speed, 77}, "unconditional speed changed fraction");
    }
}

void test_reset_restores_initial_state() {
    river_raid::DemoClock clock({42, 1, 128, 200}, {254, 64});
    (void)clock.advance();
    expect(clock.state() != river_raid::DemoClockState{42, 1, 128, 200},
           "advance did not change state before reset");
    clock.reset();
    expect_state(clock, {42, 1, 128, 200}, "reset did not restore initial state");
}

} // namespace

int main() {
    try {
        test_startup_countdown_and_phase_wrap();
        test_scroll_speed_boundaries_and_fraction_carry();
        test_reset_restores_initial_state();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
