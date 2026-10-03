#include "game/session_idle.hpp"

namespace river_raid {

void SessionIdle::begin_initial_wait() noexcept {
    state_ = {SessionIdlePhase::Waiting, 0x01};
}

void SessionIdle::begin_terminal() noexcept {
    state_ = {SessionIdlePhase::Waiting, 0xFF};
}

void SessionIdle::begin_option_wait() noexcept {
    state_ = {SessionIdlePhase::Waiting, 0x25};
}

void SessionIdle::stop() noexcept {
    state_ = {};
}

bool SessionIdle::advance(std::uint8_t sampled_raster_phase) noexcept {
    if (state_.phase == SessionIdlePhase::Inactive ||
        (sampled_raster_phase & 0x07U) != 0) {
        return false;
    }

    if (state_.countdown != 0) --state_.countdown;
    if (state_.countdown != 0) return false;

    state_.countdown = 0xFF;
    if (state_.phase != SessionIdlePhase::Waiting) return false;

    state_.phase = SessionIdlePhase::Attract;
    return true;
}

const SessionIdleState& SessionIdle::state() const noexcept {
    return state_;
}

} // namespace river_raid
