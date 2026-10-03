#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"
#include "game/fuel.hpp"
#include "game/life_cycle.hpp"
#include "support/long_session_inputs.hpp"
#include "support/session_scene_capture.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

constexpr FlightControls neutral{};
constexpr FlightControls bank_right{false, false, false, true, false};

void advance_one_tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto ticks = flight.advance_elapsed(10ms);
        require(ticks <= 1, "Ten-millisecond slice admitted multiple foreground ticks");
        (void)flight.take_audio();
        if (ticks == 1) return;
    }
    throw std::runtime_error("Bounded PAL interval did not admit one foreground tick");
}

void start_from_title(FlightStart& launch) {
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
}

struct ReplayEvidence {
    std::uint64_t elapsed_slices{};
    std::uint64_t admitted_ticks{};
    std::uint64_t fuel_increase_slices{};
};

struct SceneLandmarks {
    test::SessionSceneCapture& captures;
    bool first_refuel{};
    bool ordinary_bridge{};
    bool ordinary_next_slice{};
    bool joint_bridge{};
    bool joint_next_slice{};

    void after_slice(const FlightPreview& flight, std::uint16_t prior_fuel,
                     std::uint16_t prior_bridge) {
        if (!captures.enabled()) return;
        if (ordinary_bridge && !ordinary_next_slice) {
            captures.record("ordinary_bridge_next_slice", flight);
            ordinary_next_slice = true;
        }
        if (joint_bridge && !joint_next_slice) {
            captures.record("joint_bridge_next_slice", flight);
            joint_next_slice = true;
        }
        if (!first_refuel && flight.fuel() > prior_fuel) {
            captures.record("first_refuel", flight);
            first_refuel = true;
        }
        if (!ordinary_bridge && prior_bridge == 1 && flight.bridge_number() == 2) {
            captures.record("ordinary_bridge_hit", flight);
            ordinary_bridge = true;
        }
        if (!joint_bridge && prior_bridge == 2 && flight.bridge_number() == 3) {
            captures.record("joint_bridge_hit", flight);
            joint_bridge = true;
        }
    }
};

FlightControls controls_for(const test::SessionInputRun& run) {
    return {false, false, run.direction == 2, run.direction == 1, run.fire};
}

template <typename Runs>
ReplayEvidence replay_input_runs(FlightStart& launch, FlightPreview& flight,
                                const Runs& runs, bool finish_in_fuel_exhaustion = false,
                                std::uint16_t stop_after_bridge = 0,
                                SceneLandmarks* scenes = nullptr) {
    ReplayEvidence evidence{};
    for (std::size_t run_index = 0; run_index < std::size(runs); ++run_index) {
        const auto& run = runs[run_index];
        require(run.direction >= 0 && run.direction <= 2 && run.slices > 0,
                "Long-session input contains an invalid direction or duration");
        launch.set_controls(controls_for(run));
        for (int slice = 0; slice < run.slices; ++slice) {
            const bool last_slice = run_index + 1 == std::size(runs) &&
                                    slice + 1 == run.slices;
            const auto fuel_before = flight.fuel();
            const auto bridge_before = flight.bridge_number();
            const auto admitted = launch.advance_elapsed(20ms);
            require(admitted <= 2,
                    "Twenty-millisecond route slice admitted more than two updates");
            evidence.admitted_ticks += admitted;
            ++evidence.elapsed_slices;

            const auto fuel_after = flight.fuel();
            if (fuel_after > fuel_before) {
                const auto effect = flight.audio_state().voice1_effect;
                require(effect == Voice1AudioEffect::RefuelFuelAdded ||
                            effect == Voice1AudioEffect::RefuelTankSaturated,
                        "Fuel rose on the connected route without a refuel audio effect");
                ++evidence.fuel_increase_slices;
            }
            (void)flight.take_audio();

            if (finish_in_fuel_exhaustion && last_slice) {
                require(flight.status() == FlightStatus::Crashed &&
                            flight.life_cycle().phase == LifeCyclePhase::Exploding,
                        "Fuel suffix did not finish at its natural life-end boundary");
            } else {
                require(flight.status() == FlightStatus::Flying,
                        "Long-session replay left active flight before its expected endpoint");
            }
            if (scenes) scenes->after_slice(flight, fuel_before, bridge_before);
            if (stop_after_bridge != 0 && flight.bridge_number() == stop_after_bridge) {
                return evidence;
            }
        }
    }
    require(stop_after_bridge == 0 || flight.bridge_number() == stop_after_bridge,
            "Input prefix ended before the requested bridge endpoint");
    return evidence;
}

