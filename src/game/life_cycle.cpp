#include "game/life_cycle.hpp"

#include <stdexcept>
#include <utility>

namespace river_raid {
namespace {

constexpr std::uint8_t kLifeEndStartTimer = 0xfe;
constexpr std::uint8_t kRoundStartTimer = 0x80;
constexpr std::uint8_t kRebuildStartTimer = 0x7f;
constexpr std::uint8_t kRevealTimer = 0x32;
constexpr std::uint8_t kAwaitInputTimer = 0x31;
constexpr std::uint8_t kCollisionImageChanges = 4;
constexpr std::uint8_t kRowsPerRebuildPass = 2;

bool negative(std::uint8_t value) noexcept {
    return (value & 0x80U) != 0;
}

LifeCycleImage next_explosion_image(LifeCycleImage image) noexcept {
    return image == LifeCycleImage::ExplosionA ? LifeCycleImage::ExplosionB
                                                : LifeCycleImage::ExplosionA;
}

} // namespace

LifeCycle::LifeCycle(std::uint8_t reserve_aircraft)
    : initial_reserve_aircraft_(reserve_aircraft) {
    if (reserve_aircraft > kMaximumReserveAircraft) {
        throw std::invalid_argument("Reserve aircraft count exceeds source limit");
    }
    reset();
}

void LifeCycle::start_round(LifeCyclePlayerMode mode) noexcept {
    reset();
    state_.player_mode = mode;
    if (mode == LifeCyclePlayerMode::TwoPlayer) {
        state_.active_player = 1;
        state_.inactive_reserve_aircraft = initial_reserve_aircraft_;
    }
    state_.phase = LifeCyclePhase::Exploding;
    state_.image = LifeCycleImage::Hidden;
    state_.timer = kRoundStartTimer;
}

void LifeCycle::enter_idle() noexcept {
    state_.phase = LifeCyclePhase::Terminal;
    state_.image = LifeCycleImage::Hidden;
    state_.timer = 0xff;
    state_.image_changes_remaining = 0;
    state_.cause.reset();
}

void LifeCycle::clear_session() noexcept {
    reset();
    state_.reserve_aircraft = 0;
    state_.inactive_reserve_aircraft = 0;
    state_.active_player = 0;
    enter_idle();
}

LifeCycleTransition LifeCycle::begin_life_end(LifeEndCause cause) noexcept {
    if (state_.phase != LifeCyclePhase::Active) return {};

    state_.phase = LifeCyclePhase::Exploding;
    state_.image = LifeCycleImage::ExplosionA;
    state_.timer = kLifeEndStartTimer;
    state_.image_changes_remaining =
        cause == LifeEndCause::Collision ? kCollisionImageChanges : 0;
    state_.cause = cause;

    LifeCycleTransition transition;
    transition.start_audio = cause;
    transition.clear_player_shot = true;
    return transition;
}

LifeCycleTransition LifeCycle::advance(bool any_control_pressed) noexcept {
    LifeCycleTransition transition;

    if (state_.phase == LifeCyclePhase::Active ||
        state_.phase == LifeCyclePhase::Terminal) {
        return transition;
    }

    if (state_.phase == LifeCyclePhase::AwaitInput) {
        if (!any_control_pressed) return transition;
        state_.phase = LifeCyclePhase::Active;
        state_.image = LifeCycleImage::Straight;
        state_.timer = 0;
        state_.image_changes_remaining = 0;
        state_.cause.reset();
        transition.resume_controls = true;
        return transition;
    }

    if (state_.phase == LifeCyclePhase::Rebuilding) {
        if (state_.timer > kRevealTimer) {
            --state_.timer;
            transition.forced_scroll_rows = kRowsPerRebuildPass;
            return transition;
        }

        if (state_.timer == kRevealTimer) {
            state_.timer = kAwaitInputTimer;
            state_.phase = LifeCyclePhase::AwaitInput;
            state_.image = LifeCycleImage::Straight;
            if (state_.reserve_aircraft != 0) {
                --state_.reserve_aircraft;
                transition.reserve_decremented = true;
            }
            transition.reveal_aircraft_and_clear_transients = true;
        }
        return transition;
    }

    const auto next = static_cast<std::uint8_t>(state_.timer + 1U);
    if ((next & 0x07U) == 0) {
        if (state_.image_changes_remaining != 0) {
            --state_.image_changes_remaining;
            state_.image = next_explosion_image(state_.image);
        } else {
            state_.image = LifeCycleImage::Hidden;
        }
    }

    --state_.timer;
    if (negative(state_.timer)) return transition;

    state_.image = LifeCycleImage::Hidden;
    if (state_.player_mode == LifeCyclePlayerMode::TwoPlayer &&
        state_.inactive_reserve_aircraft != 0) {
        std::swap(state_.reserve_aircraft, state_.inactive_reserve_aircraft);
        state_.active_player ^= 1U;
        transition.player_changed = true;
    }
    if (state_.reserve_aircraft == 0) {
        if (state_.player_mode == LifeCyclePlayerMode::TwoPlayer &&
            state_.active_player != 0) {
            std::swap(state_.reserve_aircraft, state_.inactive_reserve_aircraft);
            state_.active_player = 0;
            transition.player_changed = true;
        }
        state_.phase = LifeCyclePhase::Terminal;
        state_.timer = 0xff;
        transition.clear_player_shot = true;
        transition.terminal = true;
        return transition;
    }

    state_.phase = LifeCyclePhase::Rebuilding;
    state_.timer = kRebuildStartTimer;
    transition.restore_checkpoint = true;
    transition.forced_scroll_rows = kRowsPerRebuildPass;
    return transition;
}

bool LifeCycle::add_reserve_aircraft() noexcept {
    if (state_.reserve_aircraft == kMaximumReserveAircraft) return false;
    ++state_.reserve_aircraft;
    return true;
}

void LifeCycle::exhaust_reserves() noexcept { state_.reserve_aircraft = 0; }

void LifeCycle::reset() noexcept {
    state_ = {};
    state_.reserve_aircraft = initial_reserve_aircraft_;
}

const LifeCycleState& LifeCycle::state() const noexcept { return state_; }

bool LifeCycle::controls_active() const noexcept {
    return state_.phase == LifeCyclePhase::Active;
}

} // namespace river_raid
