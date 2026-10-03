#pragma once

#include <cstdint>

namespace river_raid {

inline constexpr std::uint64_t kDefaultActivationSeed = 0x5249564552524149ULL;

// Product-only approximation: deterministic SplitMix64 replaces original timer samples.
class ApproximateActivationSource {
public:
    explicit constexpr ApproximateActivationSource(
        std::uint64_t seed = kDefaultActivationSeed) noexcept
        : seed_(seed), state_(seed) {}

    [[nodiscard]] constexpr std::uint64_t seed() const noexcept { return seed_; }

    constexpr void reset() noexcept { state_ = seed_; }
    constexpr void reset(std::uint64_t seed) noexcept {
        seed_ = seed;
        state_ = seed;
    }

    [[nodiscard]] constexpr std::uint8_t next() noexcept {
        state_ += kIncrement;
        auto mixed = state_;
        mixed = (mixed ^ (mixed >> 30U)) * kMultiplier1;
        mixed = (mixed ^ (mixed >> 27U)) * kMultiplier2;
        mixed ^= mixed >> 31U;
        return static_cast<std::uint8_t>(mixed >> 56U);
    }

private:
    static constexpr std::uint64_t kIncrement{0x9E3779B97F4A7C15ULL};
    static constexpr std::uint64_t kMultiplier1{0xBF58476D1CE4E5B9ULL};
    static constexpr std::uint64_t kMultiplier2{0x94D049BB133111EBULL};

    std::uint64_t seed_;
    std::uint64_t state_;
};

} // namespace river_raid
