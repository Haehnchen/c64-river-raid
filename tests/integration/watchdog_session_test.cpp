#include "app/flight_start.hpp"
#include "app/demo_timing_setup.hpp"
#include "app/flight_scene.hpp"

#include <algorithm>
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

void press(FlightStart& launch, StartButton button) {
    launch.set_button(button, true);
    launch.set_button(button, false);
}

void test_attract_handoff_counts_one_restart_wrap() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    launch.advance_elapsed(20ms);
    require(flight.watchdog_state().wraps_toward_eighth == 1,
            "Initial idle phase wrap was lost");
    launch.advance_elapsed(20ms);
    require(flight.watchdog_state().wraps_toward_eighth == 1,
            "Queued attract entry published a duplicate watchdog wrap");
    launch.advance_elapsed(20ms);
    require(flight.status() == FlightStatus::Attract &&
                flight.watchdog_state().wraps_toward_eighth == 2,
            "Attract restart did not preserve the initial wrap subdivision");
    press(launch, StartButton::NextOption);
    require(flight.watchdog_state() == IdleWatchdogState{},
            "Accepted option selection retained the old watchdog subdivision");
}

void test_natural_idle_blank_and_wake() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    press(launch, StartButton::Proceed);
    launch.set_controls({false, false, false, false, true});
    for (unsigned pass = 0; pass < 10000 && flight.running(); ++pass)
        launch.advance_elapsed(20ms);
    require(flight.status() == FlightStatus::GameOver && flight.high_score() != 0,
            "Natural round did not establish a terminal record");
    const auto record = flight.high_score();
    launch.set_controls({});
    for (unsigned batch = 0; batch < 200 && flight.watchdog_state().idle_eighth_wraps < 2; ++batch)
        launch.advance_elapsed(1s);
    require(flight.status() == FlightStatus::Attract &&
                flight.watchdog_state().idle_eighth_wraps >= 2,
            "Attract reset or stopped the long-idle counter");
    launch.set_controls({false, false, true, false, false});
    launch.advance_elapsed(100ms);
    require(flight.watchdog_state().idle_eighth_wraps == 0,
            "Held joystick input did not clear the long-idle count");
    launch.set_controls({});

    std::size_t admitted = 0;
    for (unsigned batch = 0; batch < 5300 && flight.status() != FlightStatus::Blank; ++batch)
        admitted += launch.advance_elapsed(1s);
    require(flight.status() == FlightStatus::Blank && flow.stage() == StartStage::Blank &&
                flight.watchdog_state().blanked &&
                flight.watchdog_state().idle_eighth_wraps == 128 &&
                admitted > 250000 && admitted < 265000,
            "Natural idle session failed to reach the full watchdog threshold");
    require(!flight.accepts_session_input() && !flight.running(),
            "Blank display still accepts a gameplay restart");
    const auto frame = make_flight_scene(flight);
    require(frame.width == 320 && frame.height == 200 &&
                std::ranges::all_of(frame.pixels, [](auto pixel) { return pixel == 0xff000000; }),
            "Blank display retained visible sprites or terrain");
    const auto world = flight.world();
    const auto audio = flight.audio_state();
    const auto idle = flight.idle_state();
    launch.advance_elapsed(1s);
    require(flight.world().generated_rows() == world.generated_rows() &&
                flight.world().object_state() == world.object_state() &&
                flight.audio_state() == audio && flight.idle_state() == idle &&
                flight.high_score() == record && !flight.take_audio().empty(),
            "Blank state advanced gameplay/controller or stopped synthesizer output");

    auto joystick_flow = flow;
    auto joystick_flight = flight;
    FlightStart joystick_launch(joystick_flow, joystick_flight);
    const FlightControls fire{false, false, false, false, true};
    joystick_launch.set_joystick_controls({FlightControls{}, fire});
    joystick_launch.advance_elapsed(100ms);
    joystick_launch.advance_elapsed(100ms);
    require(joystick_flow.stage() == StartStage::Blank,
            "The second joystick port incorrectly woke the blank display");
    joystick_launch.set_joystick_controls({fire, FlightControls{}});
    joystick_launch.advance_elapsed(100ms);
    require(joystick_flow.stage() == StartStage::Blank, "First-port wake bypassed stability");
    joystick_launch.advance_elapsed(100ms);
    require(joystick_flow.stage() == StartStage::Title && joystick_flight.high_score() == record,
            "The first joystick port failed to wake the record-preserving title");

    launch.set_button(StartButton::NextOption, true);
    launch.advance_elapsed(100ms);
    require(flow.stage() == StartStage::Blank, "One wake sample bypassed stability check");
    launch.set_button(StartButton::NextOption, false);
    launch.set_button(StartButton::Proceed, true);
    launch.advance_elapsed(100ms);
    require(flow.stage() == StartStage::Blank, "Changed wake input counted as stable");
    launch.advance_elapsed(0ns);
    require(flow.stage() == StartStage::Blank, "No admitted sample woke the title");
    launch.advance_elapsed(100ms);
    require(flow.stage() == StartStage::Title && flow.selected_option_index() == 0 &&
                flight.status() == FlightStatus::Ready && flight.high_score() == record &&
                flight.world().generated_rows() == 0 && flight.take_audio().empty(),
            "Stable wake did not rebuild the silent title while retaining records");
    launch.advance_elapsed(title_delay_duration(DemoRegion::Pal));
    require(flow.stage() == StartStage::Title, "Held wake key consumed the new title delay");
    launch.set_button(StartButton::Proceed, false);
    require(flow.stage() == StartStage::Title, "Wake-key release also skipped the title");
    launch.advance_elapsed(title_delay_duration(DemoRegion::Pal));
    require(flow.stage() == StartStage::Options && flight.high_score() == record,
            "Returned title did not use the ordinary automatic options path");
    press(launch, StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested && flight.running() &&
                flight.high_score() == record && flight.watchdog_state() == IdleWatchdogState{},
            "F1 after title return failed to start the retained-record session");
}
} // namespace

int main() {
    try {
        test_attract_handoff_counts_one_restart_wrap();
        test_natural_idle_blank_and_wake();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
