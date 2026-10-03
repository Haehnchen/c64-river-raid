#include "app/demo_timing_setup.hpp"
#include "app/demo_setup.hpp"
#include "fixtures/activation_sample_cases.hpp"

#include <chrono>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check(river_raid::DemoRegion region, std::span<const river_raid::test_data::ActivationSampleCase> samples,
           std::uint32_t numerator, std::uint32_t denominator, std::uint64_t hourly_ticks) {
    using namespace river_raid;
    using namespace std::chrono_literals;
    const auto rate = demo_frame_rate(region);
    require(rate.numerator == numerator && rate.denominator == denominator, "Wrong regional model rate");
    FramePacer hour(rate);
    require(hour.advance_elapsed(1h) == hourly_ticks, "Regional hourly tick count differs");
    FramePacer chunked(rate);
    std::uint64_t total = 0;
    for (unsigned i = 0; i < 360'000; ++i) total += chunked.advance_elapsed(10ms);
    require(total == hourly_ticks, "Host polling cadence changed regional tick count");
    require(chunked.remainder() == hour.remainder(), "Host polling cadence changed regional fractional time");

    auto timed = make_timed_demo(region);
    auto expected = make_demo_simulation();
    const auto steps = samples.back().motion_call - 2U;
    const std::uint64_t duration = (std::uint64_t{steps} * denominator * 1'000'000'000 + numerator - 1) / numerator;
    timed.elapse(std::chrono::nanoseconds{static_cast<std::int64_t>(duration - 1)});
    require(timed.pending_ticks() == steps - 1, "Rational boundary rounded early");
    timed.elapse(1ns);
    require(timed.pending_ticks() == steps, "Rational boundary rounded late");
    std::size_t sample_index = 2;
    const ActivationSampleSource source = [&]() {
        require(sample_index < samples.size(), "Timed source overread");
        const auto& sample = samples[sample_index++];
        const auto next_phase = static_cast<std::uint8_t>(timed.simulation().clock_state().animation_phase + 1U);
        require(next_phase == sample.phase, "Timed sample phase differs");
        return sample.raw_sample;
    };
    std::uint64_t completed = 0;
    while (timed.pending_ticks() != 0) completed += timed.advance_pending(source, 7);
    require(completed == steps && sample_index == samples.size(), "Budgeted queue changed sample count");
    std::size_t explicit_sample = 2;
    for (std::uint32_t call = 3; call <= samples.back().motion_call; ++call) {
        std::optional<std::uint8_t> candidate;
        if (explicit_sample < samples.size() && samples[explicit_sample].motion_call == call) {
            candidate = samples[explicit_sample++].candidate;
        }
        (void)expected.advance(candidate);
    }
    require(timed.simulation().presentation_step() == expected.presentation_step(), "Pacing changed presentation");
    require(timed.simulation().world().object_state() == expected.world().object_state(), "Pacing changed objects");
    require(timed.simulation().world().river_state() == expected.world().river_state(), "Pacing changed generator");
    timed.reset();
    require(timed.pending_ticks() == 0 && !timed.simulation().presentation_step(), "Timed reset incomplete");
}
} // namespace

int main() {
    try {
        check(river_raid::DemoRegion::Pal, river_raid::test_data::pal_activation_samples, 985248, 19656, 180448);
        check(river_raid::DemoRegion::Ntsc, river_raid::test_data::ntsc_activation_samples, 1022730, 17095, 215374);
        bool rejected = false;
        try { (void)river_raid::make_timed_demo(static_cast<river_raid::DemoRegion>(-1)); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Unknown region accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
