#include "platform/sdl_joysticks.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

SDL_malloc_func original_malloc{};
SDL_calloc_func original_calloc{};
SDL_realloc_func original_realloc{};
SDL_free_func original_free{};
std::atomic<bool> fail_next_allocation{};

void* SDLCALL test_malloc(std::size_t size) {
    if (fail_next_allocation.exchange(false)) return nullptr;
    return original_malloc(size);
}

void* SDLCALL test_calloc(std::size_t count, std::size_t size) {
    if (fail_next_allocation.exchange(false)) return nullptr;
    return original_calloc(count, size);
}

void* SDLCALL test_realloc(void* memory, std::size_t size) {
    if (fail_next_allocation.exchange(false)) return nullptr;
    return original_realloc(memory, size);
}

void SDLCALL test_free(void* memory) { original_free(memory); }

void install_allocation_probe() {
    SDL_GetOriginalMemoryFunctions(&original_malloc, &original_calloc,
                                   &original_realloc, &original_free);
    require(SDL_SetMemoryFunctions(test_malloc, test_calloc, test_realloc, test_free),
            "Could not install SDL allocation probe");
}

struct SdlSession {
    SdlSession() {
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "0xffff/0x0001");
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~SdlSession() { SDL_Quit(); }
};

class VirtualJoystick {
public:
    VirtualJoystick() {
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        desc.vendor_id = 0xffff;
        desc.product_id = 0x0001;
        desc.naxes = 2;
        desc.nbuttons = 1;
        desc.nhats = 1;
        desc.name = "River Raid virtual raw joystick";
        id_ = SDL_AttachVirtualJoystick(&desc);
        if (id_ == 0) throw std::runtime_error(SDL_GetError());
        handle_ = SDL_OpenJoystick(id_);
        if (handle_ == nullptr) {
            SDL_DetachVirtualJoystick(id_);
            id_ = 0;
            throw std::runtime_error(SDL_GetError());
        }
        SDL_UpdateJoysticks();
    }

    ~VirtualJoystick() { detach(); }
    VirtualJoystick(const VirtualJoystick&) = delete;
    VirtualJoystick& operator=(const VirtualJoystick&) = delete;

    void axis(int index, Sint16 value) {
        require(SDL_SetJoystickVirtualAxis(handle_, index, value), SDL_GetError());
    }
    void button(int index, bool down) {
        require(SDL_SetJoystickVirtualButton(handle_, index, down), SDL_GetError());
    }
    void hat(int index, Uint8 value) {
        require(SDL_SetJoystickVirtualHat(handle_, index, value), SDL_GetError());
    }

    void detach() noexcept {
        if (handle_ != nullptr) {
            SDL_CloseJoystick(handle_);
            handle_ = nullptr;
        }
        if (id_ != 0) {
            SDL_DetachVirtualJoystick(id_);
            id_ = 0;
        }
    }

private:
    SDL_JoystickID id_{};
    SDL_Joystick* handle_{};
};

void test_slots_sampling_and_hotplug() {
    using namespace river_raid;

    int existing_count = 0;
    SDL_JoystickID* existing = SDL_GetJoysticks(&existing_count);
    require(existing != nullptr, "SDL joystick enumeration failed");
    SDL_free(existing);
    require(existing_count == 0,
            "SDL joystick test requires an isolated dummy joystick backend");

    VirtualJoystick first;
    VirtualJoystick second;
    SdlJoysticks devices;
    devices.refresh_devices();
    require(devices.sample() == JoystickControls{},
            "Newly attached virtual joysticks should start neutral");

    first.axis(0, 15'999);
    first.axis(1, 16'000);
    first.hat(0, SDL_HAT_LEFTUP);
    first.button(0, true);
    second.axis(0, -16'000);
    second.button(0, true);

    auto sampled = devices.sample();
    require(sampled[0] == FlightControls{true, true, true, false, true},
            "Axis threshold, hat OR, or fire mapping differs on slot zero");
    require(sampled[1] == FlightControls{false, false, true, false, true},
            "Second attached joystick did not map to slot one");

    first.hat(0, SDL_HAT_CENTERED);
    first.axis(0, 15'999);
    first.axis(1, 0);
    first.button(0, false);
    sampled = devices.sample();
    require(sampled[0] == FlightControls{}, "Axis below deadzone was not neutral");
    first.axis(0, 16'000);
    require(devices.sample()[0].right,
            "Positive axis did not activate at the inclusive deadzone boundary");
    first.axis(0, -16'000);
    first.hat(0, SDL_HAT_RIGHT);
    sampled = devices.sample();
    require(sampled[0].left && sampled[0].right,
            "Hat and axis directions were not combined");

    VirtualJoystick third;
    devices.refresh_devices();
    sampled = devices.sample();
    require(sampled[0].left && sampled[0].right && sampled[1].fire,
            "A third device displaced an already assigned slot");

    first.detach();
    sampled = devices.sample();
    require(sampled[0] == FlightControls{} && sampled[1].fire,
            "Disconnect did not release its controls or moved the other slot");

    devices.refresh_devices();
    third.axis(1, -16'000);
    third.button(0, true);
    sampled = devices.sample();
    require(sampled[0] == FlightControls{true, false, false, false, true} &&
                sampled[1].fire,
            "A queued third device did not fill the vacant slot");

    second.detach();
    devices.refresh_devices();
    sampled = devices.sample();
    require(sampled[0].up && sampled[0].fire && sampled[1] == FlightControls{},
            "Disconnect left stale controls or compacted the remaining slot");

    fail_next_allocation = true;
    bool reported_enumeration_error = false;
    try {
        devices.refresh_devices();
    } catch (const std::runtime_error& error) {
        reported_enumeration_error =
            std::string_view(error.what()).starts_with("SDL_GetJoysticks:");
    }
    fail_next_allocation = false;
    require(reported_enumeration_error,
            "SDL joystick enumeration failure was silently ignored");
}

} // namespace

int main() {
    try {
        install_allocation_probe();
        SdlSession session;
        test_slots_sampling_and_hotplug();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
