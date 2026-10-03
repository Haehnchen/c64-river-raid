#include "platform/sdl_preview.hpp"
#include "platform/sdl_audio.hpp"

#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace std::chrono_literals;
using river_raid::Frame;
using river_raid::PreviewCallbacks;
using river_raid::PreviewTiming;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class AudioSession {
public:
    AudioSession() {
        if (!SDL_Init(SDL_INIT_AUDIO)) throw std::runtime_error(SDL_GetError());
    }
    ~AudioSession() { SDL_Quit(); }
};

void push_window(SDL_EventType type) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = 0;
    require(SDL_PushEvent(&event), "Could not push preview window event");
}

Frame test_frame() {
    return Frame{2, 2, std::vector<std::uint32_t>(4, 0xff102030)};
}

void test_audio_gate_tracks_focus_and_minimize() {
    std::uint64_t now = 0;
    int renders = 0;
    int elapsed_calls = 0;
    int audio_calls = 0;

    const PreviewCallbacks callbacks{
        [&] {
            ++renders;
            switch (renders) {
            case 1:
                push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 2:
                now = 100;
                push_window(SDL_EVENT_WINDOW_MINIMIZED);
                break;
            case 3:
                now = 200;
                push_window(SDL_EVENT_WINDOW_RESTORED);
                break;
            case 4:
                now = 300;
                push_window(SDL_EVENT_WINDOW_FOCUS_LOST);
                break;
            case 5:
                now = 400;
                push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);
                break;
            case 6:
                now = 500;
                push_window(SDL_EVENT_QUIT);
                break;
            default:
                break;
            }
            return test_frame();
        },
        {},
        {},
        [&](std::chrono::nanoseconds) { ++elapsed_calls; },
        {},
        [&] {
            ++audio_calls;
            return std::vector<float>(480, 0.0f);
        },
    };

    const PreviewTiming timing{[&now] { return now; }};
    require(river_raid::show_preview(callbacks, false, timing) == 0,
            "Preview audio gate did not return successfully");
    require(renders == 6 && elapsed_calls == 3 && audio_calls == 3,
            "Audio callback ran while focus/minimize gate was inactive");
}

void test_start_generation_gates_audio_clear() {
    using namespace river_raid;
    AudioSession session;
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);

    PreviewCallbacks callbacks;
    callbacks.button = [&](StartButton button, bool pressed) {
        launch.set_button(button, pressed);
    };
    callbacks.audio_session_generation = [&] { return launch.start_generation(); };

    SdlAudioDevice audio;
    audio.set_active(true);
    const std::vector<float> samples(4800, 0.125f);
    const auto queue_pcm = [&] {
        audio.clear();
        audio.submit(samples);
        require(audio.queued_bytes() > 0, "Dummy audio queue did not accept PCM");
    };
    int clear_count = 0;
    const auto dispatch = [&](StartButton button, bool pressed) {
        // This is the same semantic decision used by show_preview before clearing its device.
        const bool reset_audio = dispatch_preview_button(callbacks, button, pressed);
        if (reset_audio) {
            ++clear_count;
            audio.clear();
        }
        return reset_audio;
    };

    require(launch.start_generation() == 0, "Initial start generation was not zero");
    queue_pcm();
    require(!dispatch(StartButton::Proceed, true), "Title F1 cleared the session audio");
    require(!dispatch(StartButton::Proceed, false), "Title F1 release cleared audio");
    require(flow.stage() == StartStage::Options && clear_count == 0 &&
                launch.start_generation() == 0,
            "Title exit was treated as a started flight session");

    queue_pcm();
    require(!dispatch(StartButton::NextOption, true), "Options F3 cleared session audio");
    require(flow.selected_option_index() == 1, "Options F3 did not select the next option");
    require(!dispatch(StartButton::Proceed, true), "F1 passed the held-F3 input gate");
    require(!dispatch(StartButton::NextOption, false), "F3 release cleared session audio");
    require(!dispatch(StartButton::Proceed, false), "Ignored F1 release cleared session audio");
    require(flow.stage() == StartStage::Options && clear_count == 0,
            "Shared F1/F3 hold gate did not remain in options");

    queue_pcm();
    require(dispatch(StartButton::Proceed, true), "Accepted options F1 did not reset audio");
    require(launch.start_generation() == 1 && clear_count == 1 && audio.queued_bytes() == 0,
            "Accepted options F1 did not clear the queued session audio exactly once");
    require(!dispatch(StartButton::Proceed, true), "Held F1 repeated an accepted start");
    require(!dispatch(StartButton::NextOption, true), "F3 passed the held-F1 input gate");
    require(launch.start_generation() == 1 && clear_count == 1,
            "Held F1/F3 generated a duplicate audio reset");
    require(!dispatch(StartButton::Proceed, false), "F1 release cleared session audio");
    require(!dispatch(StartButton::NextOption, false), "F3 release cleared session audio");

    test::wait_for_launch(flight);
    queue_pcm();
    require(dispatch(StartButton::Proceed, true), "Accepted pre-launch restart did not reset audio");
    require(launch.start_generation() == 2 && clear_count == 2 && audio.queued_bytes() == 0,
            "Accepted restart did not clear the queued session audio exactly once");
    require(!dispatch(StartButton::Proceed, false), "Restart F1 release cleared session audio");

    test::wait_for_launch(flight);
    launch.set_controls(FlightControls{.right = true});
    for (int tick = 0; tick < 8 && flight.life_cycle().phase == LifeCyclePhase::AwaitInput; ++tick) {
        (void)flight.advance_elapsed(std::chrono::milliseconds{20});
    }
    require(flight.life_cycle().phase == LifeCyclePhase::Active,
            "Flight did not enter active control mode for the ignored-F1 check");
    queue_pcm();
    require(!dispatch(StartButton::Proceed, true), "Active-flight F1 reset the session");
    require(!dispatch(StartButton::NextOption, true), "Active-flight F3 reset the session");
    require(launch.start_generation() == 2 && clear_count == 2,
            "Ignored active-flight F1/F3 cleared or restarted audio");
    require(!dispatch(StartButton::Proceed, false), "Active F1 release cleared session audio");
    require(!dispatch(StartButton::NextOption, false), "Active F3 release cleared session audio");

    PreviewCallbacks without_generation;
    int forwarded = 0;
    without_generation.button = [&](StartButton, bool) { ++forwarded; };
    require(!dispatch_preview_button(without_generation, StartButton::Proceed, true),
            "Callback without a generation getter requested an audio reset");
    require(forwarded == 1, "Callback without a generation getter was not forwarded");
}

} // namespace

int main() {
    try {
        test_start_generation_gates_audio_clear();
        test_audio_gate_tracks_focus_and_minimize();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
