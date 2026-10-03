#include "game/idle_watchdog.hpp"

namespace river_raid {
namespace {

constexpr std::uint8_t kWrapsPerIdleStep = 8;
constexpr std::uint8_t kBlankAfterIdleSteps = 128;

} // namespace

bool IdleWatchdog::notify_phase_wrap() noexcept {
    if (state_.blanked) return false;

    ++state_.wraps_toward_eighth;
    if (state_.wraps_toward_eighth != kWrapsPerIdleStep) return false;

    state_.wraps_toward_eighth = 0;
    if (state_.idle_eighth_wraps < kBlankAfterIdleSteps) ++state_.idle_eighth_wraps;
    if (state_.idle_eighth_wraps != kBlankAfterIdleSteps) return false;

    state_.blanked = true;
    return true;
}

void IdleWatchdog::note_activity() noexcept {
    state_.idle_eighth_wraps = 0;
}

void IdleWatchdog::reset() noexcept {
    state_ = {};
}

const IdleWatchdogState& IdleWatchdog::state() const noexcept {
    return state_;
}

bool IdleWatchdog::blanked() const noexcept {
    return state_.blanked;
}

} // namespace river_raid
