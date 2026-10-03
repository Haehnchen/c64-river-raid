#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"
#include "game/shot_contact.hpp"
#include "support/long_session_inputs.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_expired_shot_retains_bridge_contact() {
    FlightPreview flight;
    constexpr std::array options{StartOption{"1"}};
    StartFlow flow(options);
    FlightStart launch(flow, flight);
    for (unsigned press = 0; press < 2; ++press) {
        launch.set_button(StartButton::Proceed, true);
        launch.set_button(StartButton::Proceed, false);
    }
    require(flight.status() == FlightStatus::Preparing,
            "Normal F1 start did not begin the launch rebuild");
    test::wait_for_launch(flight);
    (void)flight.take_audio();
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput && flight.lives() == 3,
            "Normal launch did not reach its input gate");

    std::size_t slice = 0;
    for (const auto& run : test::first_bridge_session_prefix) {
        for (int held = 0; held < run.slices; ++held, ++slice) {
            // Ordinary down input for 19 elapsed slices moves the meeting point
            // to the shot's expiry boundary. No flight/contact state is injected.
            const bool slow = slice >= 850 && slice < 869;
            launch.set_controls({false, slow, run.direction == 2, run.direction == 1, run.fire});
            const auto shot_before = flight.shot().state;
            const auto objects_before = flight.world().object_state();
            const auto score_before = flight.score();
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            require(admitted <= 2 && flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active && flight.lives() == 3,
                    "Expiry route left its normal first-life flight");
            if (flight.bridge_number() == 1) continue;

            require(slice == 884 && admitted == 1 && shot_before.vertical_position == 52,
                    "Bridge contact no longer coincides with a single expiry update");
            const auto expired = update_player_shot(shot_before, run.fire);
            require(expired.state.vertical_position == 0 && expired.pose == PlayerShotPose::Hidden &&
                        expired.vertical_position_write == 48 && !expired.start_event,
                    "Shot did not expire before retained-contact resolution");
            const auto& consumed = flight.last_consumed_contacts();
            ShotContactSnapshot snapshot{};
            snapshot.slots = consumed.shot;
            for (std::size_t index = 0; index < snapshot.object_kinds.size(); ++index)
                snapshot.object_kinds[index] = objects_before.records[index].animated_kind;
            const auto selected = select_shot_contact(snapshot);
            require(consumed.shot_presented && selected.outcome == ShotContactOutcome::Bridge &&
                        selected.object && selected.slot && selected.clear_projectile &&
                        snapshot.object_kinds[selected.object->index.value] == 14,
                    "Expiry pass lost the previously presented shot's live bridge contact");
            require(score_before == 500 && flight.score() == 1000 && flight.bridge_number() == 2 &&
                        flight.world().generated_rows() == 1029 && flight.bridge_flash() &&
                        flight.audio_state().explosion_countdown == 31 &&
                        flight.shot().state.vertical_position == 0 &&
                        flight.shot().pose == PlayerShotPose::Hidden &&
                        flight.shot().vertical_position_write == 0,
                    "Expired projectile did not complete the ordinary bridge transaction");

            launch.set_controls({});
            bool advanced = false;
            for (unsigned part = 0; part < 4 && !advanced; ++part) {
                const auto count = launch.advance_elapsed(10ms);
                require(count <= 1, "Post-bridge check admitted multiple updates");
                advanced = count == 1;
                (void)flight.take_audio();
            }
            require(advanced && flight.score() == 1000 && flight.bridge_number() == 2 &&
                        flight.lives() == 3 && flight.status() == FlightStatus::Flying,
                    "Retained bridge contact awarded twice or ended the first life");
            return;
        }
    }
    throw std::runtime_error("Ordinary-input route did not reach the expiry bridge witness");
}
} // namespace

int main() {
    try {
        test_expired_shot_retains_bridge_contact();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
