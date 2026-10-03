#pragma once

#include <cstdint>

namespace river_raid {

struct DemoClockState {
    std::uint8_t animation_phase{};
    std::uint8_t startup_ticks_remaining{};
    std::uint8_t scroll_speed{};
    std::uint8_t scroll_fraction{};

    friend bool operator==(const DemoClockState&, const DemoClockState&) = default;
};

struct DemoScrollSpeeds {
    std::uint8_t startup{};
    std::uint8_t steady{};
};

struct DemoClockTick {
    std::uint8_t animation_phase{};
    std::uint8_t terrain_rows{};

    friend bool operator==(const DemoClockTick&, const DemoClockTick&) = default;
};

class DemoClock {
public:
    DemoClock(DemoClockState initial_state, DemoScrollSpeeds scroll_speeds) noexcept;

    [[nodiscard]] DemoClockTick advance() noexcept;
    void reset() noexcept;
    [[nodiscard]] const DemoClockState& state() const noexcept;

private:
    [[nodiscard]] std::uint8_t advance_scroll() noexcept;

    DemoClockState initial_state_;
    DemoClockState state_;
    DemoScrollSpeeds scroll_speeds_;
};

} // namespace river_raid
