#pragma once

#include <cstdint>

namespace river_raid {

enum class SessionIdlePhase : std::uint8_t {
    Inactive,
    Waiting,
    Attract,
};

struct SessionIdleState {
    SessionIdlePhase phase{SessionIdlePhase::Inactive};
    std::uint8_t countdown{};

    friend bool operator==(const SessionIdleState&, const SessionIdleState&) = default;
};

class SessionIdle {
public:
    void begin_initial_wait() noexcept;
    void begin_terminal() noexcept;
    void begin_option_wait() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool advance(std::uint8_t sampled_raster_phase) noexcept;
    [[nodiscard]] const SessionIdleState& state() const noexcept;

private:
    SessionIdleState state_{};
};

} // namespace river_raid
