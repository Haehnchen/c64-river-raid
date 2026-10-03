#include "app/demo_setup.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void same(const river_raid::DemoSimulation& actual, const river_raid::DemoSimulation& expected) {
    require(actual.clock_state() == expected.clock_state(), "Clock differs");
    require(actual.world().generated_rows() == expected.world().generated_rows(), "Row count differs");
    require(actual.world().river_state() == expected.world().river_state(), "Generator differs");
    require(actual.world().object_state() == expected.world().object_state(), "Objects differ");
    require(actual.presentation_step() == expected.presentation_step(), "Presentation differs");
    require(std::ranges::equal(actual.world().viewport().rows(), expected.world().viewport().rows()), "Terrain differs");
}
} // namespace

int main() {
    try {
        auto sampled = river_raid::make_demo_simulation();
        auto explicit_input = river_raid::make_demo_simulation();
        unsigned reads = 0;
        const river_raid::ActivationSampleSource source = [&]() -> std::uint8_t {
            // Synthetic bytes cover the complete byte domain, not a gameplay RNG.
            return static_cast<std::uint8_t>(reads++);
        };
        for (unsigned step = 0; step < 4096; ++step) {
            const auto next_phase = static_cast<std::uint8_t>(sampled.clock_state().animation_phase + 1U);
            const bool opportunity = next_phase % 16 == 0;
            const auto reads_before = reads;
            const auto raw = static_cast<std::uint8_t>(reads_before);
            const auto nibble = static_cast<std::uint8_t>(raw % 16);
            const auto candidate = static_cast<std::uint8_t>(nibble < 12 ? nibble : nibble - 4);
            const auto expected_tick = explicit_input.advance(opportunity
                ? std::optional<std::uint8_t>{candidate} : std::nullopt);
            const auto actual_tick = sampled.advance_sampled(source);
            require(actual_tick == expected_tick, "Sampled tick differs");
            require(reads == reads_before + (opportunity ? 1U : 0U), "Source read count differs");
            same(sampled, explicit_input);
        }
        require(reads == 256, "Byte domain was not covered");
        sampled.reset();
        explicit_input.reset();
        same(sampled, explicit_input);
        require(reads == 256, "Simulation reset consumed the external source");

        bool missing = false;
        try { (void)sampled.advance_sampled({}); }
        catch (const std::invalid_argument&) { missing = true; }
        require(missing, "Missing sample source was silently accepted");
        same(sampled, explicit_input);

        (void)sampled.advance_sampled([] { return std::uint8_t{255}; });
        (void)explicit_input.advance(11);
        same(sampled, explicit_input);
        for (unsigned step = 0; step < 15; ++step) {
            (void)sampled.advance_sampled({});
            (void)explicit_input.advance();
            same(sampled, explicit_input);
        }
        bool failed = false;
        unsigned failures = 0;
        try {
            (void)sampled.advance_sampled([&]() -> std::uint8_t {
                ++failures;
                throw std::runtime_error("Synthetic source failure");
            });
        } catch (const std::runtime_error&) { failed = true; }
        require(failed && failures == 1, "Sample source failure did not propagate exactly once");
        same(sampled, explicit_input);
        (void)sampled.advance_sampled([] { return std::uint8_t{12}; });
        (void)explicit_input.advance(8);
        same(sampled, explicit_input);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
