#include "app/demo_setup.hpp"
#include "fixtures/activation_sample_cases.hpp"
#include "fixtures/object_motion_cases.hpp"

#include <algorithm>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check(std::span<const river_raid::test_data::ActivationSampleCase> samples, bool pal) {
    using namespace river_raid;
    require(samples.size() == 27, "Incomplete regional sample fixture");
    for (const auto& sample : samples) {
        require(is_activation_phase(sample.phase), "Original sample outside an activation phase");
        require(activation_candidate_from_sample(sample.raw_sample) == sample.candidate,
                "Raw sample projection differs from original candidate");
    }
    require(samples[0].motion_call == 1 && samples[1].motion_call == 2 &&
            samples[2].motion_call == 3, "Unexpected initialization boundary");
    auto sampled = make_demo_simulation();
    auto explicit_input = make_demo_simulation();
    std::size_t next_sample = 2;
    std::uint32_t call = 3;
    const ActivationSampleSource source = [&]() -> std::uint8_t {
        require(next_sample < samples.size(), "Unobserved source read");
        const auto& sample = samples[next_sample++];
        require(sample.motion_call == call, "Sample source read on the wrong original call");
        const auto next_phase = static_cast<std::uint8_t>(sampled.clock_state().animation_phase + 1U);
        require(sample.phase == next_phase, "Original sample and native phase differ");
        return sample.raw_sample;
    };
    for (; call <= samples.back().motion_call; ++call) {
        const bool opportunity = next_sample < samples.size() && samples[next_sample].motion_call == call;
        const auto candidate = opportunity ? std::optional<std::uint8_t>{samples[next_sample].candidate} : std::nullopt;
        const auto before_reads = next_sample;
        const auto expected_tick = explicit_input.advance(candidate);
        require(sampled.advance_sampled(source) == expected_tick, "Regional sampled tick differs");
        require(next_sample == before_reads + (opportunity ? 1U : 0U), "Original sampling cadence differs");
        require(sampled.presentation_step() == explicit_input.presentation_step(), "Regional presentation differs");
        require(sampled.world().object_state() == explicit_input.world().object_state(), "Regional objects differ");
        require(sampled.world().river_state() == explicit_input.world().river_state(), "Regional generator differs");
        require(sampled.clock_state() == explicit_input.clock_state(), "Regional clock differs");
        require(std::ranges::equal(sampled.world().viewport().rows(), explicit_input.world().viewport().rows()),
                "Regional terrain differs");
        if (pal) {
            // The next PRE snapshot follows this motion and its terrain updates.
            const auto& expected = test_data::object_motion_cases.at(call);
            require(expected.call == call + 1, "Invalid original object checkpoint");
            require(sampled.world().generated_rows() == expected.rows_before, "Original row count differs");
            const auto& actual = sampled.world().object_state();
            for (std::size_t slot = 0; slot < actual.records.size(); ++slot) {
                const auto& record = expected.records[slot];
                require(actual.records[slot] == WorldObjectRecord{record.vertical_position,
                    record.horizontal_coordinate, record.behavior_flags, record.animated_kind,
                    record.base_kind, record.left_bound, record.right_bound, record.animation_count},
                    "Raw sample driven PAL objects differ from original");
            }
        }
    }
    require(next_sample == samples.size(), "Not all original samples were consumed");
}
} // namespace

int main() {
    try {
        check(river_raid::test_data::pal_activation_samples, true);
        check(river_raid::test_data::ntsc_activation_samples, false);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
