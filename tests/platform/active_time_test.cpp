#include "platform/active_time.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        using namespace std::chrono_literals;
        river_raid::ActiveTime time(100, false);
        require(time.take_elapsed(200) == 0ns, "Inactive initial time was counted");
        time.set_active(1000, true);
        require(time.take_elapsed(1010) == 10ns, "First active interval differs");
        require(time.take_elapsed(1010) == 0ns, "Duplicate timestamp counted twice");
        time.set_active(1020, false);
        require(time.take_elapsed(5000) == 0ns, "Paused time was delivered");
        time.set_active(6000, false);
        time.set_active(10000, true);
        require(time.take_elapsed(10007) == 17ns, "Pause lost active debt or counted inactive time");
        time.set_active(10010, true);
        time.set_active(10020, false);
        time.set_active(10030, true);
        require(time.take_elapsed(10030) == 13ns, "Repeated transitions lost active time");

        bool backwards = false;
        try { time.set_active(10029, false); }
        catch (const std::invalid_argument&) { backwards = true; }
        require(backwards && time.active(), "Backward transition changed focus state");
        require(time.take_elapsed(10031) == 1ns, "Backward transition changed the epoch");
        backwards = false;
        try { (void)time.take_elapsed(10030); }
        catch (const std::invalid_argument&) { backwards = true; }
        require(backwards && time.take_elapsed(10032) == 1ns, "Backward poll changed time");

        const auto max_duration = static_cast<std::uint64_t>(std::chrono::nanoseconds::max().count());
        river_raid::ActiveTime full(0, true);
        full.set_active(max_duration - 1, true);
        bool overflow = false;
        try { full.set_active(max_duration + 1, false); }
        catch (const std::overflow_error&) { overflow = true; }
        require(overflow && full.active(), "Overflow changed activity state");
        require(full.take_elapsed(max_duration) == std::chrono::nanoseconds::max(), "Overflow changed active debt");
        require(full.take_elapsed(max_duration + 1) == 1ns, "Post-overflow continuation differs");
        river_raid::ActiveTime inactive(0, false);
        require(inactive.take_elapsed(std::numeric_limits<std::uint64_t>::max()) == 0ns,
                "Long inactive interval overflowed");
        inactive.set_active(std::numeric_limits<std::uint64_t>::max(), true);
        require(inactive.take_elapsed(std::numeric_limits<std::uint64_t>::max()) == 0ns, "Inactive gap leaked");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
