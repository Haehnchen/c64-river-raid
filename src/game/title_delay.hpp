#pragma once

#include <chrono>

namespace river_raid {

class TitleDelay {
public:
    explicit TitleDelay(std::chrono::nanoseconds duration);

    [[nodiscard]] bool advance_elapsed(std::chrono::nanoseconds elapsed,
                                       bool controls_released);
    [[nodiscard]] std::chrono::nanoseconds remaining() const noexcept;
    [[nodiscard]] bool expired() const noexcept;

private:
    std::chrono::nanoseconds remaining_;
    bool started_{};
};

} // namespace river_raid
