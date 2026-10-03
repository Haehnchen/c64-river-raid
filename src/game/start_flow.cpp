#include "game/start_flow.hpp"

#include <stdexcept>

namespace river_raid {

StartFlow::StartFlow(std::span<const StartOption> options) : options_(options) {
    if (options_.empty()) {
        throw std::invalid_argument("StartFlow needs at least one option");
    }
}

void StartFlow::set_button(StartButton button, bool pressed) {
    bool* current = button == StartButton::Proceed ? &proceed_pressed_ : &next_option_pressed_;
    if (*current == pressed) {
        return;
    }
    *current = pressed;

    if (!pressed) {
        if (!any_control_pressed()) {
            input_latched_ = false;
        }
        enter_options_if_released();
        return;
    }
    if (input_latched_) {
        return;
    }
    input_latched_ = true;

    if (stage_ == StartStage::Title) {
        if (button == StartButton::Proceed) {
            title_exit_pending_ = true;
        }
        return;
    }
    if (stage_ != StartStage::Options) {
        return;
    }

    if (button == StartButton::Proceed) {
        stage_ = StartStage::StartRequested;
    } else {
        selected_option_index_ = (selected_option_index_ + 1) % options_.size();
    }
}

void StartFlow::complete_title_delay() {
    if (stage_ != StartStage::Title) {
        return;
    }
    title_exit_pending_ = true;
    enter_options_if_released();
}

StartStage StartFlow::stage() const noexcept {
    return stage_;
}

void StartFlow::return_to_options() noexcept {
    if (stage_ == StartStage::StartRequested) stage_ = StartStage::Options;
}

void StartFlow::blank_display() noexcept {
    stage_ = StartStage::Blank;
    title_exit_pending_ = false;
}

void StartFlow::return_to_title() noexcept {
    stage_ = StartStage::Title;
    selected_option_index_ = 0;
    title_exit_pending_ = false;
    input_latched_ = any_control_pressed();
}

bool StartFlow::button_pressed(StartButton button) const noexcept {
    return button == StartButton::Proceed ? proceed_pressed_ : next_option_pressed_;
}

std::size_t StartFlow::selected_option_index() const noexcept {
    return selected_option_index_;
}

const StartOption& StartFlow::selected_option() const noexcept {
    return options_[selected_option_index_];
}

bool StartFlow::controls_released() const noexcept {
    return !any_control_pressed();
}

bool StartFlow::any_control_pressed() const noexcept {
    return proceed_pressed_ || next_option_pressed_;
}

void StartFlow::enter_options_if_released() noexcept {
    if (stage_ == StartStage::Title && title_exit_pending_ && !any_control_pressed()) {
        stage_ = StartStage::Options;
        title_exit_pending_ = false;
    }
}

} // namespace river_raid