void finish_nonterminal_life(FlightPreview& flight, int lives_before,
                             std::uint16_t bridge, std::uint32_t score) {
    require(flight.status() == FlightStatus::Crashed &&
                flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                flight.life_cycle().cause.has_value() && flight.lives() == lives_before &&
                flight.bridge_number() == bridge && flight.score() == score,
            "Natural life end changed reserves or connected-session progress at entry");
    const auto expected_checkpoint = flight.world().selected_checkpoint();
    flight.set_controls(neutral);
    for (int tick = 0; tick < 127; ++tick) {
        advance_one_tick(flight);
        if (tick < 126) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding,
                    "Life-end animation ended before its 127-tick boundary");
        }
    }
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.status() == FlightStatus::Crashed && flight.fuel() == kInitialFuel,
            "Checkpoint restore did not begin with a full tank after life end");
    require(flight.last_consumed_contacts() == ForegroundContactSnapshot{},
            "Checkpoint restore consumed contact metadata from the prior world");
    require(flight.shot().pose == PlayerShotPose::Hidden &&
                flight.shot().state.vertical_position == 0,
            "Checkpoint restore did not clear the player shot");
    // The selector predicts a future death and can advance during rebuild.
    // Compare the anchor actually restored, not that new hypothetical choice.
    require(RiverCheckpoint{flight.world().river_state().section,
                            flight.world().river_state().section_anchor} == expected_checkpoint,
            "Checkpoint restore changed the selected connected-session checkpoint");

    for (int tick = 0; tick < 83; ++tick) advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.status() == FlightStatus::Flying && flight.invulnerable() &&
                flight.lives() == lives_before - 1 && flight.bridge_number() == bridge &&
                flight.score() == score && flight.fuel() == kInitialFuel &&
                flight.world().generated_rows() == 156 &&
                RiverCheckpoint{flight.world().river_state().section,
                                flight.world().river_state().section_anchor} == expected_checkpoint,
            "Connected progress or reserve count was lost at checkpoint reveal");
}

void force_right_collision(FlightPreview& flight, std::uint16_t bridge,
                           std::uint32_t score) {
    flight.set_controls(bank_right);
    for (std::size_t tick = 0; tick < 2000; ++tick) {
        advance_one_tick(flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.life_cycle().cause == LifeEndCause::Collision &&
                        flight.bridge_number() == bridge && flight.score() == score,
                    "Held-right collision changed connected-session progress");
            return;
        }
        require(flight.status() == FlightStatus::Flying,
                "Held-right reserve route reached an unexpected lifecycle state");
    }
    throw std::runtime_error("Held-right input did not produce a natural collision");
}

