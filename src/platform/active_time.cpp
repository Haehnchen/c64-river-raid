#include "platform/active_time.hpp"

#include <stdexcept>

namespace river_raid {

ActiveTime::ActiveTime(std::uint64_t now_ns, bool active) noexcept
    : previous_(now_ns), active_(active) {}

void ActiveTime::account(std::uint64_t now_ns) {
    if (now_ns < previous_) throw std::invalid_argument("Preview clock moved backwards");
    const auto delta = active_ ? now_ns - previous_ : 0;
    constexpr auto maximum = static_cast<std::uint64_t>(std::chrono::nanoseconds::max().count());
    if (delta > maximum - pending_) throw std::overflow_error("Active preview time overflow");
    pending_ += delta;
    previous_ = now_ns;
}

void ActiveTime::set_active(std::uint64_t now_ns, bool active) {
    account(now_ns);
    active_ = active;
}

std::chrono::nanoseconds ActiveTime::take_elapsed(std::uint64_t now_ns) {
    account(now_ns);
    if (!active_) return std::chrono::nanoseconds{0};
    const auto elapsed = std::chrono::nanoseconds{static_cast<std::int64_t>(pending_)};
    pending_ = 0;
    return elapsed;
}

bool ActiveTime::active() const noexcept { return active_; }

} // namespace river_raid
