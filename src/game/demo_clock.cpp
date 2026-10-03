#include "game/demo_clock.hpp"

namespace river_raid {

DemoClock::DemoClock(DemoClockState initial_state, DemoScrollSpeeds scroll_speeds) noexcept
    : initial_state_(initial_state), state_(initial_state), scroll_speeds_(scroll_speeds) {}

DemoClockTick DemoClock::advance() noexcept {
    if (state_.startup_ticks_remaining != 0) {
        if (state_.startup_ticks_remaining == 15) {
            state_.scroll_speed = scroll_speeds_.startup;
        } else if (state_.startup_ticks_remaining == 1) {
            state_.scroll_speed = scroll_speeds_.steady;
        }
        --state_.startup_ticks_remaining;
    }

    ++state_.animation_phase;
    return {state_.animation_phase, advance_scroll()};
}

void DemoClock::reset() noexcept { state_ = initial_state_; }

const DemoClockState& DemoClock::state() const noexcept { return state_; }

std::uint8_t DemoClock::advance_scroll() noexcept {
    constexpr int attempts = 2;
    constexpr std::uint8_t unconditional_scroll_speed = 0xFE;
    std::uint8_t rows = 0;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (state_.scroll_speed >= unconditional_scroll_speed) {
            ++rows;
            continue;
        }

        const auto sum = static_cast<unsigned int>(state_.scroll_fraction) +
                         static_cast<unsigned int>(state_.scroll_speed);
        state_.scroll_fraction = static_cast<std::uint8_t>(sum);
        if (sum > 0xFFU) ++rows;
    }
    return rows;
}

} // namespace river_raid
