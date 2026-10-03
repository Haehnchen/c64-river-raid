#include "platform/sdl_audio.hpp"

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace river_raid {
namespace {

constexpr std::size_t kBytesPerFrame = sizeof(float);
constexpr std::size_t kMaxQueuedBytes = kPreviewAudioMaxQueuedFrames * kBytesPerFrame;

[[noreturn]] void throw_sdl(const char* operation) {
    std::string message(operation);
    message += ": ";
    message += SDL_GetError();
    throw std::runtime_error(message);
}

void require_sdl(bool success, const char* operation) {
    if (!success) throw_sdl(operation);
}

} // namespace

SdlAudioDevice::SdlAudioDevice() {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kPreviewAudioSampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr,
                                        nullptr);
    if (!stream_) throw_sdl("SDL_OpenAudioDeviceStream");
    // SDL3 opens the convenience stream paused. Keep the explicit state here
    // so preview focus/minimize gates cannot accidentally emit audio.
    active_ = false;
}

SdlAudioDevice::~SdlAudioDevice() {
    if (stream_) SDL_DestroyAudioStream(stream_);
}

void SdlAudioDevice::set_active(bool active) {
    if (active == active_) return;
    if (!active) {
        require_sdl(SDL_PauseAudioStreamDevice(stream_), "SDL_PauseAudioStreamDevice");
        require_sdl(SDL_ClearAudioStream(stream_), "SDL_ClearAudioStream");
        active_ = false;
        return;
    }
    require_sdl(SDL_ResumeAudioStreamDevice(stream_), "SDL_ResumeAudioStreamDevice");
    active_ = true;
}

void SdlAudioDevice::clear() {
    require_sdl(SDL_ClearAudioStream(stream_), "SDL_ClearAudioStream");
}

void SdlAudioDevice::submit(std::span<const float> mono_samples) {
    if (!active_ || mono_samples.empty()) return;
    if (mono_samples.size() > std::numeric_limits<std::size_t>::max() / kBytesPerFrame) {
        throw std::overflow_error("Audio sample block size overflow");
    }

    const auto queued = queued_bytes();
    if (queued > kMaxQueuedBytes) {
        throw std::overflow_error("SDL audio queue exceeded its bound");
    }
    const auto available = kMaxQueuedBytes - queued;
    const auto requested = mono_samples.size() * kBytesPerFrame;
    auto accepted = std::min(available, requested);
    accepted -= accepted % kBytesPerFrame;
    if (accepted < requested) dropped_frames_ += (requested - accepted) / kBytesPerFrame;
    if (accepted == 0) return;
    if (accepted > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("Audio sample block exceeds SDL length");
    }
    require_sdl(SDL_PutAudioStreamData(stream_, mono_samples.data(),
                                       static_cast<int>(accepted)),
                 "SDL_PutAudioStreamData");
}

std::size_t SdlAudioDevice::queued_bytes() const {
    const auto queued = SDL_GetAudioStreamQueued(stream_);
    if (queued < 0) throw_sdl("SDL_GetAudioStreamQueued");
    return static_cast<std::size_t>(queued);
}

} // namespace river_raid
