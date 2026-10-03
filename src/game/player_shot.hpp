#pragma once

#include <cstdint>
#include <optional>

namespace river_raid {

struct PlayerShotState {
    // Zero is the inactive sentinel; every other byte is retained exactly.
    std::uint8_t vertical_position{};

    friend bool operator==(const PlayerShotState&, const PlayerShotState&) = default;
};

enum class PlayerShotPose { Hidden, Visible };

struct PlayerShotStartEvent {
    std::uint8_t sound_countdown{};
    std::uint8_t color_index{};

    friend bool operator==(const PlayerShotStartEvent&, const PlayerShotStartEvent&) = default;
};

struct PlayerShotResult {
    PlayerShotState state;
    PlayerShotPose pose;
    // No value means that presentation retains its previous vertical position.
    std::optional<std::uint8_t> vertical_position_write;
    std::optional<PlayerShotStartEvent> start_event;
};

// One admitted shot update; collisions and sound synthesis belong to their owners.
[[nodiscard]] PlayerShotResult update_player_shot(PlayerShotState state, bool fire_pressed) noexcept;
void clear_player_shot(PlayerShotResult& shot) noexcept;

} // namespace river_raid
