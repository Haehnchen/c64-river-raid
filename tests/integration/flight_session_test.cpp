#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"
#include "session_presets.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

constexpr FlightControls bank_right{false, false, false, true, true};
constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
    StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"},
    StartOption{"8"}};

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void advance_one_tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto completed = flight.advance_elapsed(10ms);
        require(completed <= 1, "One input slice advanced multiple ticks");
        if (completed == 1) return;
    }
    throw std::runtime_error("Flight did not admit one tick within its bounded PAL interval");
}

void test_all_options_use_the_selected_session_seed() {
    FlightPreview flight;
    for (std::size_t option = 0; option < options.size(); ++option) {
        const auto& asset = assets::session_presets::presets[option / 2];
        const RiverSectionAnchor anchor{
            {asset.anchor_random[0], asset.anchor_random[1], asset.anchor_random[2]},
            asset.course};
        flight.start(option);
        const auto& state = flight.world().river_state();
        require(flight.selected_option() == option && flight.two_players() == ((option & 1U) != 0) &&
                    state.section == asset.section && state.section_anchor == anchor &&
                    state.random == anchor.random && state.course == anchor.course &&
                    flight.bridge_number() == asset.bridge_number,
                "Option did not select its exported course and bridge seed");
        require(flight.life_cycle().active_player == ((option & 1U) != 0 ? 1 : 0) &&
                    flight.life_cycle().reserve_aircraft == 4 &&
                    flight.life_cycle().inactive_reserve_aircraft == ((option & 1U) != 0 ? 4 : 0) &&
                    flight.player_score(0) == 0 && flight.player_score(1) == 0,
                "Option did not seed the expected player allocations and scores");

        test::wait_for_launch(flight);
        require(flight.life_cycle().active_player == 0 && flight.lives() == 3 &&
                    flight.life_cycle().inactive_reserve_aircraft == ((option & 1U) != 0 ? 4 : 0) &&
                    flight.world().river_state().section == asset.section &&
                    flight.bridge_number() == asset.bridge_number,
                "Launch did not reveal player 1 on the selected section");
    }
}

void test_neutral_launch_wait_preserves_geometry() {
    FlightPreview flight;
    flight.start();
    require(flight.animation_phase() == 0, "Start did not reset the animation phase");
    test::wait_for_launch(flight);
    const auto phase = flight.animation_phase();
    const auto objects = flight.world().object_state();
    const auto rows = flight.world().generated_rows();
    const auto steering = flight.steering();
    const auto fuel = flight.fuel();
    for (unsigned wait = 1; wait <= 5; ++wait) {
        advance_one_tick(flight);
        (void)flight.take_audio();
        require(flight.animation_phase() == static_cast<std::uint8_t>(phase + wait) &&
                    flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.world().generated_rows() == rows && flight.steering() == steering &&
                    flight.fuel() == fuel,
                "Neutral launch wait changed flight geometry or stopped advancing phase");
        for (std::size_t index = 0; index < objects.records.size(); ++index) {
            const auto& current = flight.world().object_state().records[index];
            require(current.vertical_position == objects.records[index].vertical_position &&
                        current.horizontal_coordinate == objects.records[index].horizontal_coordinate,
                    "Neutral launch wait moved an object");
        }
    }
}

void test_menu_selection_starts_the_selected_session() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    require(flow.stage() == StartStage::Options, "Title did not enter options");
    for (int choice = 0; choice < 5; ++choice) {
        launch.set_button(StartButton::NextOption, true);
        launch.set_button(StartButton::NextOption, false);
    }
    launch.set_button(StartButton::Proceed, true);
    require(flow.stage() == StartStage::StartRequested && flight.selected_option() == 5 &&
                flight.two_players() && flight.world().river_state().section == 20 &&
                flight.bridge_number() == 20 && flight.running(),
            "Menu start discarded Game 6's selected session");
    launch.set_button(StartButton::Proceed, false);
    test::wait_for_launch(flight);
    launch.set_button(StartButton::Proceed, true);
    require(flight.selected_option() == 5 && flight.world().river_state().section == 20 &&
                flight.life_cycle().active_player == 1 &&
                flight.life_cycle().inactive_reserve_aircraft == 4,
            "Menu restart lost the selected two-player seed");
}

void drive_natural_bank_death(FlightPreview& flight) {
    flight.set_controls(bank_right);
    for (int tick = 0; tick < 2000; ++tick) {
        advance_one_tick(flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding,
                    "Bank contact skipped the explosion entry");
            return;
        }
        require(flight.status() == FlightStatus::Flying,
                "Held-right flight left the active path before contact");
    }
    throw std::runtime_error("Held-right input did not cause a natural bank collision");
}

