#include "platform/sdl_joysticks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace river_raid {
namespace {

constexpr Sint16 kAxisDeadzone = 16'000;

bool contains_id(const SDL_JoystickID* ids, int count, SDL_JoystickID id) noexcept {
    if (ids == nullptr || count <= 0) return false;
    return std::find(ids, ids + count, id) != ids + count;
}

FlightControls read_controls(SDL_Joystick* joystick) noexcept {
    FlightControls controls{};
    if (joystick == nullptr || !SDL_JoystickConnected(joystick)) return controls;

    const int axes = SDL_GetNumJoystickAxes(joystick);
    if (axes > 0) {
        const Sint16 x = SDL_GetJoystickAxis(joystick, 0);
        controls.left = x <= -kAxisDeadzone;
        controls.right = x >= kAxisDeadzone;
    }
    if (axes > 1) {
        const Sint16 y = SDL_GetJoystickAxis(joystick, 1);
        controls.up = y <= -kAxisDeadzone;
        controls.down = y >= kAxisDeadzone;
    }

    if (SDL_GetNumJoystickHats(joystick) > 0) {
        const Uint8 hat = SDL_GetJoystickHat(joystick, 0);
        controls.up = controls.up || (hat & SDL_HAT_UP) != 0;
        controls.down = controls.down || (hat & SDL_HAT_DOWN) != 0;
        controls.left = controls.left || (hat & SDL_HAT_LEFT) != 0;
        controls.right = controls.right || (hat & SDL_HAT_RIGHT) != 0;
    }

    if (SDL_GetNumJoystickButtons(joystick) > 0) {
        controls.fire = SDL_GetJoystickButton(joystick, 0);
    }
    return controls;
}

} // namespace

SdlJoysticks::~SdlJoysticks() {
    for (auto& slot : slots_) close_slot(slot);
}

void SdlJoysticks::close_slot(Slot& slot) noexcept {
    if (slot.handle != nullptr) SDL_CloseJoystick(slot.handle);
    slot = {};
}

void SdlJoysticks::refresh_devices() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (ids == nullptr) {
        throw std::runtime_error(std::string("SDL_GetJoysticks: ") + SDL_GetError());
    }

    for (auto& slot : slots_) {
        if (slot.handle != nullptr &&
            (!SDL_JoystickConnected(slot.handle) ||
             !contains_id(ids, count, slot.instance_id))) {
            close_slot(slot);
        }
    }

    for (int i = 0; i < count; ++i) {
        const SDL_JoystickID id = ids[i];
        const bool assigned = std::any_of(slots_.begin(), slots_.end(),
            [id](const Slot& slot) {
                return slot.handle != nullptr && slot.instance_id == id;
            });
        if (assigned) continue;

        const auto vacant = std::find_if(slots_.begin(), slots_.end(),
            [](const Slot& slot) { return slot.handle == nullptr; });
        if (vacant == slots_.end()) break;

        SDL_Joystick* handle = SDL_OpenJoystick(id);
        if (handle == nullptr) continue;
        if (!SDL_JoystickConnected(handle)) {
            SDL_CloseJoystick(handle);
            continue;
        }
        vacant->instance_id = id;
        vacant->handle = handle;
    }
    SDL_free(ids);
}

JoystickControls SdlJoysticks::sample() const noexcept {
    SDL_UpdateJoysticks();
    JoystickControls result{};
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        result[i] = read_controls(slots_[i].handle);
    }
    return result;
}

} // namespace river_raid
