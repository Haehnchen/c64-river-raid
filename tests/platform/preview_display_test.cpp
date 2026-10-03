#include "platform/sdl_preview.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

river_raid::Frame frame(int width = 320, int height = 200) {
    return {width, height,
            std::vector<std::uint32_t>(static_cast<std::size_t>(width) *
                                       static_cast<std::size_t>(height), 0xff204060)};
}

SDL_Window* preview_window() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* window = windows && count == 1 ? windows[0] : nullptr;
    SDL_free(windows);
    expect(window != nullptr, "Preview did not create exactly one window");
    expect(std::string_view(SDL_GetWindowTitle(window)) ==
               "River Raid - F11: Fullscreen / Window",
           "Preview title lost its fixed fullscreen hint");
    return window;
}

void expect_window_size(SDL_Window* window, int width, int height) {
    int actual_width = 0;
    int actual_height = 0;
    expect(SDL_GetWindowSize(window, &actual_width, &actual_height),
           "Could not read preview window size");
    expect(actual_width == width && actual_height == height,
           "Preview window size does not match the requested windowed size");
}

void expect_presentation(SDL_Window* window, int width, int height,
                         SDL_FRect expected_rect) {
    SDL_Renderer* renderer = SDL_GetRenderer(window);
    expect(renderer != nullptr, "Preview window has no renderer");
    int logical_width = 0;
    int logical_height = 0;
    SDL_RendererLogicalPresentation mode{};
    expect(SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode),
           "Could not read preview logical presentation");
    expect(logical_width == width && logical_height == height &&
               mode == SDL_LOGICAL_PRESENTATION_LETTERBOX,
           "Preview logical presentation changed the intended display aspect");
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    expect(SDL_GetRenderDrawColor(renderer, &red, &green, &blue, nullptr) &&
               red == 0 && green == 0 && blue == 0,
           "Preview letterbox clear color is not black");
    SDL_FRect rect{};
    expect(SDL_GetRenderLogicalPresentationRect(renderer, &rect),
           "Could not read preview presentation rectangle");
    expect(std::abs(rect.x - expected_rect.x) < 0.01F &&
               std::abs(rect.y - expected_rect.y) < 0.01F &&
               std::abs(rect.w - expected_rect.w) < 0.01F &&
               std::abs(rect.h - expected_rect.h) < 0.01F,
           "Preview image is not centered and letterboxed at the intended aspect");
}

void push_key(SDL_Window* window, SDL_Keycode key, bool down = true,
              bool repeat = false, bool foreign = false) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(window) + (foreign ? 1 : 0);
    event.key.key = key;
    event.key.down = down;
    event.key.repeat = repeat;
    expect(SDL_PushEvent(&event), "Could not queue preview key event");
}

void push_quit() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    expect(SDL_PushEvent(&event), "Could not queue preview quit event");
}

void test_game_presentation() {
    int renders = 0;
    river_raid::PreviewCallbacks callbacks{};
    callbacks.render = [&] {
        ++renders;
        if (renders > 1) {
            auto* window = preview_window();
            switch (renders) {
            case 2:
                expect_window_size(window, 960, 720);
                expect_presentation(window, 320, 240, {0, 0, 960, 720});
                expect(SDL_SetWindowSize(window, 1280, 720), "Could not resize wide preview");
                break;
            case 3:
                expect_presentation(window, 320, 240, {160, 0, 960, 720});
                expect(SDL_SetWindowSize(window, 640, 800), "Could not resize tall preview");
                break;
            case 4:
                expect_presentation(window, 320, 240, {0, 160, 640, 480});
                push_quit();
                break;
            default:
                throw std::runtime_error("Game presentation rendered after quit");
            }
        }
        return frame();
    };
    expect(river_raid::show_preview(callbacks, false) == 0,
           "Game presentation preview failed");
    expect(renders == 4, "Game presentation did not complete its resize checks");
}

