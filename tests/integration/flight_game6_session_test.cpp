#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
#include "support/game5_session_inputs.hpp"
#include "support/regular_gap_observer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
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

[[nodiscard]] test::RegularGapSnapshot observation(const FlightPreview& flight) {
    return {flight.world().bridge_sprite_state(), flight.world().bridge_presentation_state(),
            flight.bridge_number(), flight.life_cycle().phase, flight.lives(),
            flight.status() == FlightStatus::Flying};
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"},
        StartOption{"3"}, StartOption{"4"}, StartOption{"5"}, StartOption{"6"},
        StartOption{"7"}, StartOption{"8"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    for (unsigned option = 0; option < 5; ++option)
        press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested &&
                flow.selected_option_index() == 5 && flight.selected_option() == 5 &&
                flight.two_players() && flight.status() == FlightStatus::Preparing &&
                flight.world().river_state().section == 20 && flight.bridge_number() == 20,
            "Normal F1/F3x5/F1 did not select two-player Game 6");
    for (unsigned update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 &&
                flight.world().generated_rows() == 156 && flight.score() == 0,
            "Game 6 launch did not reveal player one from section 20");

    const auto initial_checkpoint = checkpoint(flight);
    std::array<RiverCheckpoint, 2> checkpoints{initial_checkpoint, initial_checkpoint};
    std::array<std::uint32_t, 2> scores{0, 0};
    std::array<std::uint16_t, 2> bridges{20, 20};
    test::RegularGapObserver observer;
    require(!observer.armed() && !observer.confirmed() &&
                !flight.world().bridge_sprite_state().automatic_entry_enabled,
            "Game 6 launch was already at the regular-gap goal");

    // Port two is deliberately held right/fire. Only the current player's
    // port drives the checked-in ordinary first-life route.
    JoystickControls ports{};
    ports[1] = right_fire;
    launch.set_controls({});
    std::size_t slices = 0, updates = 0, bridge_changes = 0;
    std::size_t arms = 0, entries = 0, publications = 0, confirmations = 0;
    std::size_t publication_update = 0;
    for (const auto run : test::game5_regular_gap_route) {
        require(run.direction >= 0 && run.direction <= 2 && run.slices > 0,
                "Game 6 fixture contains a nonordinary route input");
        ports[0] = {false, false, run.direction == 2, run.direction == 1, run.fire};
        launch.set_joystick_controls(ports);
        for (int slice = 0; slice < run.slices; ++slice) {
            for (int half = 0; half < 2; ++half) {
                const auto before = observation(flight);
                const bool was_armed = observer.armed();
                const bool was_pending = observer.awaiting_survival();
                const bool was_confirmed = observer.confirmed();
                const auto admitted = launch.advance_elapsed(10ms);
                (void)flight.take_audio();
                require(admitted <= 1, "Game 6 route half admitted multiple updates");
                updates += admitted;
                const auto after = observation(flight);
                (void)observer.observe(before, after, admitted);
                require(flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                            flight.life_cycle().active_player == 0 &&
                            (updates == 0 || flight.life_cycle().phase == LifeCyclePhase::Active) &&
                            flight.player_score(1) == 0,
                        "Game 6 route left player one's first life or touched player two");
                if (updates == 1)
                    require(flight.pose() == FlightPose::Straight &&
                                flight.shot().start_event.has_value(),
                            "Inactive port two steered player one's first route update");
                if (before.bridge != after.bridge) {
                    ++bridge_changes;
                    require(!was_armed && !observer.armed() &&
                                before.bridge == 20 && after.bridge == 21,
                            "Game 6 bridge progression corrupted regular origin");
                }
                if (!was_armed && observer.armed()) {
                    ++arms;
                    require(admitted == 1 && !before.mechanics.crossing &&
                                before.bridge == after.bridge && after.bridge == 21 &&
                                flight.world().generated_rows() == 1792 &&
                                after.mechanics.automatic_entry_enabled &&
                                after.mechanics.gap_animation_counter == 0xff,
                            "Game 6 regular crossing origin changed");
                }
                if (was_armed && observer.armed() && admitted == 1 &&
                    before.mechanics.gap_animation_counter == 0xff &&
                    after.mechanics.gap_animation_counter <= 1) ++entries;
                if (!was_pending && observer.awaiting_survival()) {
                    ++publications;
                    publication_update = updates;
                    require(admitted == 1 && !observer.confirmed(),
                            "Game 6 credited entry without a following update");
                }
                if (!was_confirmed && observer.confirmed()) {
                    ++confirmations;
                    require(was_pending && admitted == 1 &&
                                updates == publication_update + 1,
                            "Game 6 regular gap lacked a separate surviving update");
                }
            }
            ++slices;
        }
    }
    const auto& witness = observer.witness();
    require(slices == test::game5_regular_gap_route_slices && updates == 1674 &&
                bridge_changes == 1 && arms == 1 && entries == 1 &&
                publications == 1 && confirmations == 1 && observer.confirmed() &&
                witness && witness->bridge == 21 && witness->flying &&
                witness->life_phase == LifeCyclePhase::Active && witness->lives == 3 &&
                witness->mechanics.automatic_entry_enabled &&
                witness->published.automatic_entry_enabled &&
                witness->mechanics.gap_animation_counter == 1 &&
                witness->published.gap_animation_counter == 1 &&
                flight.world().generated_rows() == 1822 &&
                flight.steering().horizontal_position == 89 &&
                flight.bridge_number() == 21 && flight.score() == 2650 &&
                flight.fuel() == 32703 && !flight.life_cycle().cause,
            "Game 6 route failed its first-life regular-origin automatic-gap witness");
    auto neutral_probe = flight;
    neutral_probe.set_controls({});
    neutral_probe.set_joystick_controls({});
    std::size_t probe_updates = 0;
    for (int half = 0; half < 2; ++half) {
        const auto admitted = neutral_probe.advance_elapsed(10ms);
        (void)neutral_probe.take_audio();
        require(admitted <= 1 && neutral_probe.status() == FlightStatus::Flying &&
                    neutral_probe.lives() == 3 && !neutral_probe.life_cycle().cause,
                "Game 6 regular entry queued a fatal contact");
        probe_updates += admitted;
    }
    require(probe_updates >= 1, "Game 6 neutral clone omitted the queued-death guard");
    scores[0] = flight.score();
    bridges[0] = flight.bridge_number();
    checkpoints[0] = checkpoint(flight);
    require(checkpoints[0] != checkpoints[1] && scores[0] > scores[1] &&
                bridges[0] > bridges[1],
            "Game 6 first-life regular route did not establish distinct player progress");

    for (int turn = 0; turn < 8; ++turn) {
        const auto player = static_cast<std::size_t>(turn & 1);
        const auto other = 1U - player;
        require(flight.life_cycle().active_player == player &&
                    flight.lives() == 3 - turn / 2 &&
                    flight.life_cycle().inactive_reserve_aircraft == 4 - (turn + 1) / 2 &&
                    flight.score() == scores[player] &&
                    flight.bridge_number() == bridges[player] &&
                    flight.player_score(other) == scores[other],
                "Game 6 alternating turn merged player progress");
        if (turn != 0) {
            // The outgoing port cannot resume the next player's input gate.
            ports = {};
            ports[other] = right_fire;
            launch.set_joystick_controls(ports);
            const auto waiting_rows = flight.world().generated_rows();
            tick(launch, flight);
            require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                        flight.world().generated_rows() == waiting_rows &&
                        flight.shot().pose == PlayerShotPose::Hidden,
                    "Inactive joystick port resumed Game 6's next aircraft");
            ports[player] = bank_right;
            launch.set_joystick_controls(ports);
            tick(launch, flight);
            require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                        flight.life_cycle().active_player == player &&
                        flight.pose() == FlightPose::Right &&
                        !flight.shot().start_event &&
                        flight.shot().pose == PlayerShotPose::Hidden,
                    "Current joystick port failed to resume or leaked other-port fire");
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
                "Game 6 turn did not end by a natural collision with separate scores");
        checkpoints[player] = flight.world().selected_checkpoint();
        require(checkpoints[0] != checkpoints[1] && scores[0] > scores[1] &&
                    bridges[0] > bridges[1],
                "Game 6 collision lost distinct score, bridge, or checkpoint progress");

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
                "Game 6 handoff restored stale contacts or the wrong player's checkpoint");
        for (unsigned update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying &&
                    flight.world().generated_rows() == 156 &&
                    checkpoint(flight) == checkpoints[next] &&
                    flight.player_score(0) == scores[0] &&
                    flight.player_score(1) == scores[1],
                "Game 6 rebuild changed distinct player progress");
    }
    const auto record = std::max(scores[0], scores[1]);
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.life_cycle().active_player == 0 &&
                flight.lives() == 0 && flight.life_cycle().inactive_reserve_aircraft == 0 &&
                scores[0] == 2650 && scores[1] == 0 &&
                bridges[0] == 21 && bridges[1] == 20 && flight.high_score() == record &&
                flight.player_score(0) == scores[0] && flight.player_score(1) == scores[1],
            "Game 6 terminal lifecycle lost separate scores or the high score");
    press(launch, StartButton::Proceed);
    require(flight.status() == FlightStatus::Preparing && flight.selected_option() == 5 &&
                flight.two_players() && flight.bridge_number() == 20 &&
                flight.world().river_state().section == 20 &&
                flight.player_score(0) == 0 && flight.player_score(1) == 0 &&
                flight.high_score() == record,
            "Game 6 F1 restart lost its preset or retained per-player scores");
    for (unsigned update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4 &&
                flight.bridge_number() == 20 && flight.world().generated_rows() == 156 &&
                flight.high_score() == record,
            "Game 6 restart did not rebuild its normal first-player launch gate");
    std::cout << "game6_regular_gap native_coverage_only=true route_slices=" << slices
              << " route_updates=" << updates << " bridge=" << bridges[0]
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
