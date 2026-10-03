#pragma once

#include "app/flight_start.hpp"

#include <chrono>
#include <stdexcept>

namespace river_raid::test {

inline void press(FlightStart& launch, StartButton button) {
    launch.set_button(button, true);
    launch.set_button(button, false);
}

inline void tick(FlightStart& launch, FlightPreview& flight) {
    for (unsigned slice = 0; slice < 4; ++slice) {
        const auto admitted = launch.advance_elapsed(std::chrono::milliseconds{10});
        (void)flight.take_audio();
        if (admitted > 1)
            throw std::runtime_error("Single-update interval admitted multiple updates");
        if (admitted == 1) return;
    }
    throw std::runtime_error("No update in bounded interval");
}

} // namespace river_raid::test
