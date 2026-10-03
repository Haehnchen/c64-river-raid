#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
#include "game/fuel.hpp"
#include "support/game7_session_inputs.hpp"

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

void collide(FlightStart& launch, FlightPreview& flight, int score) {
    launch.set_controls({false, false, false, true, false});
    for (int update = 0; update < 2000; ++update) {
        tick(launch, flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.life_cycle().cause == LifeEndCause::Collision &&
                        flight.score() == score && flight.bridge_number() == 51,
                    "Natural collision lost Game 7 progress");
            return;
        }
        require(flight.status() == FlightStatus::Flying,
                "Unexpected Game 7 collision-route state");
    }
    throw std::runtime_error("Held-right input did not reach a natural collision");
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
        StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    for (int option = 0; option < 6; ++option) press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flight.selected_option() == 6 && !flight.two_players() &&
                flight.bridge_number() == 50 && flight.world().river_state().section == 50,
            "Normal menu did not select one-player Game 7");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156 && flight.lives() == 3,
            "Game 7 launch gate changed");

    int slices = 0, updates = 0, bridge_events = 0;
    int bridge_event_slice = -1, bridge_score_delta = -1;
    bool joint_shot_contact = false;
    for (const auto run : test::game7_first_bridge_route) {
        launch.set_controls({run.vertical == test::Game7Vertical::Up,
            run.vertical == test::Game7Vertical::Down,
            run.direction == 2, run.direction == 1, run.fire});
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto previous_bridge = flight.bridge_number();
            const auto previous_score = flight.score();
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            require(admitted <= 2, "Game 7 route slice admitted too many updates");
            ++slices;
            updates += static_cast<int>(admitted);
            require(flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active &&
                        flight.lives() == 3,
                    "Fixed Game 7 route lost its first life");
            if (flight.bridge_number() != previous_bridge) {
                ++bridge_events;
                bridge_event_slice = slices;
                bridge_score_delta = flight.score() - previous_score;
                const auto& contacts = flight.last_consumed_contacts();
                for (const auto& slot : contacts.shot)
                    joint_shot_contact |= contacts.shot_presented && slot.object_contact &&
                        slot.bridge_sprite_present &&
                        (slot.primary_present || slot.secondary_present);
                require(slices == 531 && previous_bridge == 50 &&
                            flight.bridge_number() == 51 && bridge_score_delta == 750 &&
                            joint_shot_contact && flight.bridge_flash() &&
                            flight.shot().pose == PlayerShotPose::Hidden &&
                            flight.shot().state.vertical_position == 0 &&
                            flight.audio_state().explosion_countdown == 31,
                        "Game 7 route did not clear bridge 50 through ordinary contact");
            }
        }
    }
    require(slices == 531 && updates == 532 && bridge_events == 1 &&
                flight.world().generated_rows() == 1141 && flight.fuel() == 48511 &&
                flight.score() == 1300 && flight.bridge_number() == 51 &&
                flight.lives() == 3,
            "Fixed Game 7 first-bridge endpoint changed");
    require(bridge_event_slice == 531 && bridge_score_delta == 750 && joint_shot_contact,
            "Game 7 bridge award was not the observed joint projectile subtype");
    launch.set_controls({});
    tick(launch, flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                !flight.life_cycle().cause && flight.lives() == 3 &&
                flight.bridge_number() == 51 && flight.score() == 1300,
            "Game 7 bridge clear queued a fatal contact or repeated its award");

    for (int reserves = 3; reserves > 0; --reserves) {
        collide(launch, flight, 1300);
        const auto checkpoint = flight.world().selected_checkpoint();
        launch.set_controls({});
        for (int update = 0; update < 127; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.fuel() == kInitialFuel &&
                    flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                    flight.shot().pose == PlayerShotPose::Hidden &&
                    RiverCheckpoint{flight.world().river_state().section,
                                    flight.world().river_state().section_anchor} == checkpoint,
                "Game 7 checkpoint restore retained stale state or wrong anchor");
        for (int update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying && flight.lives() == reserves - 1 &&
                    flight.bridge_number() == 51 && flight.score() == 1300 &&
                    flight.world().generated_rows() == 156 &&
                    RiverCheckpoint{flight.world().river_state().section,
                                    flight.world().river_state().section_anchor} == checkpoint,
                "Rebuilt Game 7 life lost checkpoint, score or reserves");
    }
    collide(launch, flight, 1300);
    launch.set_controls({});
    for (int update = 0; update < 127; ++update) tick(launch, flight);
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.lives() == 0 && flight.high_score() == 1300,
            "Game 7 did not reach terminal state with retained high score");
    press(launch, StartButton::Proceed);
    require(flight.status() == FlightStatus::Preparing && flight.selected_option() == 6 &&
                flight.bridge_number() == 50 && flight.score() == 0 &&
                flight.world().generated_rows() == 0 && flight.high_score() == 1300,
            "F1 restart lost Game 7 preset or high score");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput && flight.lives() == 3 &&
                flight.bridge_number() == 50 && flight.score() == 0 &&
                flight.world().generated_rows() == 156 && flight.high_score() == 1300,
            "Restart did not rebuild Game 7 initial checkpoint");
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
