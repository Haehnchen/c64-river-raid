#pragma once

#include "game/demo_simulation.hpp"
#include "game/frame_pacer.hpp"

#include <chrono>
#include <cstdint>

namespace river_raid {

// Wall time is supplied by the owner; pending ticks are never silently dropped.
class TimedDemo {
public:
    TimedDemo(DemoSimulation simulation, FramePacer pacer);
    void elapse(std::chrono::nanoseconds elapsed);
    [[nodiscard]] std::uint64_t advance_pending(const ActivationSampleSource& source,
                                              std::uint64_t max_steps);
    void reset();
    [[nodiscard]] std::uint64_t pending_ticks() const noexcept;
    [[nodiscard]] const DemoSimulation& simulation() const noexcept;

private:
    DemoSimulation simulation_;
    FramePacer pacer_;
    std::uint64_t pending_{};
};

} // namespace river_raid
