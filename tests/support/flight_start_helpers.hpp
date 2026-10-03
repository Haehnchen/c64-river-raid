#pragma once

#include "app/flight_preview.hpp"

#include <chrono>
#include <cstddef>
#include <stdexcept>

namespace river_raid::test {

// Advance startup one admitted presentation/foreground tick at a time. Held controls remain
// on FlightPreview; the helper does not start, reset, or resume the flight.
inline void wait_for_launch(FlightPreview& flight) {
    using namespace std::chrono_literals;
    for (unsigned tick = 0; tick < 128 &&
                             flight.life_cycle().phase != LifeCyclePhase::AwaitInput; ++tick) {
        std::size_t completed = 0;
        for (int slice = 0; slice < 4 && completed == 0; ++slice) {
            completed = flight.advance_elapsed(10ms);
            if (completed > 1) {
                throw std::runtime_error("Startup slice admitted multiple ticks");
            }
        }
        if (completed != 1) {
            throw std::runtime_error("Startup did not admit one tick");
        }
    }
    if (flight.life_cycle().phase != LifeCyclePhase::AwaitInput) {
        throw std::runtime_error("Startup did not reach its input-gated launch");
    }
}

} // namespace river_raid::test
