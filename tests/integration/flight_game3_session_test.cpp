#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
#include "game/fuel.hpp"
#include "support/game3_session_inputs.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using river_raid::test::press;
using river_raid::test::tick;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void collide(FlightStart& launch, FlightPreview& flight) {
    launch.set_controls({false, false, false, true, false});
    for (int update = 0; update < 2000; ++update) {
        tick(launch, flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.life_cycle().cause == LifeEndCause::Collision &&
                        flight.score() == 1510 && flight.bridge_number() == 6,
                    "Natural collision lost Game 3 progress");
            return;
        }
        require(flight.status() == FlightStatus::Flying, "Unexpected collision route state");
    }
    throw std::runtime_error("Held-right input did not reach collision");
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    press(launch, StartButton::NextOption);
    press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flight.selected_option() == 2 && !flight.two_players() &&
                flight.bridge_number() == 5 && flight.world().river_state().section == 5,
            "Normal menu did not select Game 3");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156 && flight.lives() == 3,
            "Game 3 launch gate changed");

    std::size_t slices = 0, updates = 0, bridge_hits = 0;
    for (const auto run : test::game3_bridge_route) {
        launch.set_controls({false, false, run.direction == 2, run.direction == 1, run.fire});
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto bridge_before = flight.bridge_number();
            const auto score_before = flight.score();
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            require(admitted <= 2, "Route slice admitted too many updates");
            ++slices;
            updates += admitted;
            require(flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active && flight.lives() == 3,
                    "Fixed Game 3 route lost its first life");
            if (flight.bridge_number() != bridge_before) {
                ++bridge_hits;
                bool joint_shot_contact = false;
                const auto& contacts = flight.last_consumed_contacts();
                for (const auto& slot : contacts.shot) {
                    joint_shot_contact |= slot.object_contact &&
                        slot.bridge_sprite_present &&
                        (slot.primary_present || slot.secondary_present);
                }
                // Kind 16 is the joint projectile/bridge-sprite award; a
                // player-triggered bridge event writes zero at this boundary.
                require(slices == 904 && bridge_before == 5 && flight.bridge_number() == 6 &&
                            contacts.shot_presented && joint_shot_contact &&
                            flight.score() - score_before == 750 && flight.bridge_flash() &&
                            flight.shot().pose == PlayerShotPose::Hidden &&
                            flight.shot().state.vertical_position == 0 &&
                            flight.audio_state().explosion_countdown == 31,
                        "Game 3 ordinary bridge transaction changed");
            }
        }
    }
    require(slices == 904 && updates == 906 && bridge_hits == 1 &&
                flight.world().generated_rows() == 1054 && flight.fuel() == 42687 &&
                flight.score() == 1510 && flight.bridge_number() == 6,
            "Full fixed Game 3 route endpoint changed");
    launch.set_controls({});
    tick(launch, flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                !flight.life_cycle().cause && flight.lives() == 3 && flight.score() == 1510,
            "Bridge hit queued a fatal contact or repeated its award");

    for (int reserves = 3; reserves > 0; --reserves) {
        collide(launch, flight);
        const auto checkpoint = flight.world().selected_checkpoint();
        launch.set_controls({});
        for (int update = 0; update < 127; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.fuel() == kInitialFuel &&
                    flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                    flight.shot().pose == PlayerShotPose::Hidden &&
                    RiverCheckpoint{flight.world().river_state().section,
                                    flight.world().river_state().section_anchor} == checkpoint,
                "Checkpoint restore retained stale state or restored the wrong anchor");
        for (int update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying && flight.lives() == reserves - 1 &&
                    flight.bridge_number() == 6 && flight.score() == 1510 &&
                    flight.world().generated_rows() == 156 &&
                    RiverCheckpoint{flight.world().river_state().section,
                                    flight.world().river_state().section_anchor} == checkpoint,
                "Rebuilt Game 3 life lost checkpoint, score or reserves");
    }
    collide(launch, flight);
    launch.set_controls({});
    for (int update = 0; update < 127; ++update) tick(launch, flight);
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.lives() == 0 && flight.high_score() == 1510,
            "Game 3 did not reach terminal state with retained high score");
    press(launch, StartButton::Proceed);
    require(flight.status() == FlightStatus::Preparing && flight.selected_option() == 2 &&
                flight.bridge_number() == 5 && flight.score() == 0 &&
                flight.world().generated_rows() == 0 && flight.high_score() == 1510,
            "F1 restart lost Game 3 preset or high score");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput && flight.lives() == 3 &&
                flight.bridge_number() == 5 && flight.score() == 0 &&
                flight.world().generated_rows() == 156 && flight.high_score() == 1510,
            "Restart did not rebuild Game 3 initial checkpoint");
}
} // namespace

int main() {
    try {
        run_session();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
