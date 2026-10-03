#include "platform/sdl_preview.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void event(SDL_EventType type) {
    SDL_Event value{};
    value.type = type;
    require(SDL_PushEvent(&value), "Could not queue preview event");
}
}

int main() {
    using namespace river_raid;
    try {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "0xffff/0x0001");
        JoystickControls controls{};
        SDL_Joystick* device = nullptr;
        SDL_JoystickID id = 0;
        unsigned renders = 0;
        std::uint64_t now = 0;
        const PreviewCallbacks callbacks{
            [&] {
                ++renders;
                if (renders == 1) {
                    SDL_VirtualJoystickDesc descriptor{};
                    SDL_INIT_INTERFACE(&descriptor);
                    descriptor.type = SDL_JOYSTICK_TYPE_FLIGHT_STICK;
                    descriptor.vendor_id = 0xffff;
                    descriptor.product_id = 0x0001;
                    descriptor.naxes = 2;
                    descriptor.nbuttons = 1;
                    descriptor.name = "River Raid preview input test";
                    id = SDL_AttachVirtualJoystick(&descriptor);
                    require(id != 0, "Virtual joystick attachment failed");
                    device = SDL_OpenJoystick(id);
                    require(device != nullptr, "Virtual joystick open failed");
                    require(SDL_SetJoystickVirtualButton(device, 0, true), "Virtual fire failed");
                    SDL_UpdateJoysticks();
                    event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                } else if (renders == 2) {
                    require(controls[0].fire && controls[1] == FlightControls{},
                            "Preview did not publish the first joystick port");
                    event(SDL_EVENT_WINDOW_FOCUS_LOST);
                } else if (renders == 3) {
                    require(controls == JoystickControls{}, "Focus loss retained joystick input");
                    event(SDL_EVENT_WINDOW_FOCUS_GAINED);
                } else if (renders == 4) {
                    require(controls[0].fire, "Focus restoration did not resample held joystick");
                    SDL_CloseJoystick(device);
                    device = nullptr;
                    require(SDL_DetachVirtualJoystick(id), "Virtual disconnect failed");
                } else if (renders == 5) {
                    require(controls == JoystickControls{}, "Disconnect left fire held");
                    event(SDL_EVENT_QUIT);
                }
                now += 20'000'000;
                return Frame{320, 200, std::vector<std::uint32_t>(64000, 0xff000000)};
            }, {}, {},
            [](auto) {}, {}, {}, [&](auto ports) { controls = ports; }};
        require(show_preview(callbacks, false, {[&] { return now; }}) == 0,
                "Joystick preview failed");
        require(renders == 5, "Joystick preview sequence incomplete");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
