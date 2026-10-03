#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void advance_one_tick(river_raid::FlightPreview& preview) {
    using namespace std::chrono_literals;
    for (int slice = 0; slice < 4; ++slice) {
        const auto completed = preview.advance_elapsed(10ms);
        require(completed <= 1, "Preview slice admitted multiple ticks");
        if (completed == 1) return;
    }
    throw std::runtime_error("Preview tick did not advance");
}

} // namespace

int main() {
    try {
        using namespace std::chrono_literals;
        using namespace river_raid;

        FlightPreview preview;
        const auto initial_steering = preview.steering();
        const auto initial_river = preview.world().river_state();
        require(initial_steering == PlayerSteeringState{86, 0, 0, 0x40},
                "Flight preview did not use the verified round-entry state");
        require(!preview.running(), "Preview unexpectedly starts running");
        require(preview.world().generated_rows() == 0,
                "Preview constructor unexpectedly generated flight rows");
        require(preview.advance_elapsed(1s) == 0 && preview.steering() == initial_steering &&
                    preview.world().river_state() == initial_river,
                "Inactive preview advanced");

        preview.start();
        preview.set_controls({false, false, false, true});
        require(preview.status() == FlightStatus::Preparing && preview.running() &&
                    preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                    preview.life_cycle().image == LifeCycleImage::Hidden &&
                    preview.presented_image() == LifeCycleImage::Hidden &&
                    preview.life_cycle().timer == 0x80 && preview.lives() == 4 &&
                    preview.world().generated_rows() == 0,
                "Start did not enter the empty preparing lifecycle");
        advance_one_tick(preview);
        require(preview.status() == FlightStatus::Preparing &&
                    preview.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    preview.life_cycle().image == LifeCycleImage::Hidden &&
                    preview.presented_image() == LifeCycleImage::Hidden &&
                    preview.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                    preview.world().generated_rows() == 0,
                "Cold reset did not defer its first two rows");
        for (int busy = 0; busy < 5; ++busy) {
            advance_one_tick(preview);
            if (busy < 4) {
                require(preview.checkpoint_reset_work() ==
                            CheckpointResetWorkState{static_cast<std::uint8_t>(4 - busy), 2} &&
                            preview.world().generated_rows() == 0 &&
                            preview.life_cycle().phase == LifeCyclePhase::Rebuilding,
                        "Held right input advanced the pending cold reset");
            }
        }
        require(!preview.checkpoint_reset_work() && preview.world().generated_rows() == 2,
                "Cold reset did not publish its deferred rows exactly once");
        test::wait_for_launch(preview);
        require(preview.status() == FlightStatus::Flying &&
                    preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    preview.life_cycle().image == LifeCycleImage::Straight &&
                    preview.presented_image() == LifeCycleImage::Hidden &&
                    preview.lives() == 3 && preview.invulnerable() &&
                    preview.world().generated_rows() == 156 &&
                    preview.pose() == FlightPose::Straight && preview.animation_phase() == 84,
                "Cold PAL reset did not reveal at the observed 84-presentation-update boundary");
        advance_one_tick(preview);
        require(preview.life_cycle().phase == LifeCyclePhase::Active &&
                    preview.presented_image() == LifeCycleImage::Straight &&
                    preview.pose() == FlightPose::Right &&
                    preview.steering().horizontal_step == 16 &&
                    preview.steering().horizontal_fraction == 16 && !preview.invulnerable(),
                "Held right input resumed before the following pass");

        preview.start();
        preview.set_controls({});
        require(preview.status() == FlightStatus::Preparing &&
                    preview.presented_image() == LifeCycleImage::Hidden &&
                    preview.steering().horizontal_position == initial_steering.horizontal_position &&
                    preview.steering().horizontal_fraction == initial_steering.horizontal_fraction &&
                    preview.steering().horizontal_step == initial_steering.horizontal_step &&
                    preview.steering().scroll_speed == 0xFE &&
                    preview.pose() == FlightPose::Straight && preview.lives() == 4 &&
                    preview.world().generated_rows() == 0,
                "Restart did not reset preview state");
        test::wait_for_launch(preview);
        const auto waiting_rows = preview.world().generated_rows();
        require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    preview.status() == FlightStatus::Flying && preview.invulnerable() &&
                    waiting_rows == 156,
                "Neutral launch did not reach AwaitInput");
        for (int tick = 0; tick < 8; ++tick) {
            advance_one_tick(preview);
            require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                        preview.world().generated_rows() == waiting_rows && preview.invulnerable(),
                    "Neutral AwaitInput advanced before resume");
        }

        preview.start();
        require(preview.status() == FlightStatus::Preparing && preview.running() &&
                    preview.steering().horizontal_position == initial_steering.horizontal_position &&
                    preview.steering().horizontal_fraction == initial_steering.horizontal_fraction &&
                    preview.steering().horizontal_step == initial_steering.horizontal_step &&
                    preview.steering().scroll_speed == 0xFE && preview.lives() == 4 &&
                    preview.world().generated_rows() == 0,
                "Restart after round end failed");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
