#include "game/player_shot.hpp"

#include <array>
#include <cstdint>
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

        auto result = update_player_shot({}, false);
        require(result.state == PlayerShotState{0} && result.pose == PlayerShotPose::Hidden,
                "Idle shot did not stay hidden");
        require(!result.vertical_position_write && !result.start_event,
                "Idle shot overwrote retained presentation state");

        result = update_player_shot({}, true);
        require(result.state == PlayerShotState{0xC0} && result.pose == PlayerShotPose::Visible,
                "Shot did not start at its first visible position");
        require(result.vertical_position_write == 0xC0,
                "Shot start wrote the wrong vertical position");
        require(result.start_event == PlayerShotStartEvent{0x20, 1},
                "Shot start event differs");

        result = update_player_shot(result.state, true);
        require(result.state == PlayerShotState{0xBC} && result.pose == PlayerShotPose::Visible,
                "Held fire did not advance the active shot");
        require(result.vertical_position_write == 0xBC && !result.start_event,
                "Active shot emitted another start event");

        result = update_player_shot(result.state, false);
        require(result.state == PlayerShotState{0xB8} && result.pose == PlayerShotPose::Visible,
                "Fire release cancelled the active shot");

        result = update_player_shot({0x35}, false);
        require(result.state == PlayerShotState{0x31} && result.pose == PlayerShotPose::Visible &&
                    result.vertical_position_write == 0x31,
                "Last visible position differs");
        result = update_player_shot({0x34}, false);
        require(result.state == PlayerShotState{0} && result.pose == PlayerShotPose::Hidden &&
                    result.vertical_position_write == 0x30,
                "Despawn boundary did not retain the position write");
        require(!result.start_event, "Despawn emitted a start event");

        constexpr std::array<std::uint8_t, 3> wrapped_positions{0xFD, 0xFE, 0xFF};
        for (std::uint8_t input = 1; input <= 3; ++input) {
            result = update_player_shot({input}, false);
            require(result.state.vertical_position == wrapped_positions[input - 1] &&
                        result.pose == PlayerShotPose::Visible &&
                        result.vertical_position_write == wrapped_positions[input - 1],
                    "Low synthetic position did not wrap as an eight-bit subtraction");
        }
        result = update_player_shot({4}, false);
        require(result.state == PlayerShotState{0} && result.pose == PlayerShotPose::Hidden &&
                    result.vertical_position_write == 0,
                "Position four did not despawn through zero");

        for (unsigned input = 1; input < 256; ++input) {
            for (const bool fire_pressed : {false, true}) {
                const auto position = static_cast<std::uint8_t>(input - 4U);
                const bool visible = position >= 0x31;
                result = update_player_shot({static_cast<std::uint8_t>(input)}, fire_pressed);
                require(result.state.vertical_position == (visible ? position : 0) &&
                            result.pose == (visible ? PlayerShotPose::Visible : PlayerShotPose::Hidden) &&
                            result.vertical_position_write == position && !result.start_event,
                        "Nonzero byte-state rule differs");
            }
        }

        PlayerShotState held_state{};
        unsigned visible_ticks = 0;
        unsigned start_events = 0;
        for (;;) {
            result = update_player_shot(held_state, true);
            held_state = result.state;
            if (result.start_event) ++start_events;
            if (result.pose == PlayerShotPose::Hidden) break;
            ++visible_ticks;
        }
        require(visible_ticks == 36 && start_events == 1 && held_state == PlayerShotState{0},
                "Held-fire lifetime differs");
        result = update_player_shot(held_state, true);
        require(result.state == PlayerShotState{0xC0} && result.pose == PlayerShotPose::Visible &&
                    result.start_event.has_value(),
                "Held fire did not restart after despawn");
        const auto launch = result.start_event;
        clear_player_shot(result);
        require(result.state == PlayerShotState{0} && result.pose == PlayerShotPose::Hidden &&
                    result.vertical_position_write == 0 && result.start_event == launch,
                "Shared hit clear retained projectile state or erased its earlier launch event");
        clear_player_shot(result);
        require(result.start_event == launch && result.vertical_position_write == 0,
                "Repeated hit clear changed the retained launch event");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
