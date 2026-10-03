#pragma once

#include <array>

namespace river_raid {

// Keep opposing inputs distinct until the game's priority rules are applied.
struct FlightControls {
    bool up{};
    bool down{};
    bool left{};
    bool right{};
    bool fire{};

    friend bool operator==(const FlightControls&, const FlightControls&) = default;
};

using JoystickControls = std::array<FlightControls, 2>;

[[nodiscard]] constexpr FlightControls combine_controls(FlightControls first,
                                                        FlightControls second) noexcept {
    return {first.up || second.up, first.down || second.down,
            first.left || second.left, first.right || second.right,
            first.fire || second.fire};
}

} // namespace river_raid
