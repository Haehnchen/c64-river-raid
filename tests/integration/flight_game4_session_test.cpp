#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
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

RiverCheckpoint current_checkpoint(const FlightPreview& flight) {
    return {flight.world().river_state().section, flight.world().river_state().section_anchor};
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"},
                                StartOption{"3"}, StartOption{"4"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    for (int option = 0; option < 3; ++option) press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flight.selected_option() == 3 && flight.two_players(), "Game 4 selection failed");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 && flight.bridge_number() == 5,
            "Game 4 initial hidden handoff did not select player one");

    std::array checkpoints{current_checkpoint(flight), current_checkpoint(flight)};
    std::array<std::uint32_t, 2> scores{0, 0};
    std::array<std::uint16_t, 2> bridges{5, 5};
    std::size_t route_slices = 0, route_updates = 0;
    for (const auto run : test::game3_bridge_route) {
        launch.set_controls({false, false, run.direction == 2, run.direction == 1, run.fire});
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto admitted = launch.advance_elapsed(20ms);
            (void)flight.take_audio();
            ++route_slices;
            route_updates += admitted;
            require(admitted <= 2 && flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active && flight.lives() == 3 &&
                        flight.life_cycle().active_player == 0 && flight.player_score(1) == 0,
                    "Player-one route changed the other player or lost a life");
        }
    }
    require(route_slices == 904 && route_updates == 906 && flight.score() == 1510 &&
                flight.bridge_number() == 6 && flight.world().generated_rows() == 1054,
            "Game 4 first-player bridge route changed");
    scores[0] = 1510;
    bridges[0] = 6;
    launch.set_controls({});
    tick(launch, flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active && flight.lives() == 3,
            "Game 4 bridge clear queued a fatal contact");

    for (int turn = 0; turn < 8; ++turn) {
        const auto player = static_cast<std::size_t>(turn & 1);
        require(flight.life_cycle().active_player == player && flight.lives() == 3 - turn / 2 &&
                    flight.life_cycle().inactive_reserve_aircraft == 4 - (turn + 1) / 2 &&
                    flight.score() == scores[player] && flight.bridge_number() == bridges[player],
                "Alternating turn lost its player's distinct progress");
        launch.set_controls({false, false, false, true, false});
        for (int update = 0; update < 2000 && flight.status() == FlightStatus::Flying; ++update)
            tick(launch, flight);
        require(flight.status() == FlightStatus::Crashed &&
                    flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                    flight.life_cycle().cause == LifeEndCause::Collision &&
                    flight.score() == scores[player] && flight.bridge_number() == bridges[player] &&
                    flight.player_score(0) == scores[0] && flight.player_score(1) == scores[1],
                "Natural alternating-player collision changed progress");
        checkpoints[player] = flight.world().selected_checkpoint();
        require(checkpoints[0] != checkpoints[1],
                "Two-player test did not reach distinct checkpoint anchors");
        launch.set_controls({});
        for (int update = 0; update < 127; ++update) tick(launch, flight);
        if (turn == 7) break;
        const auto next = 1U - player;
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.life_cycle().active_player == next &&
                    current_checkpoint(flight) == checkpoints[next] &&
                    flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                    flight.shot().pose == PlayerShotPose::Hidden,
                "Player handoff restored the wrong checkpoint or stale contact state");
        for (int update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying &&
                    current_checkpoint(flight) == checkpoints[next] &&
                    flight.world().generated_rows() == 156 &&
                    flight.player_score(0) == scores[0] && flight.player_score(1) == scores[1],
                "Alternating rebuild changed checkpoints or merged scores");
    }
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.life_cycle().active_player == 0 && flight.lives() == 0 &&
                flight.life_cycle().inactive_reserve_aircraft == 0 && flight.high_score() == 1510 &&
                flight.player_score(0) == 1510 && flight.player_score(1) == 0,
            "Game 4 terminal state lost distinct scores or high score");
    press(launch, StartButton::Proceed);
    require(flight.selected_option() == 3 && flight.two_players() &&
                flight.status() == FlightStatus::Preparing && flight.bridge_number() == 5 &&
                flight.player_score(0) == 0 && flight.player_score(1) == 0 &&
                flight.high_score() == 1510,
            "Game 4 F1 restart lost preset, score reset or shared record");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 &&
                flight.bridge_number() == 5 && flight.world().generated_rows() == 156,
            "Game 4 restart did not reset both player allocations");
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