void finish_death_with_neutral_input(FlightPreview& flight, bool terminal) {
    flight.set_controls({});
    const auto frozen_rows = flight.world().generated_rows();
    for (int tick = 1; tick <= 127; ++tick) {
        advance_one_tick(flight);
        if (tick < 127) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.world().generated_rows() == frozen_rows,
                    "Explosion advanced world rows or ended early");
        }
    }
    if (terminal) {
        require(flight.status() == FlightStatus::GameOver &&
                    flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                    flight.life_cycle().active_player == 0,
                "Both-player final death did not normalize to terminal player 1");
        return;
    }
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.life_cycle().timer == 0x7f && flight.world().generated_rows() == 0,
            "Player change did not defer its first checkpoint rows during reset presentation");
    for (int busy = 0; busy < 5; ++busy) {
        advance_one_tick(flight);
        if (busy < 4) {
            require(flight.checkpoint_reset_work() ==
                        CheckpointResetWorkState{static_cast<std::uint8_t>(4 - busy), 2} &&
                        flight.world().generated_rows() == 0 &&
                        flight.life_cycle().phase == LifeCyclePhase::Rebuilding,
                    "Reset presentation advanced the neutral two-player lifecycle");
        }
    }
    require(!flight.checkpoint_reset_work() && flight.world().generated_rows() == 2,
            "Reset completion did not publish the first two checkpoint rows");
    for (int tick = 0; tick < 78; ++tick) advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.status() == FlightStatus::Flying &&
                flight.world().generated_rows() == 156 && flight.invulnerable(),
            "Neutral rebuild did not pause at the aircraft reveal");
}

void test_natural_two_player_round_and_restart() {
    FlightPreview flight;
    flight.start(1);
    test::wait_for_launch(flight);
    const auto initial_section = flight.world().river_state().section;
    require(initial_section == 1 && flight.life_cycle().active_player == 0 &&
                flight.life_cycle().reserve_aircraft == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 4,
            "Game 2 did not start with player 1's first aircraft");

    std::array<std::uint32_t, 2> scores{};
    std::array<std::uint16_t, 2> bridges{1, 1};
    for (int turn = 0; turn < 8; ++turn) {
        const auto active = static_cast<std::size_t>(turn & 1);
        require(flight.life_cycle().active_player == active &&
                    flight.life_cycle().reserve_aircraft == 3 - turn / 2 &&
                    flight.score() == flight.player_score(active) &&
                    flight.bridge_number() == bridges[active],
                "Turn did not restore the active player's reserves, score, or bridge");
        if (turn < 2) {
            require(flight.world().river_state().section == initial_section,
                    "The first two players did not start on the same section");
        }

        drive_natural_bank_death(flight);
        scores[active] = flight.score();
        bridges[active] = flight.bridge_number();
        require(flight.player_score(0) == scores[0] && flight.player_score(1) == scores[1],
                "Natural contact changed the other player's score slot");
        finish_death_with_neutral_input(flight, turn == 7);
        require(flight.player_score(0) == scores[0] && flight.player_score(1) == scores[1],
                "Player transition merged or cleared fixed score slots");
        if (turn < 7) {
            const auto next = static_cast<std::size_t>((turn + 1) & 1);
            require(flight.life_cycle().active_player == next &&
                        flight.bridge_number() == bridges[next],
                    "Player transition did not restore the next player's bridge");
        }
    }
    require(!flight.running() && flight.high_score() ==
                (scores[0] > scores[1] ? scores[0] : scores[1]),
            "Terminal round did not store the higher player score");

    const auto record = flight.high_score();
    flight.start(1);
    require(flight.selected_option() == 1 && flight.high_score() == record &&
                flight.player_score(0) == 0 && flight.player_score(1) == 0 &&
                flight.bridge_number() == 1 && flight.world().river_state().section == 1 &&
                flight.life_cycle().active_player == 1 &&
                flight.life_cycle().reserve_aircraft == 4 &&
                flight.life_cycle().inactive_reserve_aircraft == 4,
            "Restart did not reset both player starts while retaining the option record");
}

