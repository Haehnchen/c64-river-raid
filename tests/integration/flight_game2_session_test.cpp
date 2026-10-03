#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
#include "support/long_session_inputs.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using river_raid::test::press;
using river_raid::test::tick;
using namespace std::chrono_literals;

constexpr FlightControls bank_right{false, false, false, true, false};
constexpr FlightControls right_fire{false, false, false, true, true};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] RiverCheckpoint checkpoint(const FlightPreview& flight) {
    return {flight.world().river_state().section, flight.world().river_state().section_anchor};
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested &&
                flow.selected_option_index() == 1 && flight.selected_option() == 1 &&
                flight.two_players() && flight.status() == FlightStatus::Preparing &&
                flight.world().river_state().section == 1 && flight.bridge_number() == 1,
            "Normal F1/F3/F1 did not select two-player Game 2");
    for (unsigned update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 &&
                flight.world().generated_rows() == 156 && flight.score() == 0,
            "Game 2 launch did not reveal player one from section one");

    const auto initial_checkpoint = checkpoint(flight);
    std::array<RiverCheckpoint, 2> checkpoints{initial_checkpoint, initial_checkpoint};
    std::array<std::uint32_t, 2> scores{0, 0};
    std::array<std::uint16_t, 2> bridges{1, 1};
    JoystickControls ports{};
    ports[1] = right_fire;
    launch.set_controls({});
    std::size_t slices = 0, updates = 0, bridge_events = 0;
    for (const auto run : test::first_bridge_session_prefix) {
        require(run.direction >= 0 && run.direction <= 2 && run.slices > 0,
                "Game 2 fixture contains an invalid ordinary input");
        ports[0] = {false, false, run.direction == 2, run.direction == 1, run.fire};
        launch.set_joystick_controls(ports);
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto before_bridge = flight.bridge_number();
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            require(admitted <= 2, "Game 2 route slice admitted too many updates");
            ++slices;
            updates += admitted;
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                        flight.life_cycle().phase == LifeCyclePhase::Active &&
                        flight.life_cycle().active_player == 0 &&
                        flight.player_score(1) == 0,
                    "Game 2 first-player route lost a life or touched player two");
            if (updates == 1)
                require(flight.pose() == FlightPose::Straight &&
                            flight.shot().start_event.has_value(),
                        "Inactive port two steered player one's first update");
            if (flight.bridge_number() != before_bridge) {
                ++bridge_events;
                require(slices == 883 && before_bridge == 1 && flight.bridge_number() == 2,
                        "Game 2 ordinary route changed its first bridge progression");
                break;
            }
        }
        if (bridge_events != 0) break;
    }
    require(slices == 883 && updates == 885 && bridge_events == 1 &&
                flight.world().generated_rows() == 1033 && flight.bridge_number() == 2 &&
                flight.score() == 1000 && flight.fuel() == 39455 && flight.lives() == 3,
            "Game 2 first-life route diverged from the Game 1 ordinary-input endpoint");
    std::size_t second_slices = 0, second_updates = 0;
    for (const auto run : test::second_bridge_session_suffix) {
        require(run.direction >= 0 && run.direction <= 2 && run.slices > 0,
                "Game 2 second-bridge fixture contains an invalid ordinary input");
        ports[0] = {false, false, run.direction == 2, run.direction == 1, run.fire};
        launch.set_joystick_controls(ports);
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            require(admitted <= 2, "Game 2 second-bridge slice admitted too many updates");
            ++second_slices;
            second_updates += admitted;
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                        flight.life_cycle().phase == LifeCyclePhase::Active &&
                        flight.life_cycle().active_player == 0 &&
                        flight.player_score(1) == 0,
                    "Game 2 second-bridge route lost its first life or touched player two");
        }
    }
    require(second_slices == 1129 && second_updates == 1132 &&
                flight.world().generated_rows() == 2165 && flight.bridge_number() == 3 &&
                flight.score() == 2190 && flight.fuel() == 3231 &&
                flight.world().river_state().section == 3,
            "Game 2 second-bridge route diverged from the Game 1 ordinary-input endpoint");
    ports = {};
    launch.set_joystick_controls(ports);
    tick(launch, flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.lives() == 3 && !flight.life_cycle().cause,
            "Game 2 bridge clear queued a fatal contact");
    scores[0] = flight.score();
    bridges[0] = flight.bridge_number();
    checkpoints[0] = checkpoint(flight);
    require(checkpoints[0] != checkpoints[1] && scores[0] > scores[1] &&
                bridges[0] > bridges[1],
            "Game 2 route did not establish separate player progress");

    for (int turn = 0; turn < 8; ++turn) {
        const auto player = static_cast<std::size_t>(turn & 1);
        const auto other = 1U - player;
        require(flight.life_cycle().active_player == player &&
                    flight.lives() == 3 - turn / 2 &&
                    flight.life_cycle().inactive_reserve_aircraft == 4 - (turn + 1) / 2 &&
                    flight.score() == scores[player] &&
                    flight.bridge_number() == bridges[player] &&
                    flight.player_score(other) == scores[other],
                "Game 2 alternating turn merged player progress");
        if (turn != 0) {
            ports = {};
            ports[other] = right_fire;
            launch.set_joystick_controls(ports);
            const auto waiting_rows = flight.world().generated_rows();
            tick(launch, flight);
            require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                        flight.world().generated_rows() == waiting_rows &&
                        flight.shot().pose == PlayerShotPose::Hidden,
                    "Inactive port resumed Game 2's next aircraft");
            ports[player] = bank_right;
            launch.set_joystick_controls(ports);
            tick(launch, flight);
            require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                        flight.life_cycle().active_player == player &&
                        flight.pose() == FlightPose::Right &&
                        !flight.shot().start_event &&
                        flight.shot().pose == PlayerShotPose::Hidden,
                    "Current port failed to resume or leaked inactive-port fire");
        } else {
            ports[0] = bank_right;
            launch.set_joystick_controls(ports);
        }
        for (unsigned update = 0; update < 2000 &&
                                  flight.status() == FlightStatus::Flying; ++update)
            tick(launch, flight);
        require(flight.status() == FlightStatus::Crashed &&
                    flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                    flight.life_cycle().cause == LifeEndCause::Collision &&
                    flight.score() == scores[player] &&
                    flight.bridge_number() == bridges[player] &&
                    flight.player_score(other) == scores[other],
                "Game 2 turn did not end by natural collision with separate progress");
        checkpoints[player] = flight.world().selected_checkpoint();
        require(checkpoints[0] != checkpoints[1],
                "Game 2 collision lost separate checkpoint progress");

        ports = {};
        launch.set_joystick_controls(ports);
        for (unsigned update = 0; update < 127; ++update) tick(launch, flight);
        if (turn == 7) break;
        const auto next = 1U - player;
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.life_cycle().active_player == next &&
                    flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                    checkpoint(flight) == checkpoints[next] &&
                    flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                    flight.shot().pose == PlayerShotPose::Hidden,
                "Game 2 handoff restored stale contacts or wrong checkpoint");
        for (unsigned update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying &&
                    flight.world().generated_rows() == 156 &&
                    checkpoint(flight) == checkpoints[next] &&
                    flight.player_score(0) == scores[0] &&
                    flight.player_score(1) == scores[1],
                "Game 2 rebuild changed distinct player progress");
    }
    const auto record = std::max(scores[0], scores[1]);
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.life_cycle().active_player == 0 && flight.lives() == 0 &&
                flight.life_cycle().inactive_reserve_aircraft == 0 &&
                scores[0] == 2190 && scores[1] == 0 && bridges[0] == 3 &&
                bridges[1] == 1 && flight.high_score() == record &&
                flight.player_score(0) == scores[0] &&
                flight.player_score(1) == scores[1],
            "Game 2 terminal state lost separate progress or high score");
    press(launch, StartButton::Proceed);
    require(flight.status() == FlightStatus::Preparing && flight.selected_option() == 1 &&
                flight.two_players() && flight.bridge_number() == 1 &&
                flight.world().river_state().section == 1 &&
                flight.player_score(0) == 0 && flight.player_score(1) == 0 &&
                flight.high_score() == record,
            "Game 2 F1 restart lost its preset or retained per-player scores");
    for (unsigned update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 &&
                flight.bridge_number() == 1 && flight.world().generated_rows() == 156 &&
                flight.high_score() == record,
            "Game 2 restart did not rebuild its normal player-one input gate");
    std::cout << "game2_two_bridges native_coverage_only=true route_slices="
              << slices + second_slices << " route_updates=" << updates + second_updates
              << " bridge=" << bridges[0]
              << " score=" << scores[0] << " turns=8 terminal=true\n";
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
