#pragma once

#include "game/demo_clock.hpp"
#include "game/activation_sample.hpp"
#include "game/demo_presentation.hpp"
#include "game/river_world.hpp"

#include <optional>

namespace river_raid {

// Starts at the initialized demo boundary; activation samples remain external.
class DemoSimulation {
public:
    DemoSimulation(RiverWorld world, DemoClock clock, std::size_t initial_rows);
    [[nodiscard]] DemoClockTick advance(std::optional<std::uint8_t> activation_candidate = {});
    // Samples once on an activation phase, before mutating simulation state.
    [[nodiscard]] DemoClockTick advance_sampled(const ActivationSampleSource& source);
    void reset();
    [[nodiscard]] const RiverWorld& world() const noexcept;
    [[nodiscard]] const DemoClockState& clock_state() const noexcept;
    // Empty after reset; the view is replaced by the next successful advance.
    [[nodiscard]] const std::optional<DemoPresentationStep>& presentation_step() const noexcept;

private:
    RiverWorld world_;
    DemoClock clock_;
    std::size_t initial_rows_;
    std::optional<DemoPresentationStep> presentation_step_;
};

} // namespace river_raid
