#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
    StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"},
    StartOption{"8"}};
constexpr FlightControls fire{false, false, false, false, true};
constexpr FlightControls right_fire{false, false, false, true, true};

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

void test_all_options_launch_from_player_one_port_only() {
    FlightPreview flight;
    for (std::size_t option = 0; option < options.size(); ++option) {
        flight.set_controls({});
        flight.set_joystick_controls({});
        flight.start(option);
        require(flight.life_cycle().active_player == ((option & 1U) != 0 ? 1 : 0),
                "Initial odd-option hidden state lost player 2 selector");
        test::wait_for_launch(flight);
        require(flight.life_cycle().active_player == 0 && flight.invulnerable(),
                "Initial hidden boundary did not select player 1");

        JoystickControls ports{};
        ports[1] = fire;
        flight.set_joystick_controls(ports);
        const auto frozen_rows = flight.world().generated_rows();
        advance_one_tick(flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.world().generated_rows() == frozen_rows &&
                    flight.shot().pose == PlayerShotPose::Hidden,
                "Player 2 port launched player 1's aircraft");

        ports[0] = right_fire;
        flight.set_joystick_controls(ports);
        advance_one_tick(flight);
        require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                    flight.life_cycle().active_player == 0 &&
                    flight.pose() == FlightPose::Right &&
                    flight.shot().start_event.has_value(),
                "Player 1 port did not launch, steer, and fire on the same pass");
    }
}

void test_player_change_routes_the_next_life_to_port_two() {
    FlightPreview flight;
    flight.start(1);
    test::wait_for_launch(flight);
    JoystickControls ports{};
    ports[0] = right_fire;
    flight.set_joystick_controls(ports);
    for (int tick = 0; tick < 2000 && flight.status() != FlightStatus::Crashed; ++tick) {
        advance_one_tick(flight);
    }
    require(flight.status() == FlightStatus::Crashed &&
                flight.life_cycle().phase == LifeCyclePhase::Exploding,
            "Player 1 did not reach a natural bank collision");

    for (int tick = 0; tick < 127; ++tick) advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.life_cycle().active_player == 1 &&
                flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.world().generated_rows() == 0,
            "Player change did not defer its checkpoint rows during reset presentation");
    for (int busy = 0; busy < 5; ++busy) {
        advance_one_tick(flight);
        if (busy < 4) {
            require(flight.checkpoint_reset_work() ==
                        CheckpointResetWorkState{static_cast<std::uint8_t>(4 - busy), 2} &&
                        flight.world().generated_rows() == 0 &&
                        flight.life_cycle().phase == LifeCyclePhase::Rebuilding,
                    "Held outgoing input advanced the pending reset interval");
        }
    }
    require(!flight.checkpoint_reset_work() && flight.world().generated_rows() == 2,
            "Checkpoint reset did not publish its two deferred rows at completion");
    for (int tick = 0; tick < 78; ++tick) advance_one_tick(flight);
    require(flight.status() == FlightStatus::Flying && flight.invulnerable() &&
                flight.life_cycle().active_player == 1 &&
                flight.life_cycle().reserve_aircraft == 3 &&
                flight.life_cycle().inactive_reserve_aircraft == 3 &&
                flight.shot().pose == PlayerShotPose::Hidden &&
                flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156,
            "Natural death did not hand the next aircraft to player 2");
    const auto frozen_rows = flight.world().generated_rows();
    for (int tick = 0; tick < 5; ++tick) advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == frozen_rows &&
                flight.shot().pose == PlayerShotPose::Hidden,
            "Held outgoing port 1 fire launched player 2");

    ports[1] = right_fire;
    flight.set_joystick_controls(ports);
    advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.life_cycle().active_player == 1 &&
                flight.pose() == FlightPose::Right &&
                flight.shot().start_event.has_value(),
            "Incoming port 2 did not launch, steer, and fire");
}

