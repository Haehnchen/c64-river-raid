#include "app/flight_start.hpp"
#include "app/flight_scene.hpp"
#include "app/player_overlay.hpp"
#include "app/shot_overlay.hpp"
#include "app/world_scene_setup.hpp"
#include "platform/sdl_preview.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void key(SDL_Keycode code, bool pressed, bool repeat = false, SDL_WindowID window = 0) {
    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = code;
    event.key.repeat = repeat;
    event.key.windowID = window;
    require(SDL_PushEvent(&event), "Key event failed");
}
void window_event(SDL_EventType type) {
    SDL_Event event{};
    event.type = type;
    require(SDL_PushEvent(&event), "Window event failed");
}
}

int main() {
    using namespace river_raid;
    try {
        constexpr std::array options{StartOption{"1"}, StartOption{"2"}};
        StartFlow flow(options);
        FlightPreview flight;
        FlightStart launch(flow, flight);
        FlightControls controls{};
        PlayerSteeringState initial{}, paused{};
        PlayerShotState paused_shot{};
        std::optional<FlightPreview> expected_resume;
        std::size_t initial_rows = 0, paused_rows = 0;
        unsigned starts = 0, renders = 0;
        std::uint64_t now = 0;
        bool start_sent = false;
        const PreviewCallbacks callbacks{
            [&] {
                ++renders;
                if (renders == 3 && !start_sent && flight.status() != FlightStatus::Attract) {
                    --renders;
                    now += 100'000'000ULL;
                    return make_flight_scene(flight);
                }
                if (start_sent && flight.life_cycle().phase != LifeCyclePhase::Active) {
                    --renders;
                    now += 100'000'000ULL;
                    return make_flight_scene(flight);
                }
                if (renders == 1) {
                    window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                    key(SDLK_F1, true);
                } else if (renders == 2) {
                    require(!flight.running(), "Title key hold started flight");
                    key(SDLK_F1, false);
                } else if (renders == 3) {
                    require(flow.stage() == StartStage::Options &&
                                flight.status() == FlightStatus::Attract && starts == 0 &&
                                flight.idle_state().phase == SessionIdlePhase::Attract,
                            "Title exit did not wait in options and enter attract");
                    key(SDLK_F3, true);
                    key(SDLK_F3, false);
                    key(SDLK_F1, true);
                    key(SDLK_RIGHT, true);
                    key(SDLK_UP, true);
                    key(SDLK_SPACE, true);
                    key(SDLK_SPACE, true, true);
                    key(SDLK_SPACE, false, false, 0x7FFFFFFFU);
                    start_sent = true;
                } else if (renders == 4) {
                    require(flight.running() && starts == 1, "F1 did not start exactly once");
                    require(flow.selected_option_index() == 1 && flight.selected_option() == 1,
                            "SDL F3 selection did not reach the launched flight");
                    require(controls.right && controls.up && !controls.down, "Arrow mapping differs");
                    require(controls.fire && flight.shot().pose == PlayerShotPose::Visible,
                            "Held Space did not fire");
                    key(SDLK_F1, false);
                    key(SDLK_F3, true);
                    key(SDLK_F3, false);
                    key(SDLK_LEFT, true);
                } else if (renders == 5) {
                    require(flow.stage() == StartStage::StartRequested && starts == 1 &&
                                flow.selected_option_index() == 1,
                            "Active-flight F3 changed the option flow");
                    require(controls.left && controls.right, "Opposing arrows were collapsed");
                    require(flight.pose() == FlightPose::Right, "Right priority lost");
                    require(flight.steering().horizontal_position != initial.horizontal_position,
                            "Held arrow did not move aircraft");
                    require(flight.world().generated_rows() > initial_rows, "Flight did not scroll");
                    key(SDLK_RIGHT, false);
                } else if (renders == 6) {
                    require(flight.pose() == FlightPose::Left, "Released right still held");
                    window_event(SDL_EVENT_WINDOW_FOCUS_LOST);
                } else if (renders == 7) {
                    require(controls == FlightControls{}, "Focus loss left arrows held");
                    paused = flight.steering();
                    paused_shot = flight.shot().state;
                    paused_rows = flight.world().generated_rows();
                    expected_resume = flight;
                    // ActiveTime retains the active tail before focus loss, not the pause.
                    expected_resume->advance_elapsed(std::chrono::milliseconds{100});
                    now += 5'000'000'000ULL;
                } else if (renders == 8) {
                    require(flight.steering() == paused && flight.world().generated_rows() == paused_rows &&
                            flight.shot().state == paused_shot,
                            "Unfocused flight advanced");
                    window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                } else if (renders == 9) {
                    require(flight.steering() == expected_resume->steering() &&
                            flight.world().generated_rows() == expected_resume->world().generated_rows() &&
                            flight.shot().state == expected_resume->shot().state,
                            "Focus restoration replayed inactive time");
                    key(SDLK_F1, true);
                } else if (renders == 10) {
                    require(starts == 1 && flight.life_cycle().phase == LifeCyclePhase::Active &&
                                flight.world().generated_rows() > initial_rows &&
                                flight.steering().horizontal_position != initial.horizontal_position,
                            "Active-flight F1 reset or stopped the current session");
                    key(SDLK_F1, false);
                    window_event(SDL_EVENT_QUIT);
                }
                now += 100'000'000ULL;
                if (flight.status() == FlightStatus::Ready) {
                    return Frame{320, 200, std::vector<std::uint32_t>(64000, 0xff000000)};
                }
                return make_flight_scene(flight);
            },
            [&](auto button, bool pressed) {
                const auto previous_stage = flow.stage();
                const auto previous_option = flow.selected_option_index();
                launch.set_button(button, pressed);
                if (button == StartButton::NextOption && pressed &&
                    previous_stage == StartStage::Options) {
                    require(flow.selected_option_index() == (previous_option + 1) % options.size(),
                            "SDL F3 did not advance the options selection");
                }
                if (button == StartButton::Proceed && pressed &&
                    previous_stage != StartStage::StartRequested &&
                    flow.stage() == StartStage::StartRequested) {
                    ++starts;
                    initial = flight.steering();
                    initial_rows = flight.world().generated_rows();
                }
            }, {},
            [&](auto elapsed) { launch.advance_elapsed(elapsed); },
            [&](auto value) { controls = value; launch.set_controls(value); },
        };
        require(show_preview(callbacks, false, {[&] { return now; }}) == 0, "Preview failed");
        require(renders == 10, "Incomplete integration sequence");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
