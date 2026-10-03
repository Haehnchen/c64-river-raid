#include "game/player_shot.hpp"

namespace river_raid {
namespace {
constexpr std::uint8_t shot_start_basis = 0xC4;
constexpr std::uint8_t shot_step = 4;
constexpr std::uint8_t first_visible_position = 0x31;
constexpr PlayerShotStartEvent shot_start_event{0x20, 1};
} // namespace

PlayerShotResult update_player_shot(PlayerShotState state, bool fire_pressed) noexcept {
    std::optional<PlayerShotStartEvent> start_event;
    if (state.vertical_position == 0) {
        if (!fire_pressed) {
            return {state, PlayerShotPose::Hidden, std::nullopt, std::nullopt};
        }
        state.vertical_position = shot_start_basis;
        start_event = shot_start_event;
    }

    const auto vertical_position = static_cast<std::uint8_t>(state.vertical_position - shot_step);
    if (vertical_position < first_visible_position) {
        state.vertical_position = 0;
        return {state, PlayerShotPose::Hidden, vertical_position, start_event};
    }

    state.vertical_position = vertical_position;
    return {state, PlayerShotPose::Visible, vertical_position, start_event};
}

void clear_player_shot(PlayerShotResult& shot) noexcept {
    shot.state = {};
    shot.pose = PlayerShotPose::Hidden;
    shot.vertical_position_write = 0;
    // A launch earlier in this pass still emitted its event.
}

} // namespace river_raid
