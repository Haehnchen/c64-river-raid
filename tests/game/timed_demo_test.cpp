#include "app/demo_setup.hpp"
#include "game/timed_demo.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void same(const river_raid::DemoSimulation& actual, const river_raid::DemoSimulation& expected) {
    require(actual.clock_state() == expected.clock_state(), "Clock differs");
    require(actual.world().generated_rows() == expected.world().generated_rows(), "Rows differ");
    require(actual.world().river_state() == expected.world().river_state(), "Generator differs");
    require(actual.world().object_state() == expected.world().object_state(), "Objects differ");
    require(actual.presentation_step() == expected.presentation_step(), "Presentation differs");
    require(std::ranges::equal(actual.world().viewport().rows(), expected.world().viewport().rows()), "Terrain differs");
}
} // namespace

int main() {
    try {
        using namespace std::chrono_literals;
        using namespace river_raid;
        TimedDemo timed(make_demo_simulation(), FramePacer({16, 1}));
        auto explicit_input = make_demo_simulation();
        unsigned reads = 0;
        const ActivationSampleSource source = [&] { ++reads; return std::uint8_t{255}; };
        require(timed.advance_pending({}, 5) == 0, "Empty queue sampled a source");
        timed.elapse(31250us);
        require(timed.pending_ticks() == 0, "Premature tick");
        timed.elapse(31250us);
        require(timed.pending_ticks() == 1, "Fractional time was lost");
        require(timed.advance_pending(source, 0) == 0 && reads == 0, "Zero budget consumed a tick");
        bool missing = false;
        try { (void)timed.advance_pending({}, 1); }
        catch (const std::invalid_argument&) { missing = true; }
        require(missing && timed.pending_ticks() == 1, "Missing source lost pending work");
        same(timed.simulation(), explicit_input);
        require(timed.advance_pending(source, 1) == 1 && reads == 1, "First tick did not complete");
        (void)explicit_input.advance(11);
        same(timed.simulation(), explicit_input);
        timed.elapse(2s);
        require(timed.pending_ticks() == 32, "Elapsed time did not accumulate");
        require(timed.advance_pending(source, 5) == 5 && timed.pending_ticks() == 27, "Catch-up budget lost ticks");
        for (unsigned i = 0; i < 5; ++i) (void)explicit_input.advance();
        same(timed.simulation(), explicit_input);

        bool failed = false;
        try {
            (void)timed.advance_pending([]() -> std::uint8_t { throw std::runtime_error("Synthetic source failure"); }, 27);
        } catch (const std::runtime_error&) { failed = true; }
        // Ten nonsampling ticks finish; phase 16 remains pending.
        require(failed && timed.pending_ticks() == 17, "Failed sample lost or duplicated a tick");
        for (unsigned i = 0; i < 10; ++i) (void)explicit_input.advance();
        same(timed.simulation(), explicit_input);
        require(timed.advance_pending(source, 17) == 17 && timed.pending_ticks() == 0, "Retry did not drain queue");
        for (unsigned i = 0; i < 17; ++i) (void)explicit_input.advance(i % 16 == 0
            ? std::optional<std::uint8_t>{11} : std::nullopt);
        same(timed.simulation(), explicit_input);

        timed.elapse(1ns);
        bool negative = false;
        try { timed.elapse(-1ns); } catch (const std::invalid_argument&) { negative = true; }
        require(negative && timed.pending_ticks() == 0, "Negative elapsed time changed queue");
        timed.elapse(62499999ns);
        require(timed.pending_ticks() == 1, "Rejected time changed fractional remainder");
        timed.reset();
        explicit_input.reset();
        require(timed.pending_ticks() == 0 && reads == 3, "Reset changed external source or retained ticks");
        same(timed.simulation(), explicit_input);
        timed.elapse(62499999ns);
        require(timed.pending_ticks() == 0, "Reset retained fractional time");

        // Near-limit queued work must also leave fractional time unchanged on failure.
        TimedDemo full(make_demo_simulation(), FramePacer({1'000'000'000, 1}));
        const auto huge = std::chrono::nanoseconds::max();
        full.elapse(huge);
        full.elapse(huge);
        require(full.pending_ticks() == std::numeric_limits<std::uint64_t>::max() - 1, "Large time accounting differs");
        bool overflow = false;
        try { full.elapse(2ns); } catch (const std::overflow_error&) { overflow = true; }
        require(overflow && full.pending_ticks() == std::numeric_limits<std::uint64_t>::max() - 1, "Overflow changed queue");
        full.elapse(1ns);
        require(full.pending_ticks() == std::numeric_limits<std::uint64_t>::max(), "Overflow recovery lost time");

        TimedDemo fractional_full(make_demo_simulation(), FramePacer({1'000'000'000, 3}));
        for (unsigned i = 0; i < 6; ++i) fractional_full.elapse(huge);
        fractional_full.elapse(1ns);
        require(fractional_full.pending_ticks() == std::numeric_limits<std::uint64_t>::max() - 1, "Fractional near-limit queue differs");
        overflow = false;
        try { fractional_full.elapse(8ns); } catch (const std::overflow_error&) { overflow = true; }
        require(overflow, "Fractional queue overflow accepted");
        fractional_full.elapse(2ns);
        require(fractional_full.pending_ticks() == std::numeric_limits<std::uint64_t>::max(), "Overflow consumed fractional time");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
