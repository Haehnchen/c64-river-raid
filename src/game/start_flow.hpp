#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace river_raid {

enum class StartStage {
    Title,
    Options,
    StartRequested,
    Blank,
};

enum class StartButton {
    Proceed,
    NextOption,
};

struct StartOption {
    std::string_view value;
};

class StartFlow {
public:
    explicit StartFlow(std::span<const StartOption> options);

    void set_button(StartButton button, bool pressed);
    void complete_title_delay();
    void return_to_options() noexcept;
    void blank_display() noexcept;
    void return_to_title() noexcept;

    [[nodiscard]] StartStage stage() const noexcept;
    [[nodiscard]] std::size_t selected_option_index() const noexcept;
    [[nodiscard]] const StartOption& selected_option() const noexcept;
    [[nodiscard]] bool controls_released() const noexcept;
    [[nodiscard]] bool button_pressed(StartButton button) const noexcept;

private:
    [[nodiscard]] bool any_control_pressed() const noexcept;
    void enter_options_if_released() noexcept;

    std::span<const StartOption> options_;
    StartStage stage_{StartStage::Title};
    std::size_t selected_option_index_{};
    bool proceed_pressed_{};
    bool next_option_pressed_{};
    bool input_latched_{};
    bool title_exit_pending_{};
};

} // namespace river_raid
