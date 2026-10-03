#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool audible(const std::vector<float>& samples) {
    return std::ranges::any_of(samples, [](float sample) { return sample != 0; });
}
} // namespace

int main() {
    using namespace river_raid;
    using namespace std::chrono_literals;
    try {
        FlightPreview flight;
        require(flight.advance_elapsed(20ms) == 0 && flight.take_audio().empty(),
                "Ready state generated audio");
        flight.start();
        FlightControls controls;
        controls.fire = true;
        flight.set_controls(controls);
        river_raid::test::wait_for_launch(flight);
        require(flight.advance_elapsed(20ms) == 1, "First tick not admitted");
        require(flight.audio_state().shot_countdown == 32,
                "Shot event was consumed before the audio pass");
        (void)flight.take_audio();
        require(flight.advance_elapsed(20ms) == 1, "Shot audio pass not admitted");
        require(flight.audio_state().shot_countdown == 31,
                "Pending shot event missing from the next audio pass");
        require(audible(flight.take_audio()), "Standard flight shot is silent");
        controls.left = true;
        flight.set_controls(controls);
        bool crash_sound = false;
        for (int tick = 0; tick < 5000 && flight.running(); ++tick) {
            flight.advance_elapsed(20ms);
            const auto samples = flight.take_audio();
            if (flight.audio_state().voice1_effect == Voice1AudioEffect::Collision) {
                crash_sound = crash_sound || audible(samples);
            }
        }
        require(crash_sound, "Natural crash has no life-end sound");
        require(flight.status() == FlightStatus::GameOver, "Terminal crash not reached");
        const auto rows = flight.world().generated_rows();
        const auto steering = flight.steering();
        const auto score = flight.score();
        const auto fuel = flight.fuel();
        auto previous_idle_countdown = flight.idle_state().countdown;
        auto previous_objects = flight.world().object_state();
        bool idle_advanced = false;
        bool audio_advanced = false;
        bool objects_advanced = false;
        for (int tick = 0; tick < 400; ++tick) {
            const auto completed = flight.advance_elapsed(20ms);
            const auto terminal_audio = flight.take_audio();
            if (completed != 0) {
                require(!terminal_audio.empty(), "Terminal foreground tick did not advance audio");
                audio_advanced = true;
            }
            if (flight.idle_state().countdown < previous_idle_countdown) idle_advanced = true;
            previous_idle_countdown = flight.idle_state().countdown;
            if (flight.world().object_state() != previous_objects) objects_advanced = true;
            previous_objects = flight.world().object_state();
            require(flight.world().generated_rows() == rows && flight.steering() == steering &&
                    flight.score() == score && flight.fuel() == fuel && flight.lives() == 0,
                    "Terminal foreground changed terrain, steering, score, fuel, or reserves");
        }
        require(audio_advanced, "Terminal foreground did not continue audio synthesis");
        require(objects_advanced, "Terminal foreground did not advance world objects");
        require(idle_advanced && flight.idle_state().phase == SessionIdlePhase::Waiting,
                "Terminal foreground did not advance the idle countdown");
        flight.start();
        AudioControllerState restarted;
        restarted.transition_countdown = 2;
        require(flight.audio_state() == restarted && flight.take_audio().empty(),
                "Restart retains audio state");

        FlightPreview grouped, split;
        grouped.start();
        split.start();
        grouped.set_controls(controls);
        split.set_controls(controls);
        std::vector<float> grouped_samples, split_samples;
        for (int second = 0; second < 4; ++second) {
            grouped.advance_elapsed(1s);
            const auto batch = grouped.take_audio();
            grouped_samples.insert(grouped_samples.end(), batch.begin(), batch.end());
            for (int tick = 0; tick < 50; ++tick) {
                split.advance_elapsed(20ms);
                const auto part = split.take_audio();
                split_samples.insert(split_samples.end(), part.begin(), part.end());
            }
        }
        require(grouped_samples == split_samples, "Audio depends on elapsed-time partition");
        require(grouped.audio_state() == split.audio_state(), "Audio controller cadence differs");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
