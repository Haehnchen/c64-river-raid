#pragma once

#include "app/flight_preview.hpp"
#include "game/start_flow.hpp"
#include "game/title_delay.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace river_raid {

// Shared title, menu, and native playable session.
class FlightStart {
public:
    FlightStart(StartFlow& flow, FlightPreview& flight);

    [[nodiscard]] std::uint64_t start_generation() const noexcept { return start_generation_; }

    std::size_t advance_elapsed(std::chrono::nanoseconds elapsed);

    void set_button(StartButton button, bool pressed);

    void set_controls(FlightControls controls);

    void set_joystick_controls(JoystickControls controls);

private:
    std::size_t advance_session(std::chrono::nanoseconds elapsed);

    [[nodiscard]] std::uint8_t wake_sample() const noexcept;

    StartFlow& flow_;
    FlightPreview& flight_;
    FlightControls controls_{};
    JoystickControls joystick_controls_{};
    TitleDelay title_delay_;
    std::uint8_t previous_wake_sample_{};
    std::uint64_t start_generation_{};
};

} // namespace river_raid
