#include "game/timed_demo.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace river_raid {

TimedDemo::TimedDemo(DemoSimulation simulation, FramePacer pacer)
    : simulation_(std::move(simulation)), pacer_(std::move(pacer)) {}

void TimedDemo::elapse(std::chrono::nanoseconds elapsed) {
    auto next_pacer = pacer_;
    const auto due = next_pacer.advance_elapsed(elapsed);
    if (due > std::numeric_limits<std::uint64_t>::max() - pending_) {
        throw std::overflow_error("Pending demo tick count overflow");
    }
    pacer_ = std::move(next_pacer);
    pending_ += due;
}

std::uint64_t TimedDemo::advance_pending(const ActivationSampleSource& source, std::uint64_t max_steps) {
    std::uint64_t completed = 0;
    while (pending_ != 0 && completed < max_steps) {
        (void)simulation_.advance_sampled(source);
        --pending_;
        ++completed;
    }
    return completed;
}

void TimedDemo::reset() {
    simulation_.reset();
    pacer_.reset();
    pending_ = 0;
}

std::uint64_t TimedDemo::pending_ticks() const noexcept { return pending_; }
const DemoSimulation& TimedDemo::simulation() const noexcept { return simulation_; }

} // namespace river_raid