void test_invalid_option_preserves_active_session() {
    FlightPreview flight;
    flight.start(5);
    test::wait_for_launch(flight);
    const auto before_state = flight.life_cycle();
    const auto before_world = flight.world().river_state();
    const auto before_rows = flight.world().generated_rows();
    const auto before_bridge = flight.bridge_number();
    bool rejected = false;
    try {
        flight.start(8);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    require(rejected && flight.selected_option() == 5 && flight.two_players() &&
                flight.life_cycle() == before_state &&
                flight.world().river_state() == before_world &&
                flight.world().generated_rows() == before_rows &&
                flight.bridge_number() == before_bridge && flight.running(),
            "Invalid option changed the active session");
}

void test_restart_uses_lifecycle_and_shared_release_gate() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    flow.complete_title_delay();
    const auto release = [&] { launch.set_button(StartButton::Proceed, false); };
    const auto proceed = [&] { launch.set_button(StartButton::Proceed, true); };
    proceed();
    release();
    advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.world().generated_rows() == 0,
            "Initial reset entry did not defer the first two rows");
    proceed();
    require(flight.status() == FlightStatus::Preparing && flight.world().generated_rows() == 0,
            "F1 did not restart the inactive initial rebuild");
    release();
    test::wait_for_launch(flight);

    proceed();
    require(flight.status() == FlightStatus::Preparing && flight.world().generated_rows() == 0,
            "Full key release did not permit restart at launch wait");
    release();
    test::wait_for_launch(flight);

    flight.set_controls(bank_right);
    advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Active,
            "Normal input did not resume flight");
    const auto active_state = flight.life_cycle();
    const auto active_rows = flight.world().generated_rows();
    launch.set_button(StartButton::NextOption, true);
    launch.set_button(StartButton::NextOption, false);
    require(flow.stage() == StartStage::StartRequested && flight.selected_option() == 0 &&
                flight.life_cycle() == active_state && flight.world().generated_rows() == active_rows,
            "Active-flight F3 changed the session");
    proceed();
    require(flight.life_cycle() == active_state && flight.world().generated_rows() == active_rows,
            "Active-flight F1 reset the session");
    drive_natural_bank_death(flight);
    const auto crashed = flight.life_cycle();
    proceed();
    require(flight.life_cycle() == crashed, "Held F1 was deferred until a crash");
    release();

    const auto rows_before_options = flight.world().generated_rows();
    const auto objects_before_options = flight.world().object_state();
    const std::vector<PackedRiverRow> viewport_before_options(
        flight.world().viewport().rows().begin(), flight.world().viewport().rows().end());
    require(flight.life_cycle().phase == LifeCyclePhase::Exploding && flight.lives() == 3,
            "F3 test did not reach an inactive life with a reserve");

    launch.set_button(StartButton::NextOption, true);
    const auto after_f3 = flight.life_cycle();
    require(flow.stage() == StartStage::Options && flow.selected_option_index() == 1 &&
                flight.selected_option() == 1 && flight.status() == FlightStatus::OptionWait &&
                after_f3.phase == LifeCyclePhase::Terminal &&
                after_f3.image == LifeCycleImage::Hidden && after_f3.reserve_aircraft == 0 &&
                after_f3.inactive_reserve_aircraft == 0 && flight.player_score(0) == 0 &&
                flight.player_score(1) == 0 &&
                flight.idle_state() == SessionIdleState{SessionIdlePhase::Waiting, 37},
            "F3 did not clear the session and return to the next option");
    require(flight.world().generated_rows() == rows_before_options &&
                flight.world().object_state() == objects_before_options &&
                std::vector<PackedRiverRow>(flight.world().viewport().rows().begin(),
                                            flight.world().viewport().rows().end()) ==
                    viewport_before_options,
            "F3 option presentation discarded the current viewport or objects");

    proceed();
    require(flow.stage() == StartStage::Options && flight.life_cycle() == after_f3 &&
                flight.status() == FlightStatus::OptionWait &&
                flight.world().generated_rows() == rows_before_options,
            "F1 was accepted while F3 remained held");
    release();
    proceed();
    require(flow.stage() == StartStage::Options && flight.life_cycle() == after_f3 &&
                flight.status() == FlightStatus::OptionWait,
            "Partial key release unlocked F1 while F3 remained held");
    release();
    launch.set_button(StartButton::NextOption, false);
    proceed();
    require(flow.stage() == StartStage::StartRequested && flight.selected_option() == 1 &&
                flight.status() == FlightStatus::Preparing && flight.world().generated_rows() == 0,
            "Full key release did not permit F1 to start the selected option");
}

} // namespace

int main() {
    try {
        test_all_options_use_the_selected_session_seed();
        test_neutral_launch_wait_preserves_geometry();
        test_menu_selection_starts_the_selected_session();
        test_natural_two_player_round_and_restart();
        test_invalid_option_preserves_active_session();
        test_restart_uses_lifecycle_and_shared_release_gate();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
