#pragma once

#include "game/flight_controls.hpp"

#include <SDL3/SDL.h>

#include <array>

namespace river_raid {

// Reads two raw SDL joysticks. Axis 0/1 and hat 0 control movement; button 0 fires.
// The joystick subsystem must be initialized by the caller.
class SdlJoysticks {
public:
    SdlJoysticks() = default;
    ~SdlJoysticks();

    SdlJoysticks(const SdlJoysticks&) = delete;
    SdlJoysticks& operator=(const SdlJoysticks&) = delete;

    // Keeps connected devices in their existing slots and fills vacant slots in
    // SDL enumeration order. Call after joystick add/remove notifications.
    void refresh_devices();

    // Samples the latest state, so disconnects and neutral inputs cannot remain held.
    [[nodiscard]] JoystickControls sample() const noexcept;

private:
    struct Slot {
        SDL_JoystickID instance_id{};
        SDL_Joystick* handle{};
    };

    void close_slot(Slot& slot) noexcept;

    std::array<Slot, 2> slots_{};
};

} // namespace river_raid
