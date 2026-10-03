#include "game/title_delay.hpp"

#include <stdexcept>

namespace river_raid {

TitleDelay::TitleDelay(std::chrono::nanoseconds duration) : remaining_(duration) {
    if (duration.count() <= 0) {
        throw std::invalid_argument("Title delay must be positive");
    }
}

bool TitleDelay::advance_elapsed(std::chrono::nanoseconds elapsed,
                                 bool controls_released) {
    if (elapsed.count() < 0) {
        throw std::invalid_argument("Elapsed title time must not be negative");
    }
    if (expired()) return true;
    if (!started_) {
        if (!controls_released) return false;
        started_ = true;
    }

    if (elapsed >= remaining_) {
        remaining_ = std::chrono::nanoseconds::zero();
    } else {
        remaining_ -= elapsed;
    }
    return expired();
}

std::chrono::nanoseconds TitleDelay::remaining() const noexcept {
    return remaining_;
}

bool TitleDelay::expired() const noexcept {
    return remaining_.count() == 0;
}

} // namespace river_raid
