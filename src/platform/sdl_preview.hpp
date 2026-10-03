#pragma once

#include "render/glyph_atlas.hpp"
#include "game/start_flow.hpp"
#include "game/flight_controls.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

namespace river_raid {
struct PreviewCallbacks {
    std::function<Frame()> render;
    std::function<void(StartButton, bool)> button;
    std::function<void()> step;
    std::function<void(std::chrono::nanoseconds)> elapsed{};
    std::function<void(FlightControls)> flight_controls{};
    std::function<std::vector<float>()> take_audio{};
    std::function<void(JoystickControls)> joystick_controls{};
    std::function<std::uint64_t()> audio_session_generation{};
};

struct PreviewTiming {
    std::function<std::uint64_t()> now_ns;
};

// Forward one button event and report whether it started a new audio session.
[[nodiscard]] bool dispatch_preview_button(const PreviewCallbacks& callbacks,
                                           StartButton button, bool pressed);

int show_preview(const Frame& frame, bool smoke_test);
int show_preview(const PreviewCallbacks& callbacks, bool smoke_test,
                 PreviewTiming timing = {});
}
