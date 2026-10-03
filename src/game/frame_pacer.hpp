#pragma once

#include <chrono>
#include <cstdint>

namespace river_raid {

// A tick rate expressed as ticks per second.
struct FrameRate {
    std::uint32_t numerator{};
    std::uint32_t denominator{};
};

class FramePacer {
public:
    explicit FramePacer(FrameRate rate);

    // Return the number of ticks due for elapsed time and retain its fraction.
    // The retained fraction is measured in denominator * nanoseconds per second
    // and is always in [0, denominator * 1'000'000'000).  This invariant makes
    // successive calls exactly equivalent to one call for their summed duration.
    // Negative durations and unrepresentable tick counts throw; in either case
    // the pacer's state is unchanged.
    [[nodiscard]] std::uint64_t advance_elapsed(std::chrono::nanoseconds elapsed);

    void reset() noexcept;

    [[nodiscard]] FrameRate rate() const noexcept { return rate_; }
    [[nodiscard]] std::uint64_t remainder() const noexcept { return remainder_; }

private:
    FrameRate rate_;
    std::uint64_t fractional_denominator_;
    std::uint64_t remainder_{};
};

} // namespace river_raid
