#include "game/demo_simulation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace river_raid {

DemoSimulation::DemoSimulation(RiverWorld world, DemoClock clock, std::size_t initial_rows)
    : world_(std::move(world)), clock_(std::move(clock)), initial_rows_(initial_rows) {
    reset();
}

DemoClockTick DemoSimulation::advance(std::optional<std::uint8_t> activation_candidate) {
    if (activation_candidate && *activation_candidate >= kWorldObjectCapacity) {
        throw std::invalid_argument("Invalid demo activation candidate");
    }
    DemoPresentationStep presentation;
    presentation.scroll_phase = world_.river_state().phase;
    presentation.rows_before = world_.generated_rows();
    presentation.before_motion = world_.object_state();
    std::ranges::copy(world_.viewport().rows(), presentation.terrain_before_scroll.begin());
    const auto tick = clock_.advance();
    presentation.animation_phase = tick.animation_phase;
    presentation.row_count = tick.terrain_rows;
    world_.advance_motion({tick.animation_phase, world_.river_state().section >= 2,
                           activation_candidate});
    presentation.after_motion = world_.object_state();
    for (unsigned row = 0; row < tick.terrain_rows; ++row) {
        (void)world_.advance();
        presentation.after_rows[row] = world_.object_state();
    }
    presentation_step_ = std::move(presentation);
    return tick;
}

DemoClockTick DemoSimulation::advance_sampled(const ActivationSampleSource& source) {
    const auto next_phase = static_cast<std::uint8_t>(clock_.state().animation_phase + 1U);
    if (!is_activation_phase(next_phase)) return advance();
    if (!source) throw std::invalid_argument("Activation phase requires a sample source");
    const auto candidate = activation_candidate_from_sample(source());
    return advance(candidate);
}

void DemoSimulation::reset() {
    presentation_step_.reset();
    clock_.reset();
    world_.reset();
    for (std::size_t row = 0; row < initial_rows_; ++row) (void)world_.advance();
}

const RiverWorld& DemoSimulation::world() const noexcept { return world_; }
const DemoClockState& DemoSimulation::clock_state() const noexcept { return clock_.state(); }
const std::optional<DemoPresentationStep>& DemoSimulation::presentation_step() const noexcept {
    return presentation_step_;
}

} // namespace river_raid
