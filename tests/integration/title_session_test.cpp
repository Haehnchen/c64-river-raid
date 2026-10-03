#include "app/flight_start.hpp"
#include "app/demo_timing_setup.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;
constexpr std::array options{StartOption{"1"}, StartOption{"2"}};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_automatic_title_exit() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    const auto duration = title_delay_duration(DemoRegion::Pal);
    require(duration > 12s && duration < 13s, "PAL title approximation changed");
    require(title_delay_duration(DemoRegion::Ntsc) > 11s &&
                title_delay_duration(DemoRegion::Ntsc) < 12s,
            "NTSC title approximation changed");
    require(launch.advance_elapsed(duration - 1ns) == 0 &&
                flow.stage() == StartStage::Title && flight.status() == FlightStatus::Ready &&
                flight.take_audio().empty(), "Title advanced gameplay or emitted audio");
    require(launch.advance_elapsed(1ns) == 0 && flow.stage() == StartStage::Options &&
                flight.status() == FlightStatus::OptionWait && flight.selected_option() == 0,
            "Timeout failed to enter the ordinary initial options path");
    launch.advance_elapsed(100ms);
    require(flight.status() == FlightStatus::Attract, "Automatic title exit did not reach demo");
    launch.set_button(StartButton::Proceed, true);
    require(flow.stage() == StartStage::StartRequested && flight.running(),
            "F1 did not start from automatic demo");
}

void test_release_gate() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    const auto duration = title_delay_duration(DemoRegion::Pal);
    launch.set_button(StartButton::NextOption, true);
    launch.advance_elapsed(duration * 2);
    require(flow.stage() == StartStage::Title, "Initial held key consumed title delay");
    launch.set_button(StartButton::NextOption, false);
    launch.advance_elapsed(1ns);
    launch.set_button(StartButton::NextOption, true);
    launch.advance_elapsed(duration);
    require(flow.stage() == StartStage::Title && flight.status() == FlightStatus::Ready,
            "Expired title ignored held-key release gate");
    launch.set_button(StartButton::NextOption, false);
    require(flow.stage() == StartStage::Options && flight.status() == FlightStatus::OptionWait &&
                flow.selected_option_index() == 0, "Final release did not enter default options");
}

void test_elapsed_partition_and_early_f1() {
    StartFlow batched_flow(options), split_flow(options);
    FlightPreview batched, split;
    FlightStart batched_start(batched_flow, batched), split_start(split_flow, split);
    const auto duration = title_delay_duration(DemoRegion::Pal);
    batched_start.advance_elapsed(duration + 100ms);
    split_start.advance_elapsed(duration / 2);
    split_start.advance_elapsed(duration - duration / 2);
    split_start.advance_elapsed(100ms);
    require(batched.status() == split.status() &&
                batched.world().generated_rows() == split.world().generated_rows() &&
                batched.idle_state() == split.idle_state(),
            "Title timeout lost or replayed elapsed time");

    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    launch.advance_elapsed(1s);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    launch.advance_elapsed(100ms);
    require(flow.stage() == StartStage::Options && flight.status() == FlightStatus::Attract,
            "F1 early exit waited for title deadline");
}
} // namespace

int main() {
    try {
        test_automatic_title_exit();
        test_release_gate();
        test_elapsed_partition_and_early_f1();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
