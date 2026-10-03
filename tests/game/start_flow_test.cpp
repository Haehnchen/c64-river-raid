#include "game/start_flow.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

constexpr std::array options{
    river_raid::StartOption{"A"}, river_raid::StartOption{"B"},
    river_raid::StartOption{"C"}, river_raid::StartOption{"D"},
    river_raid::StartOption{"E"}, river_raid::StartOption{"F"},
    river_raid::StartOption{"G"}, river_raid::StartOption{"H"},
};

void test_title_proceed_waits_for_release() {
    river_raid::StartFlow flow(options);
    require(flow.stage() == river_raid::StartStage::Title, "Flow must begin at the title");
    require(flow.selected_option().value == "A", "Game A must be the default");

    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Title, "Proceed press must not leave the title");
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Title, "Repeated key-down must be ignored");
    flow.set_button(river_raid::StartButton::Proceed, false);
    require(flow.stage() == river_raid::StartStage::Options, "Proceed release must show options");
}

void test_title_waits_until_all_flow_controls_are_released() {
    river_raid::StartFlow flow(options);
    require(flow.controls_released(), "Initial function keys must be released");
    flow.set_button(river_raid::StartButton::Proceed, true);
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::Proceed, false);
    require(!flow.controls_released(), "Partial release incorrectly unlocked function keys");
    require(flow.stage() == river_raid::StartStage::Title, "Another held control must gate options");
    flow.set_button(river_raid::StartButton::NextOption, false);
    require(flow.controls_released(), "Full release did not unlock function keys");
    require(flow.stage() == river_raid::StartStage::Options, "Final release must show options");
    require(flow.selected_option().value == "A", "Title input must not change the option");
}

void test_explicit_title_delay_completion_uses_the_release_gate() {
    river_raid::StartFlow flow(options);
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.complete_title_delay();
    require(flow.stage() == river_raid::StartStage::Title, "Held input must gate title timeout");
    flow.set_button(river_raid::StartButton::NextOption, false);
    require(flow.stage() == river_raid::StartStage::Options, "Timeout must finish after release");

    river_raid::StartFlow idle_flow(options);
    idle_flow.complete_title_delay();
    require(idle_flow.stage() == river_raid::StartStage::Options,
            "Idle title timeout must show options immediately");
}

void test_next_option_uses_press_edges_and_wraps() {
    river_raid::StartFlow flow(options);
    flow.complete_title_delay();

    for (std::size_t index = 1; index < options.size(); ++index) {
        flow.set_button(river_raid::StartButton::NextOption, true);
        require(flow.selected_option_index() == index, "Option press selected the wrong game");
        flow.set_button(river_raid::StartButton::NextOption, true);
        require(flow.selected_option_index() == index, "Held option control must not repeat");
        flow.set_button(river_raid::StartButton::NextOption, false);
    }
    flow.set_button(river_raid::StartButton::NextOption, true);
    require(flow.selected_option_index() == 0, "Game H must wrap to game A");
}

void test_options_proceed_requests_start_on_press() {
    river_raid::StartFlow flow(options);
    flow.complete_title_delay();
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::NextOption, false);
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::StartRequested,
            "Proceed press in options must request gameplay");
    require(flow.selected_option().value == "B", "Start request must preserve the selection");
}

void test_another_button_waits_for_full_release() {
    river_raid::StartFlow flow(options);
    flow.complete_title_delay();
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Options,
            "A second button must not trigger while the first is held");
    flow.set_button(river_raid::StartButton::NextOption, false);
    flow.set_button(river_raid::StartButton::Proceed, false);
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::StartRequested,
            "Input must resume after a full release");
}

void test_blank_tracks_buttons_without_processing_start_or_options() {
    river_raid::StartFlow flow(options);
    flow.complete_title_delay();
    for (int index = 0; index < 3; ++index) {
        flow.set_button(river_raid::StartButton::NextOption, true);
        flow.set_button(river_raid::StartButton::NextOption, false);
    }
    require(flow.stage() == river_raid::StartStage::Options &&
                flow.selected_option_index() == 3,
            "Blank test setup did not select a nondefault option");

    flow.blank_display();
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Blank &&
                flow.selected_option_index() == 3 &&
                flow.button_pressed(river_raid::StartButton::NextOption) &&
                flow.button_pressed(river_raid::StartButton::Proceed),
            "Blank state did not record held inputs while ignoring menu actions");
}

void test_title_return_defaults_option_and_latches_wake_input() {
    river_raid::StartFlow flow(options);
    flow.complete_title_delay();
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::NextOption, false);
    flow.set_button(river_raid::StartButton::NextOption, true);
    flow.set_button(river_raid::StartButton::NextOption, false);
    require(flow.selected_option_index() == 2, "Return test setup selected wrong option");

    flow.blank_display();
    flow.set_button(river_raid::StartButton::NextOption, true);
    require(flow.stage() == river_raid::StartStage::Blank &&
                flow.button_pressed(river_raid::StartButton::NextOption),
            "Blank state did not retain the wake key");

    flow.return_to_title();
    require(flow.stage() == river_raid::StartStage::Title &&
                flow.selected_option_index() == 0 && !flow.controls_released(),
            "Title return did not reset the option and latch the held wake key");
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Title &&
                flow.button_pressed(river_raid::StartButton::NextOption) &&
                flow.button_pressed(river_raid::StartButton::Proceed),
            "Held wake key allowed F1 to leave the returned title");

    flow.set_button(river_raid::StartButton::Proceed, false);
    require(flow.stage() == river_raid::StartStage::Title && !flow.controls_released(),
            "Partial release unlocked the wake-key latch");
    flow.set_button(river_raid::StartButton::NextOption, false);
    require(flow.stage() == river_raid::StartStage::Title && flow.controls_released(),
            "Full release unexpectedly left the title without a new F1 edge");
    flow.set_button(river_raid::StartButton::Proceed, true);
    require(flow.stage() == river_raid::StartStage::Title,
            "Fresh F1 press should wait for its release");
    flow.set_button(river_raid::StartButton::Proceed, false);
    require(flow.stage() == river_raid::StartStage::Options &&
                flow.selected_option_index() == 0,
            "Fresh F1 after wake release did not enter default options");
}

void test_empty_options_are_rejected() {
    bool rejected = false;
    try {
        (void)river_raid::StartFlow(std::span<const river_raid::StartOption>{});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "An empty option table must be rejected");
}

} // namespace

int main() {
    try {
        test_title_proceed_waits_for_release();
        test_title_waits_until_all_flow_controls_are_released();
        test_explicit_title_delay_completion_uses_the_release_gate();
        test_next_option_uses_press_edges_and_wraps();
        test_options_proceed_requests_start_on_press();
        test_another_button_waits_for_full_release();
        test_blank_tracks_buttons_without_processing_start_or_options();
        test_title_return_defaults_option_and_latches_wake_input();
        test_empty_options_are_rejected();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