void test_batched_handoff_resamples_a_preheld_incoming_port() {
    FlightPreview flight;
    flight.start(1);
    test::wait_for_launch(flight);
    JoystickControls ports{};
    ports[0] = right_fire;
    flight.set_joystick_controls(ports);
    for (int tick = 0; tick < 2000 && flight.status() != FlightStatus::Crashed; ++tick) {
        advance_one_tick(flight);
    }
    require(flight.status() == FlightStatus::Crashed &&
                flight.life_cycle().active_player == 0,
            "Batched handoff did not begin with a natural player 1 death");

    ports[0] = {};
    ports[1] = fire;
    flight.set_joystick_controls(ports);
    for (int tick = 0; tick < 100; ++tick) advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                flight.life_cycle().active_player == 0,
            "Pre-handoff input did not remain with player 1 during explosion");
    require(flight.advance_elapsed(1280ms) == 64 &&
                flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.life_cycle().active_player == 1,
            "Bounded elapsed batch did not cross the player change");
    require(flight.advance_elapsed(1280ms) == 64 &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.life_cycle().active_player == 1 &&
                flight.life_cycle().reserve_aircraft == 3,
            "Preheld incoming port 2 did not launch after batched handoff");
}

void wait_for_idle_eighth(FlightPreview& flight) {
    for (int batch = 0; batch < 34; ++batch) {
        require(flight.advance_elapsed(1280ms) == 64,
                "Idle batch did not admit its bounded foreground ticks");
        require(flight.status() == FlightStatus::Attract,
                "Natural idle accumulation left the attract session");
        if (flight.watchdog_state().idle_eighth_wraps != 0) return;
    }
    throw std::runtime_error("Attract session did not accumulate one idle watchdog step");
}

void test_both_physical_ports_keep_attract_awake() {
    FlightPreview flight;
    flight.show_options(1, true);
    for (int tick = 0; tick < 24 && flight.status() != FlightStatus::Attract; ++tick) {
        advance_one_tick(flight);
    }
    require(flight.status() == FlightStatus::Attract &&
                flight.life_cycle().active_player == 0,
            "Initial option did not reach the selected attract session");

    wait_for_idle_eighth(flight);
    JoystickControls ports{};
    ports[1] = fire;
    flight.set_joystick_controls(ports);
    require(flight.advance_elapsed(1280ms) == 64 &&
                flight.watchdog_state().idle_eighth_wraps == 0 &&
                flight.status() == FlightStatus::Attract,
            "Inactive port 2 did not clear the idle watchdog");

    flight.set_joystick_controls({});
    wait_for_idle_eighth(flight);
    ports = {};
    ports[0] = fire;
    flight.set_joystick_controls(ports);
    require(flight.advance_elapsed(1280ms) == 64 &&
                flight.watchdog_state().idle_eighth_wraps == 0 &&
                flight.status() == FlightStatus::Attract,
            "Port 1 did not clear the idle watchdog");
}

void test_keyboard_overlay_keeps_opposing_direction_priority() {
    FlightPreview conflict;
    FlightPreview expected;
    conflict.start(0);
    expected.start(0);
    test::wait_for_launch(conflict);
    test::wait_for_launch(expected);

    conflict.set_controls({true, false, false, true, false});
    JoystickControls ports{};
    ports[0] = {false, true, true, false, false};
    ports[1] = right_fire;
    conflict.set_joystick_controls(ports);
    expected.set_controls({true, false, false, true, false});
    advance_one_tick(conflict);
    advance_one_tick(expected);
    require(conflict.life_cycle().phase == LifeCyclePhase::Active &&
                conflict.steering() == expected.steering() &&
                conflict.pose() == FlightPose::Right &&
                conflict.pose() == expected.pose() &&
                conflict.shot().pose == PlayerShotPose::Hidden,
            "Keyboard overlay lost up/right priority or leaked inactive port 2 fire");
}

void test_flight_start_forwards_physical_ports() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    flow.complete_title_delay();
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    test::wait_for_launch(flight);
    JoystickControls ports{};
    ports[0] = fire;
    launch.set_joystick_controls(ports);
    advance_one_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.shot().start_event.has_value(),
            "FlightStart did not forward player 1 physical fire");
}

} // namespace

int main() {
    try {
        test_all_options_launch_from_player_one_port_only();
        test_player_change_routes_the_next_life_to_port_two();
        test_batched_handoff_resamples_a_preheld_incoming_port();
        test_both_physical_ports_keep_attract_awake();
        test_keyboard_overlay_keeps_opposing_direction_priority();
        test_flight_start_forwards_physical_ports();
    } catch (const std::exception& error) {
        std::cerr << "joystick session test failed: " << error.what() << '\n';
        return 1;
    }
}
