#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

struct SDL_AudioStream;

namespace river_raid {

inline constexpr int kPreviewAudioSampleRate = 48000;
inline constexpr std::size_t kPreviewAudioMaxQueuedFrames =
    static_cast<std::size_t>(kPreviewAudioSampleRate) / 10;

// Main-thread PCM handoff for the preview. The SDL stream remains a platform
// detail; no game or synthesis state is touched by SDL's audio thread.
class SdlAudioDevice {
public:
    SdlAudioDevice();
    ~SdlAudioDevice();

    SdlAudioDevice(const SdlAudioDevice&) = delete;
    SdlAudioDevice& operator=(const SdlAudioDevice&) = delete;
    SdlAudioDevice(SdlAudioDevice&&) = delete;
    SdlAudioDevice& operator=(SdlAudioDevice&&) = delete;

    // Inactive devices pause and discard queued PCM. Resuming is explicit.
    void set_active(bool active);
    void clear();
    void submit(std::span<const float> mono_samples);

    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] std::size_t queued_bytes() const;
    [[nodiscard]] std::uint64_t dropped_frames() const noexcept {
        return dropped_frames_;
    }

private:
    SDL_AudioStream* stream_{};
    bool active_{};
    std::uint64_t dropped_frames_{};
};

} // namespace river_raid
