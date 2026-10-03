#include "game/player_steering.hpp"

namespace river_raid {

PlayerSteeringResult steer_player(PlayerSteeringState state, FlightControls controls,
                                 PlayerSteeringContext context) noexcept {
    auto pose = FlightPose::Straight;
    if (!controls.left && !controls.right) {
        state.horizontal_step = 0;
        state.horizontal_fraction = 0;
    } else {
        const auto accelerated = static_cast<unsigned>(state.horizontal_step) + 16U;
        const bool whole_step = accelerated > 255U;
        if (!whole_step) state.horizontal_step = static_cast<std::uint8_t>(accelerated);
        const auto add_fraction = [&] {
            const auto base = context.use_max_fraction_for_rightward_add ? 255U
                : static_cast<unsigned>(state.horizontal_fraction);
            const auto sum = base + state.horizontal_step;
            if (sum > 255U) ++state.horizontal_position;
            state.horizontal_fraction = static_cast<std::uint8_t>(sum);
        };
        if (controls.right) {
            pose = FlightPose::Right;
            if (whole_step) {
                ++state.horizontal_position;
                state.horizontal_fraction = 0;
            } else {
                add_fraction();
            }
        } else {
            pose = FlightPose::Left;
            if (whole_step) {
                --state.horizontal_position;
                if (state.horizontal_position == 0) ++state.horizontal_position;
                state.horizontal_fraction = 8;
            } else {
                const int difference = static_cast<int>(state.horizontal_fraction) - state.horizontal_step;
                if (difference >= 0) {
                    state.horizontal_fraction = static_cast<std::uint8_t>(difference);
                } else {
                    --state.horizontal_position;
                    if (state.horizontal_position == 0) {
                        // The origin crossing reuses the old fraction in the add path.
                        add_fraction();
                    } else {
                        state.horizontal_fraction = static_cast<std::uint8_t>(difference);
                    }
                }
            }
        }
    }

    if (controls.up) {
        if (state.scroll_speed < 254U) state.scroll_speed = static_cast<std::uint8_t>(state.scroll_speed + 2U);
    } else if (controls.down) {
        if (state.scroll_speed >= 65U) state.scroll_speed = static_cast<std::uint8_t>(state.scroll_speed - 2U);
    } else if (state.scroll_speed < 128U) {
        state.scroll_speed = static_cast<std::uint8_t>(state.scroll_speed + 2U);
    } else if (state.scroll_speed > 128U) {
        state.scroll_speed = static_cast<std::uint8_t>(state.scroll_speed - 2U);
    }
    return {state, pose};
}

} // namespace river_raid
