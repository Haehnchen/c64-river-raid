#pragma once

#include <chrono>
#include <cstdint>

namespace river_raid {

// Accounts activity at observed transition times, not historical event times.
class ActiveTime {
public:
    ActiveTime(std::uint64_t now_ns, bool active) noexcept;
    void set_active(std::uint64_t now_ns, bool active);
    // Paused calls retain previously accrued active time until resumption.
    [[nodiscard]] std::chrono::nanoseconds take_elapsed(std::uint64_t now_ns);
    [[nodiscard]] bool active() const noexcept;

private:
    void account(std::uint64_t now_ns);
    std::uint64_t previous_;
    std::uint64_t pending_{};
    bool active_;
};

} // namespace river_raid
