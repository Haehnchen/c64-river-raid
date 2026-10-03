#pragma once

#include <cstdint>

namespace river_raid {

struct IdleWatchdogState {
    std::uint8_t wraps_toward_eighth{};
    std::uint8_t idle_eighth_wraps{};
    bool blanked{};

    friend bool operator==(const IdleWatchdogState&, const IdleWatchdogState&) = default;
};

class IdleWatchdog {
public:
    // Returns true only on the phase-wrap event that first blanks the display.
    [[nodiscard]] bool notify_phase_wrap() noexcept;
    void note_activity() noexcept;
    void reset() noexcept;

    [[nodiscard]] const IdleWatchdogState& state() const noexcept;
    [[nodiscard]] bool blanked() const noexcept;

private:
    IdleWatchdogState state_{};
};

} // namespace river_raid
