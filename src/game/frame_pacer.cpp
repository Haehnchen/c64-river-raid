#include "game/frame_pacer.hpp"

#include <limits>
#include <stdexcept>

namespace river_raid {
namespace {

constexpr std::uint64_t kNanosPerSecond = 1'000'000'000ULL;
constexpr std::uint64_t kMaxUint64 = std::numeric_limits<std::uint64_t>::max();

[[nodiscard]] std::uint64_t checked_add(std::uint64_t left, std::uint64_t right) {
    if (left > kMaxUint64 - right) {
        throw std::overflow_error("frame tick count exceeds uint64_t");
    }
    return left + right;
}

// Divide seconds * numerator by denominator without ever forming the product
// of the two potentially large operands.  The low product is bounded by
// (2^32 - 1)^2, which fits in uint64_t.
struct WholeSeconds {
    std::uint64_t ticks;
    std::uint64_t remainder;
};

[[nodiscard]] WholeSeconds divide_whole_seconds(std::uint64_t seconds,
                                                FrameRate rate) {
    const auto denominator = static_cast<std::uint64_t>(rate.denominator);
    const auto numerator = static_cast<std::uint64_t>(rate.numerator);

    const auto high = seconds / denominator;
    const auto low = seconds % denominator;

    if (high > kMaxUint64 / numerator) {
        throw std::overflow_error("frame tick count exceeds uint64_t");
    }
    const auto high_ticks = high * numerator;

    // low < denominator <= UINT32_MAX and numerator <= UINT32_MAX.
    const auto low_product = low * numerator;
    const auto low_ticks = low_product / denominator;
    const auto ticks = checked_add(high_ticks, low_ticks);
    return {ticks, low_product % denominator};
}

} // namespace

FramePacer::FramePacer(FrameRate rate)
    : rate_(rate),
      fractional_denominator_(static_cast<std::uint64_t>(rate.denominator) *
                              kNanosPerSecond) {
    if (rate.numerator == 0 || rate.denominator == 0) {
        throw std::invalid_argument("frame rate numerator and denominator must be nonzero");
    }
}

std::uint64_t FramePacer::advance_elapsed(std::chrono::nanoseconds elapsed) {
    const auto count = elapsed.count();
    if (count < 0) {
        throw std::invalid_argument("elapsed duration must not be negative");
    }

    const auto unsigned_count = static_cast<std::uint64_t>(count);
    const auto seconds = unsigned_count / kNanosPerSecond;
    const auto nanos = unsigned_count % kNanosPerSecond;
    const auto whole = divide_whole_seconds(seconds, rate_);

    // Every term below is bounded: denominator * 1e9 is below 2^63, and
    // nanos * numerator is below 2^62.  Thus the exact fractional sum needs
    // no wider-than-standard integer type.
    const auto second_fraction = whole.remainder * kNanosPerSecond;
    const auto nanosecond_product = nanos * static_cast<std::uint64_t>(rate_.numerator);
    const auto with_nanoseconds = checked_add(second_fraction, nanosecond_product);
    const auto fractional_sum = checked_add(with_nanoseconds, remainder_);
    const auto subsecond_ticks = fractional_sum / fractional_denominator_;
    const auto new_remainder = fractional_sum % fractional_denominator_;
    const auto ticks = checked_add(whole.ticks, subsecond_ticks);

    remainder_ = new_remainder;
    return ticks;
}

void FramePacer::reset() noexcept {
    remainder_ = 0;
}

} // namespace river_raid
