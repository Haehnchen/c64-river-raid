#include "platform/sdl_preview.hpp"

#include "platform/active_time.hpp"
#include "platform/sdl_audio.hpp"
#include "platform/sdl_joysticks.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <optional>
#include <stdexcept>

namespace river_raid {
namespace {
constexpr char window_title[] = "River Raid - F11: Fullscreen / Window";

void require(bool success) {
    if (!success) {
        throw std::runtime_error(SDL_GetError());
    }
}

bool valid_preview_frame(const Frame& frame) noexcept {
    if (frame.width <= 0 || frame.width > 1024 ||
        frame.height <= 0 || frame.height > 1024) {
        return false;
    }
    const auto area = static_cast<std::size_t>(frame.width) *
                      static_cast<std::size_t>(frame.height);
    return frame.pixels.size() == area;
}

struct Session {
    Session(bool audio, bool joysticks) {
        const auto flags = static_cast<SDL_InitFlags>(
            SDL_INIT_VIDEO | (audio ? SDL_INIT_AUDIO : 0U) |
            (joysticks ? SDL_INIT_JOYSTICK : 0U));
        require(SDL_Init(flags));
    }
    ~Session() { SDL_Quit(); }
};
} // namespace

bool dispatch_preview_button(const PreviewCallbacks& callbacks, StartButton button,
                             bool pressed) {
    if (!callbacks.button) return false;
    if (!pressed || !callbacks.audio_session_generation) {
        callbacks.button(button, pressed);
        return false;
    }
    const auto before = callbacks.audio_session_generation();
    callbacks.button(button, pressed);
    return callbacks.audio_session_generation() != before;
}

int show_preview(const Frame& frame, bool smoke_test) {
    return show_preview(PreviewCallbacks{[&] { return frame; }, {}, {}}, smoke_test);
}

int show_preview(const PreviewCallbacks& callbacks, bool smoke_test, PreviewTiming timing) {
    if (!callbacks.render) {
        throw std::invalid_argument("Preview needs a rendering callback");
    }
    Session session(static_cast<bool>(callbacks.take_audio),
                    static_cast<bool>(callbacks.joystick_controls));
    std::optional<SdlJoysticks> joysticks;
    if (callbacks.joystick_controls) joysticks.emplace();
    auto frame = callbacks.render();
    const int width = frame.width;
    const int height = frame.height;
    if (!valid_preview_frame(frame)) {
        throw std::invalid_argument("Invalid initial preview frame");
    }
    std::optional<SdlAudioDevice> audio;
    if (callbacks.take_audio) audio.emplace();
    SDL_Window* raw_window{};
    SDL_Renderer* raw_renderer{};
    const SDL_WindowFlags flags = smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
    const bool game_frame = width == 320 && height == 200;
    // C64 game pixels fill a 4:3 display; diagnostic sheets retain their own aspect.
    const int logical_height = game_frame ? 240 : height;
    const int scale = game_frame ? 3 : (width <= 320 && height <= 200 ? 4 : 2);
    require(SDL_CreateWindowAndRenderer(window_title, width * scale,
                                       logical_height * scale, flags, &raw_window, &raw_renderer));
    const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window, SDL_DestroyWindow);
    const std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw_renderer, SDL_DestroyRenderer);
    const std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
        SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                          frame.width, frame.height), SDL_DestroyTexture);
    require(texture != nullptr);
    require(SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST));
    require(SDL_SetRenderLogicalPresentation(renderer.get(), width, logical_height,
                                            SDL_LOGICAL_PRESENTATION_LETTERBOX));
    const SDL_WindowID window_id = SDL_GetWindowID(window.get());
    require(window_id != 0);
    if (!timing.now_ns) timing.now_ns = SDL_GetTicksNS;
    const auto window_flags = SDL_GetWindowFlags(window.get());
    bool focused = (window_flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    bool minimized = (window_flags & SDL_WINDOW_MINIMIZED) != 0;
    std::optional<ActiveTime> active_time;
    if (callbacks.elapsed || callbacks.take_audio) {
        active_time.emplace(timing.now_ns(), focused && !minimized);
    }
    const auto relevant_window = [window_id](SDL_WindowID event_window_id) {
        return event_window_id == 0 || event_window_id == window_id;
    };
    const auto set_active_gate = [&] {
        const bool active = focused && !minimized;
        if (active_time) {
            active_time->set_active(timing.now_ns(), active);
        }
        if (audio) audio->set_active(active);
    };
    if (audio) audio->set_active(focused && !minimized);
    bool proceed_held = false;
    bool next_option_held = false;
    FlightControls flight_controls{};
    JoystickControls joystick_controls{};
    const auto release_buttons = [&](bool force) {
        if (callbacks.button && (force || proceed_held)) {
            callbacks.button(StartButton::Proceed, false);
        }
        if (callbacks.button && (force || next_option_held)) {
            callbacks.button(StartButton::NextOption, false);
        }
        proceed_held = false;
        next_option_held = false;
        if (callbacks.flight_controls && (force || flight_controls != FlightControls{})) {
            flight_controls = {};
            callbacks.flight_controls(flight_controls);
        }
        if (callbacks.joystick_controls && (force || joystick_controls != JoystickControls{})) {
            joystick_controls = {};
            callbacks.joystick_controls(joystick_controls);
        }
    };
    bool running = true;
    // Track the requested state because desktop transitions may be asynchronous.
    bool fullscreen = false;
    int frames = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const bool relevant_key = (event.type == SDL_EVENT_KEY_DOWN ||
                                       event.type == SDL_EVENT_KEY_UP) &&
                                      relevant_window(event.key.windowID);
            const bool relevant_close = event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                        relevant_window(event.window.windowID);
            if (event.type == SDL_EVENT_QUIT || relevant_close ||
                (relevant_key && event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE)) {
                running = false;
                break;
            }
            if (relevant_key && event.key.key == SDLK_F11) {
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    // A null mode keeps the desktop resolution in borderless fullscreen.
                    require(SDL_SetWindowFullscreenMode(window.get(), nullptr));
                    require(SDL_SetWindowFullscreen(window.get(), !fullscreen));
                    fullscreen = !fullscreen;
                }
                continue;
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST &&
                relevant_window(event.window.windowID)) {
                if (focused) {
                    focused = false;
                    set_active_gate();
                }
                release_buttons(!active_time);
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED &&
                relevant_window(event.window.windowID) && !focused) {
                focused = true;
                set_active_gate();
            }
            if (event.type == SDL_EVENT_WINDOW_MINIMIZED &&
                relevant_window(event.window.windowID) && !minimized) {
                minimized = true;
                set_active_gate();
                if (active_time) release_buttons(false);
            }
            if (event.type == SDL_EVENT_WINDOW_RESTORED &&
                relevant_window(event.window.windowID) && minimized) {
                minimized = false;
                set_active_gate();
            }
            if (relevant_key) {
                const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
                if (pressed && event.key.repeat) continue;
                if (active_time && !active_time->active()) continue;
                if (callbacks.button && event.key.key == SDLK_F1) {
                    if (dispatch_preview_button(callbacks, StartButton::Proceed, pressed) && audio)
                        audio->clear();
                    proceed_held = pressed;
                }
                if (callbacks.button && event.key.key == SDLK_F3) {
                    if (dispatch_preview_button(callbacks, StartButton::NextOption, pressed) && audio)
                        audio->clear();
                    next_option_held = pressed;
                }
                if (callbacks.step && pressed && event.key.key == SDLK_SPACE) {
                    callbacks.step();
                }
                if (callbacks.flight_controls) {
                    auto next = flight_controls;
                    if (event.key.key == SDLK_UP) next.up = pressed;
                    if (event.key.key == SDLK_DOWN) next.down = pressed;
                    if (event.key.key == SDLK_LEFT) next.left = pressed;
                    if (event.key.key == SDLK_RIGHT) next.right = pressed;
                    if (event.key.key == SDLK_SPACE) next.fire = pressed;
                    if (next != flight_controls) {
                        flight_controls = next;
                        callbacks.flight_controls(flight_controls);
                    }
                }
            }
        }
        if (!running) break;
        if (joysticks) {
            joysticks->refresh_devices();
            const auto next = focused && !minimized ? joysticks->sample() : JoystickControls{};
            if (next != joystick_controls) {
                joystick_controls = next;
                callbacks.joystick_controls(joystick_controls);
            }
        }
        if (active_time && active_time->active()) {
            if (callbacks.elapsed) callbacks.elapsed(active_time->take_elapsed(timing.now_ns()));
            else (void)active_time->take_elapsed(timing.now_ns());
            if (audio) audio->submit(callbacks.take_audio());
        }
        frame = callbacks.render();
        if (frame.width != width || frame.height != height || !valid_preview_frame(frame)) {
            throw std::invalid_argument("Preview frame size changed or is invalid");
        }
        require(SDL_UpdateTexture(texture.get(), nullptr, frame.pixels.data(),
                                  frame.width * static_cast<int>(sizeof(std::uint32_t))));
        require(SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255));
        require(SDL_RenderClear(renderer.get()));
        require(SDL_RenderTexture(renderer.get(), texture.get(), nullptr, nullptr));
        require(SDL_RenderPresent(renderer.get()));
        if (smoke_test && ++frames == 3) {
            break;
        }
        SDL_Delay(16);
    }
    return 0;
}

} // namespace river_raid
