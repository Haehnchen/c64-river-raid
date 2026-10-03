#include "platform/sdl_preview.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using river_raid::Frame;
using river_raid::PreviewCallbacks;
using river_raid::PreviewTiming;
using river_raid::StartButton;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void push_event(SDL_Event event) {
    expect(SDL_PushEvent(&event), "Could not push deterministic SDL event");
}

void push_window_event(SDL_EventType type, SDL_WindowID window_id = 0) {
    SDL_Event event{};
    event.type = type;
    // Zero is the synthetic-window ID accepted by the preview bridge.
    event.window.windowID = window_id;
    push_event(event);
}

void push_key(SDL_Keycode key, bool pressed, bool repeat = false, SDL_WindowID window_id = 0) {
    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = window_id;
    event.key.key = key;
    event.key.down = pressed;
    event.key.repeat = repeat;
    push_event(event);
}

void push_quit() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    push_event(event);
}

Frame test_frame() {
    return Frame{2, 2, std::vector<std::uint32_t>(4, 0xff102030)};
}

PreviewTiming clock_for(std::uint64_t& now) {
    return PreviewTiming{[&now] { return now; }};
}

void test_hidden_preview_starts_inactive() {
    std::uint64_t now = 10;
    int renders = 0;
    std::vector<std::chrono::nanoseconds> elapsed;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 2) {
                // The first loop ran while the hidden window was inactive.
                now = 100;
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
            } else if (renders == 3) {
                now = 150;
                push_quit();
            }
            return test_frame();
        },
        {},
        {},
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, true, clock_for(now)) == 0,
           "Inactive preview did not return successfully");
    expect(renders == 3, "Inactive preview rendered after its quit event");
    expect(elapsed.size() == 1 && elapsed.front() == 0ns,
           "Hidden inactive time leaked into the first active elapsed callback");
}

void test_default_clock_smoke() {
    int renders = 0;
    std::vector<std::chrono::nanoseconds> elapsed;
    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 1) push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
            return test_frame();
        },
        {},
        {},
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, true) == 0,
           "Default-clock preview smoke did not return successfully");
    expect(renders == 4 && !elapsed.empty(),
           "Default-clock preview did not complete its smoke frames");
    for (const auto value : elapsed) {
        expect(value >= 0ns, "Default preview clock produced negative elapsed time");
    }
}

void test_focus_pause_does_not_catch_up() {
    std::uint64_t now = 0;
    int renders = 0;
    std::vector<std::chrono::nanoseconds> elapsed;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            switch (renders) {
            case 1:
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 2:
                now = 100;
                push_window_event(SDL_EVENT_WINDOW_FOCUS_LOST);
                break;
            case 3:
                now = 1'000;
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 4:
                now = 1'100;
                push_quit();
                break;
            default:
                break;
            }
            return test_frame();
        },
        {},
        {},
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, false, clock_for(now)) == 0,
           "Focus pause preview did not return successfully");
    expect(renders == 4, "Focus pause preview rendered after its quit event");
    expect(elapsed == std::vector<std::chrono::nanoseconds>{0ns, 100ns},
           "Focus pause accounted paused time or lost the active interval");
}

struct ButtonEvent {
    StartButton button;
    bool pressed;
};

void test_minimize_requires_restore_and_releases_held_buttons() {
    std::uint64_t now = 0;
    int renders = 0;
    std::vector<std::chrono::nanoseconds> elapsed;
    std::vector<ButtonEvent> buttons;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            switch (renders) {
            case 1:
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 2:
                now = 100;
                push_key(SDLK_F1, true);
                push_window_event(SDL_EVENT_WINDOW_MINIMIZED);
                break;
            case 3:
                now = 200;
                // Minimize -> focus lost -> restore must remain paused until
                // focus is gained again.
                push_window_event(SDL_EVENT_WINDOW_FOCUS_LOST);
                break;
            case 4:
                now = 300;
                // This key-up arrives while paused and must not re-press or
                // duplicate the release generated by minimization.
                push_key(SDLK_F1, false);
                push_window_event(SDL_EVENT_WINDOW_RESTORED);
                break;
            case 5:
                now = 400;
                // Restore alone did not resume: focus is still lost.
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 6:
                now = 500;
                push_quit();
                break;
            default:
                break;
            }
            return test_frame();
        },
        [&](StartButton button, bool pressed) { buttons.push_back({button, pressed}); },
        {},
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, false, clock_for(now)) == 0,
           "Minimize pause preview did not return successfully");
    expect(renders == 6, "Minimize pause preview rendered after its quit event");
    expect(elapsed == std::vector<std::chrono::nanoseconds>{0ns, 100ns},
           "Minimize pause resumed before restore or counted paused time");
    expect(buttons.size() == 2 && buttons[0].button == StartButton::Proceed &&
               buttons[0].pressed && buttons[1].button == StartButton::Proceed &&
               !buttons[1].pressed,
           "Minimization did not release the held preview button exactly once");
}