void test_connected_two_bridge_session(test::SessionSceneCapture& captures) {
    FlightPreview flight;
    constexpr std::array options{StartOption{"1"}};
    StartFlow flow(options);
    FlightStart launch(flow, flight);
    launch.set_controls(neutral);
    start_from_title(launch);
    require(flight.status() == FlightStatus::Preparing && flight.running() &&
                flight.selected_option() == 0,
            "Normal F1 start did not prepare the first-option flight");
    test::wait_for_launch(flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156 && flight.lives() == 3 &&
                flight.bridge_number() == 1,
            "Normal startup did not reach the first input-gated flight pass");

    SceneLandmarks scenes{captures};

    const auto first_bridge = replay_input_runs(
        launch, flight, test::first_bridge_session_prefix, false, 2, &scenes);
    require(first_bridge.elapsed_slices == 883 && first_bridge.admitted_ticks == 885 &&
                flight.status() == FlightStatus::Flying && flight.bridge_number() == 2 &&
                flight.score() == 1000 && flight.lives() == 3 &&
                flight.world().generated_rows() == 1033 && flight.fuel() == 39455,
            "Connected prefix did not preserve its verified second-bridge endpoint");

    const auto second_bridge = replay_input_runs(
        launch, flight, test::second_bridge_session_suffix, false, 0, &scenes);
    std::cout << "Connected second-bridge endpoint: slices=" << second_bridge.elapsed_slices
              << " updates=" << second_bridge.admitted_ticks
              << " bridge=" << flight.bridge_number() << " score=" << flight.score()
              << " reserves=" << flight.lives() << " rows=" << flight.world().generated_rows()
              << " fuel=" << flight.fuel() << '\n';
    require(second_bridge.elapsed_slices == 1129 &&
                second_bridge.admitted_ticks == 1132 &&
                flight.status() == FlightStatus::Flying && flight.bridge_number() == 3 &&
                flight.score() == 2190 && flight.lives() == 3 &&
                flight.world().generated_rows() == 2165 && flight.fuel() == 3231 &&
                flight.world().river_state().section == 3,
            "Second-bridge suffix did not preserve the connected checkpoint state");

    const auto fuel_suffix = replay_input_runs(
        launch, flight, test::fuel_exhaustion_session_suffix, true, 0, &scenes);
    require(fuel_suffix.elapsed_slices == 101 && fuel_suffix.admitted_ticks == 101 &&
                flight.status() == FlightStatus::Crashed &&
                flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                flight.life_cycle().cause == LifeEndCause::FuelExhausted &&
                flight.world().generated_rows() == 2265 && flight.bridge_number() == 3 &&
                flight.score() == 2270 && flight.lives() == 3 && flight.fuel() == 31,
            "Connected fuel suffix did not reach its verified natural exhaustion endpoint");
    require(first_bridge.fuel_increase_slices == 3 &&
                second_bridge.fuel_increase_slices == 0 &&
                fuel_suffix.fuel_increase_slices == 0,
            "Connected route lost its three observed refuel/audio events");
    require(!captures.enabled() ||
                (scenes.first_refuel && scenes.ordinary_bridge &&
                 scenes.ordinary_next_slice &&
                 scenes.joint_bridge && scenes.joint_next_slice),
            "Connected route missed a requested event-boundary scene");
    captures.record("fuel_death", flight);
    finish_nonterminal_life(flight, 3, 3, 2270);
    require(flight.world().river_state().section == 3,
            "Fuel-exhaustion rebuild did not retain the section-three checkpoint");
    captures.record("checkpoint_reveal_boundary", flight);

    // The same FlightPreview and FlightStart continue from the checkpoint.
    // Two natural held-right collisions use the remaining reserves, followed
    // by one terminal collision and an ordinary F1 restart.
    for (int reserve = flight.lives(); reserve > 0; --reserve) {
        force_right_collision(flight, 3, 2270);
        finish_nonterminal_life(flight, reserve, 3, 2270);
        require(flight.world().river_state().section == 3,
                "Collision rebuild changed the connected checkpoint section");
    }

    force_right_collision(flight, 3, 2270);
    flight.set_controls(neutral);
    for (int tick = 0; tick < 127; ++tick) advance_one_tick(flight);
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal && flight.lives() == 0 &&
                flight.bridge_number() == 3 && flight.score() == 2270 &&
                flight.high_score() == 2270 && !flight.running(),
            "Connected reserve exhaustion lost score, bridge progress, or high score");
    captures.record("game_over", flight);

    launch.set_controls(neutral);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    require(flight.status() == FlightStatus::Preparing && flight.running() &&
                flight.selected_option() == 0 && flight.bridge_number() == 1 &&
                flight.score() == 0 && flight.lives() == kInitialReserveAircraft &&
                flight.world().generated_rows() == 0 && flight.high_score() == 2270,
            "F1 restart did not reset the session while retaining the high score");
    test::wait_for_launch(flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156 && flight.lives() == 3 &&
                flight.bridge_number() == 1 && flight.score() == 0 &&
                flight.high_score() == 2270,
            "Restarted session did not begin from a clean first-bridge checkpoint");
    captures.record("restart_reveal_boundary", flight);
}

} // namespace

int main(int argc, char** argv) {
    try {
        test::SessionSceneCapture captures;
        captures.configure(argc, argv);
        test_connected_two_bridge_session(captures);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