void test_diagnostic_presentation() {
    int renders = 0;
    river_raid::PreviewCallbacks callbacks{};
    callbacks.render = [&] {
        ++renders;
        if (renders == 2) {
            auto* window = preview_window();
            expect_window_size(window, 800, 400);
            expect_presentation(window, 400, 200, {0, 0, 800, 400});
            expect(SDL_SetWindowSize(window, 960, 720), "Could not resize diagnostic preview");
        } else if (renders == 3) {
            expect_presentation(preview_window(), 400, 200, {0, 120, 960, 480});
            push_quit();
        } else {
            expect(renders == 1, "Diagnostic presentation rendered after quit");
        }
        return frame(400, 200);
    };
    expect(river_raid::show_preview(callbacks, false) == 0,
           "Diagnostic presentation preview failed");
    expect(renders == 3, "Diagnostic presentation did not complete its resize checks");
}

void expect_fullscreen(SDL_Window* window, bool enabled, const SDL_DisplayMode& desktop) {
    expect(SDL_SyncWindow(window), "Could not synchronize preview window state");
    expect(((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0) == enabled,
           "F11 produced an unexpected fullscreen state");
    expect(SDL_GetWindowFullscreenMode(window) == nullptr,
           "Preview selected an exclusive fullscreen display mode");
    const auto* current = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    expect(current && current->w == desktop.w && current->h == desktop.h &&
               current->format == desktop.format && current->refresh_rate == desktop.refresh_rate,
           "Preview fullscreen changed the desktop display mode");
    if (enabled) {
        int output_width = 0;
        int output_height = 0;
        expect(SDL_GetRenderOutputSize(SDL_GetRenderer(window), &output_width, &output_height),
               "Could not read fullscreen output dimensions");
        const float width = static_cast<float>(output_width);
        const float height = static_cast<float>(output_height);
        const float scale = std::fmin(width / 320.0F, height / 240.0F);
        expect_presentation(window, 320, 240,
                            {(width - 320.0F * scale) / 2.0F,
                             (height - 240.0F * scale) / 2.0F,
                             320.0F * scale, 240.0F * scale});
    }
}

void test_fullscreen_controls() {
    int renders = 0;
    SDL_DisplayMode desktop{};
    river_raid::PreviewCallbacks callbacks{};
    callbacks.render = [&] {
        ++renders;
        if (renders == 1) return frame();
        auto* window = preview_window();
        if (renders == 2) {
            const auto* current = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
            expect(current != nullptr, "Could not read initial desktop display mode");
            desktop = *current;
        }
        expect_fullscreen(window, (renders >= 6 && renders <= 9) || renders == 12, desktop);
        switch (renders) {
        case 2:
            expect(SDL_SetWindowSize(window, 800, 600), "Could not set restored window size");
            push_key(window, SDLK_F11, true, false, true);
            break;
        case 3:
            push_key(window, SDLK_F11, true, true);
            break;
        case 4:
            push_key(window, SDLK_F11, false);
            break;
        case 5: {
            // Window controls remain available while the game is paused by lost focus.
            SDL_Event focus_lost{};
            focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            focus_lost.window.windowID = SDL_GetWindowID(window);
            expect(SDL_PushEvent(&focus_lost), "Could not queue preview focus loss");
            push_key(window, SDLK_F11);
            break;
        }
        case 6:
            push_key(window, SDLK_F11, true, true);
            break;
        case 7:
            push_key(window, SDLK_F11, true, false, true);
            break;
        case 8:
            push_key(window, SDLK_F11, false);
            break;
        case 9:
            push_key(window, SDLK_F11);
            break;
        case 10:
            expect_window_size(window, 800, 600);
            push_key(window, SDLK_F11);
            push_key(window, SDLK_F11);
            break;
        case 11:
            expect_window_size(window, 800, 600);
            push_key(window, SDLK_F11);
            break;
        case 12:
            push_key(window, SDLK_ESCAPE);
            break;
        default:
            throw std::runtime_error("Escape did not quit the fullscreen preview");
        }
        return frame();
    };
    callbacks.elapsed = [](auto) {};
    callbacks.button = [](auto, bool) {
        throw std::runtime_error("Window control reached the game's button handler");
    };
    callbacks.step = [] {
        throw std::runtime_error("Window control advanced the diagnostic scene");
    };
    callbacks.flight_controls = [](auto) {
        throw std::runtime_error("Window control changed the flight controls");
    };
    expect(river_raid::show_preview(callbacks, false) == 0, "Fullscreen preview failed");
    expect(renders == 12, "Fullscreen preview did not complete its controls checks");
}
} // namespace

int main() {
    try {
        test_game_presentation();
        test_diagnostic_presentation();
        test_fullscreen_controls();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