SDL_WindowID foreign_window_id() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    expect(windows != nullptr && count == 1, "Expected exactly one preview window");
    const SDL_WindowID own = SDL_GetWindowID(windows[0]);
    SDL_free(windows);
    return own == std::numeric_limits<SDL_WindowID>::max() ? own - 1 : own + 1;
}

void test_foreign_window_events_are_ignored() {
    std::uint64_t now = 0;
    int renders = 0;
    int steps = 0;
    std::vector<std::chrono::nanoseconds> elapsed;
    std::vector<ButtonEvent> buttons;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 1) {
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
            } else if (renders == 2) {
                now = 100;
                const SDL_WindowID foreign = foreign_window_id();
                push_window_event(SDL_EVENT_WINDOW_FOCUS_LOST, foreign);
                push_window_event(SDL_EVENT_WINDOW_MINIMIZED, foreign);
                push_window_event(SDL_EVENT_WINDOW_RESTORED, foreign);
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED, foreign);
                push_key(SDLK_F1, true, false, foreign);
                push_key(SDLK_SPACE, true, false, foreign);
                push_window_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED, foreign);
            } else if (renders == 3) {
                now = 200;
                push_quit();
            }
            return test_frame();
        },
        [&](StartButton button, bool pressed) { buttons.push_back({button, pressed}); },
        [&] { ++steps; },
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, false, clock_for(now)) == 0,
           "Foreign-event preview did not return successfully");
    expect(renders == 3, "Foreign close request incorrectly stopped the preview");
    expect(elapsed == std::vector<std::chrono::nanoseconds>{0ns, 100ns},
           "Foreign focus/minimize events changed active-time accounting");
    expect(steps == 0 && buttons.empty(), "Foreign-window input reached preview callbacks");
}

void test_terminal_event_stops_the_current_batch(SDL_EventType terminal) {
    std::uint64_t now = 0;
    int renders = 0;
    int steps = 0;
    std::vector<std::chrono::nanoseconds> elapsed;
    std::vector<ButtonEvent> buttons;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 1) {
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
            } else if (renders == 2) {
                now = 100;
                if (terminal == SDL_EVENT_KEY_DOWN) {
                    push_key(SDLK_ESCAPE, true);
                } else {
                    SDL_Event event{};
                    event.type = terminal;
                    if (terminal == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                        event.window.windowID = 0;
                    }
                    push_event(event);
                }
                // These events are in the same SDL batch and must not reach
                // input callbacks after the terminal event.
                push_key(SDLK_F1, true);
                push_key(SDLK_SPACE, true);
            }
            return test_frame();
        },
        [&](StartButton button, bool pressed) { buttons.push_back({button, pressed}); },
        [&] { ++steps; },
        [&](std::chrono::nanoseconds value) { elapsed.push_back(value); },
    };

    expect(river_raid::show_preview(callbacks, false, clock_for(now)) == 0,
           "Terminal preview did not return successfully");
    expect(renders == 2, "Preview rendered after a terminal event");
    expect(elapsed == std::vector<std::chrono::nanoseconds>{0ns},
           "Preview delivered elapsed time after a terminal event");
    expect(steps == 0 && buttons.empty(),
           "Preview delivered queued input after a terminal event");
}

void test_clock_moving_backwards_is_rejected() {
    std::uint64_t now = 100;
    int renders = 0;
    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 2) {
                now = 50;
                push_window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
            }
            return test_frame();
        },
        {},
        {},
        [](std::chrono::nanoseconds) {},
    };

    bool rejected = false;
    try {
        (void)river_raid::show_preview(callbacks, false, clock_for(now));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "Preview accepted a backwards injected clock");
}

void test_clock_is_unused_without_elapsed_callback() {
    int renders = 0;
    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            if (renders == 1) push_quit();
            return test_frame();
        },
        {},
        {},
    };
    const PreviewTiming timing{[]() -> std::uint64_t {
        throw std::runtime_error("untimed preview unexpectedly sampled its clock");
    }};

    expect(river_raid::show_preview(callbacks, false, timing) == 0,
           "Untimed legacy preview did not return successfully");
    expect(renders == 1, "Untimed legacy preview rendered after its quit event");
}

} // namespace

int main() {
    try {
        test_hidden_preview_starts_inactive();
        test_default_clock_smoke();
        test_focus_pause_does_not_catch_up();
        test_minimize_requires_restore_and_releases_held_buttons();
        test_foreign_window_events_are_ignored();
        test_terminal_event_stops_the_current_batch(SDL_EVENT_QUIT);
        test_terminal_event_stops_the_current_batch(SDL_EVENT_KEY_DOWN);
        test_terminal_event_stops_the_current_batch(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
        test_clock_moving_backwards_is_rejected();
        test_clock_is_unused_without_elapsed_callback();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
