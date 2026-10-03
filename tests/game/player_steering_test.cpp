#include "game/player_steering.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        using namespace river_raid;
        constexpr FlightControls right{false, false, false, true};
        constexpr FlightControls left{false, false, true, false};
        PlayerSteeringState state{80, 0, 0, 128};
        for (unsigned tick = 0; tick < 15; ++tick) state = steer_player(state, right).state;
        require(state == PlayerSteeringState{87, 128, 240, 128}, "Right acceleration differs");
        auto result = steer_player(state, right);
        require(result.state == PlayerSteeringState{88, 0, 240, 128} && result.pose == FlightPose::Right,
                "Whole right step differs");
        state = {80, 0, 0, 128};
        for (unsigned tick = 0; tick < 15; ++tick) state = steer_player(state, left).state;
        require(state == PlayerSteeringState{72, 128, 240, 128}, "Left acceleration differs");
        result = steer_player(state, left);
        require(result.state == PlayerSteeringState{71, 8, 240, 128} && result.pose == FlightPose::Left,
                "Whole left step differs");
        state = steer_player({80, 0, 0, 128}, right).state;
        result = steer_player(state, left);
        require(result.state == PlayerSteeringState{79, 240, 32, 128}, "Direction change reset acceleration");
        result = steer_player(result.state, {});
        require(result.state == PlayerSteeringState{79, 0, 0, 128} && result.pose == FlightPose::Straight,
                "Neutral direction failed to reset fractional motion");

        require(steer_player({1, 0, 0, 128}, left).state == PlayerSteeringState{0, 16, 16, 128},
                "Left origin crossing used ordinary subtraction");
        require(steer_player({1, 0, 0, 128}, left, {true}).state == PlayerSteeringState{1, 15, 16, 128},
                "Origin crossing ignored fraction bias");
        require(steer_player({64, 0, 0, 128}, right, {true}).state == PlayerSteeringState{65, 15, 16, 128},
                "Right addition ignored fraction bias");
        require(steer_player({255, 0, 255, 128}, right).state == PlayerSteeringState{0, 0, 255, 128},
                "Whole right step failed to wrap");
        require(steer_player({1, 0, 240, 128}, left).state == PlayerSteeringState{1, 8, 240, 128},
                "Whole-step origin case differs");
        require(steer_player({0, 0, 0, 128}, left).state == PlayerSteeringState{255, 240, 16, 128},
                "Left position wrap differs");
        require(steer_player({255, 255, 0, 128}, right).state == PlayerSteeringState{0, 15, 16, 128},
                "Right position wrap differs");
        require(steer_player({80, 0, 239, 128}, right).state.horizontal_step == 255,
                "Nonlattice step was incorrectly clamped");
        require(steer_player({80, 0, 241, 128}, right).state.horizontal_step == 241,
                "Overflow overwrote the previous step");

        for (unsigned flags = 0; flags < 16; ++flags) {
            const FlightControls input{bool(flags & 1), bool(flags & 2), bool(flags & 4), bool(flags & 8)};
            const auto pose = input.right ? FlightPose::Right : input.left ? FlightPose::Left : FlightPose::Straight;
            for (unsigned speed = 0; speed < 256; ++speed) {
                const auto value = static_cast<std::uint8_t>(speed);
                const auto actual = steer_player({80, 0, 0, value}, input);
                require(actual.pose == pose, "Horizontal input priority differs");
                unsigned expected;
                if (input.up) expected = speed + 2 <= 255 ? speed + 2 : speed;
                else if (input.down) expected = speed < 65 ? speed : speed - 2;
                else expected = speed == 128 ? speed : speed < 128 ? speed + 2 : speed - 2;
                require(actual.state.scroll_speed == expected, "Speed rule differs");
            }
        }
        result = steer_player({80, 0, 0, 127}, {});
        require(result.state.scroll_speed == 129 && steer_player(result.state, {}).state.scroll_speed == 127,
                "Odd neutral speed was incorrectly clamped");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
