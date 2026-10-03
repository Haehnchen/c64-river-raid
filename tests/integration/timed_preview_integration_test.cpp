#include "app/demo_timing_setup.hpp"
#include "app/demo_setup.hpp"
#include "app/world_scene_setup.hpp"
#include "platform/sdl_preview.hpp"
#include "fixtures/activation_sample_cases.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        using namespace river_raid;
        auto timed = make_timed_demo(DemoRegion::Pal);
        auto expected = make_demo_simulation();
        std::uint64_t now = 0;
        std::size_t sample = 2;
        std::uint64_t completed = 0;
        unsigned renders = 0;
        unsigned elapsed_calls = 0;
        const ActivationSampleSource source = [&] {
            const auto& input = test_data::pal_activation_samples.at(sample++);
            const auto next_phase = static_cast<std::uint8_t>(timed.simulation().clock_state().animation_phase + 1U);
            require(input.phase == next_phase, "SDL delivery changed activation phase");
            return input.raw_sample;
        };
        const PreviewCallbacks callbacks{
            [&] {
                ++renders;
                if (renders == 1) {
                    SDL_Event event{};
                    event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                    require(SDL_PushEvent(&event), "Cannot push focus event");
                }
                if (renders == 2) now = 1'000'000'000;
                if (renders == 3) now = 2'000'000'000;
                if (timed.simulation().presentation_step()) return make_demo_scene(*timed.simulation().presentation_step());
                return Frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff000000)};
            }, {}, {},
            [&](std::chrono::nanoseconds elapsed) {
                ++elapsed_calls;
                timed.elapse(elapsed);
                completed += timed.advance_pending(source, 64);
            }};
        require(show_preview(callbacks, true, PreviewTiming{[&] { return now; }}) == 0, "Timed preview failed");
        require(renders == 4 && elapsed_calls == 3, "Unexpected SDL timing/render cadence");
        require(completed == 100 && timed.pending_ticks() == 0 && sample == 9, "SDL timed step count differs");
        std::size_t expected_sample = 2;
        for (unsigned call = 3; call < 103; ++call) {
            std::optional<std::uint8_t> candidate;
            if (test_data::pal_activation_samples.at(expected_sample).motion_call == call) {
                candidate = test_data::pal_activation_samples.at(expected_sample++).candidate;
            }
            (void)expected.advance(candidate);
        }
        require(timed.simulation().presentation_step() == expected.presentation_step(), "SDL timing changed presentation");
        require(timed.simulation().world().object_state() == expected.world().object_state(), "SDL timing changed objects");
        require(timed.simulation().world().river_state() == expected.world().river_state(), "SDL timing changed generator");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
