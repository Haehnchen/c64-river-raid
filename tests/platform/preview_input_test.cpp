#include "platform/sdl_preview.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("Preview input test failed");
}
void key(SDL_Keycode code, bool pressed, bool repeat = false) {
    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = code;
    event.key.repeat = repeat;
    require(SDL_PushEvent(&event));
}
}

int main() {
    try {
        constexpr std::array options{river_raid::StartOption{"A"}, river_raid::StartOption{"B"}};
        river_raid::StartFlow flow(options);
        int renders = 0;
        int steps = 0;
        const river_raid::PreviewCallbacks callbacks{
            [&] {
                ++renders;
                if (renders == 1) key(SDLK_F1, true);
                if (renders == 2) {
                    require(flow.stage() == river_raid::StartStage::Title);
                    key(SDLK_F1, false);
                    key(SDLK_F3, true);
                    key(SDLK_F3, false);
                    key(SDLK_F3, true, true);
                    key(SDLK_SPACE, true);
                    key(SDLK_SPACE, true, true);
                }
                if (renders == 3) {
                    require(flow.stage() == river_raid::StartStage::Options);
                    require(flow.selected_option_index() == 1 && steps == 1);
                    key(SDLK_F1, true);
                }
                if (renders == 4) require(flow.stage() == river_raid::StartStage::StartRequested);
                return river_raid::Frame{8, 8, std::vector<std::uint32_t>(64, 0xff000000)};
            },
            [&](auto button, bool pressed) { flow.set_button(button, pressed); },
            [&] { ++steps; },
        };
        require(river_raid::show_preview(callbacks, true) == 0);
        require(renders == 4);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
