#include "app/flight_start.hpp"

#include "app/demo_timing_setup.hpp"

#include <algorithm>

namespace river_raid {

FlightStart::FlightStart(StartFlow& flow, FlightPreview& flight)
    : flow_(flow), flight_(flight), title_delay_(title_delay_duration(DemoRegion::Pal)) {}

std::size_t FlightStart::advance_elapsed(std::chrono::nanoseconds elapsed) {
    if (flow_.stage() != StartStage::Title) return advance_session(elapsed);
    const auto remaining = title_delay_.remaining();
    if (!title_delay_.advance_elapsed(elapsed, flow_.controls_released())) return 0;
    flow_.complete_title_delay();
    if (flow_.stage() == StartStage::Title) return 0;
    flight_.show_options(flow_.selected_option_index(), true);
    return advance_session(elapsed - std::min(elapsed, remaining));
}

void FlightStart::set_button(StartButton button, bool pressed) {
    if (flow_.stage() == StartStage::Blank) {
        flow_.set_button(button, pressed);
        return;
    }
    if (pressed && flow_.controls_released()) flight_.note_function_key();
    const auto previous = flow_.stage();
    const auto previous_option = flow_.selected_option_index();
    const bool restart = button == StartButton::Proceed && pressed && flow_.controls_released() &&
                         flow_.stage() == StartStage::StartRequested &&
                         flight_.accepts_session_input();
    if (button == StartButton::NextOption && pressed && flow_.controls_released() &&
        previous == StartStage::StartRequested && flight_.accepts_session_input()) {
        flow_.return_to_options();
    }
    flow_.set_button(button, pressed);
    if (restart || (previous != StartStage::StartRequested &&
                    flow_.stage() == StartStage::StartRequested)) {
        flight_.start(flow_.selected_option_index());
        ++start_generation_;
        flight_.set_controls(controls_);
    } else if (flow_.stage() == StartStage::Options &&
               (previous != StartStage::Options ||
                previous_option != flow_.selected_option_index())) {
        flight_.show_options(flow_.selected_option_index(), previous == StartStage::Title);
    }
}

void FlightStart::set_controls(FlightControls controls) {
    controls_ = controls;
    flight_.set_controls(controls);
}

void FlightStart::set_joystick_controls(JoystickControls controls) {
    joystick_controls_ = controls;
    flight_.set_joystick_controls(controls);
}

std::size_t FlightStart::advance_session(std::chrono::nanoseconds elapsed) {
    const auto ticks = flight_.advance_elapsed(elapsed);
    if (flight_.status() != FlightStatus::Blank) return ticks;
    if (flow_.stage() != StartStage::Blank) {
        flow_.blank_display();
        previous_wake_sample_ = 0;
        return ticks;
    }
    if (ticks == 0) return 0;
    const auto sample = wake_sample();
    if (sample != 0 && sample == previous_wake_sample_) {
        flight_.return_to_title();
        flight_.set_controls(controls_);
        flight_.set_joystick_controls(joystick_controls_);
        flow_.return_to_title();
        title_delay_ = TitleDelay(title_delay_duration(DemoRegion::Pal));
        previous_wake_sample_ = 0;
    } else {
        previous_wake_sample_ = sample;
    }
    return ticks;
}

std::uint8_t FlightStart::wake_sample() const noexcept {
    const auto wake = combine_controls(controls_, joystick_controls_[0]);
    return static_cast<std::uint8_t>(
        (flow_.button_pressed(StartButton::Proceed) ? 1U : 0U) |
        (flow_.button_pressed(StartButton::NextOption) ? 2U : 0U) |
        (wake.up ? 4U : 0U) | (wake.down ? 8U : 0U) |
        (wake.left ? 16U : 0U) | (wake.right ? 32U : 0U) |
        (wake.fire ? 64U : 0U));
}

} // namespace river_raid
