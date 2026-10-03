#include "app/approximate_activation_source.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

constexpr std::uint64_t kNonzeroSeed = 0x0123456789ABCDEFULL;

// Independently computed SplitMix64 top-byte vectors. The product source is
// deliberately not used to derive these expected values.
constexpr std::array<std::uint8_t, 16> kSeedZero{
    0xE2, 0x6E, 0x06, 0xF8, 0x1B, 0x53, 0x2C, 0xC5,
    0x3E, 0xF3, 0x65, 0xC2, 0x86, 0x8E, 0xB5, 0x84,
};
constexpr std::array<std::uint8_t, 16> kNonzeroSeedBytes{
    0x15, 0xD5, 0x2F, 0xA2, 0x01, 0x14, 0xB8, 0x89,
    0xF9, 0x26, 0xCD, 0x6A, 0x8E, 0x97, 0x38, 0xD7,
};

void require_vector(ApproximateActivationSource& source,
                    const auto& expected, std::string_view message) {
    for (const auto value : expected) {
        require(source.next() == value, message);
    }
}

void test_known_vectors_and_seed_ownership() {
    ApproximateActivationSource zero{0};
    require(zero.seed() == 0, "Zero source did not retain its seed");
    require_vector(zero, kSeedZero, "SplitMix64 zero-seed vector differs");

    ApproximateActivationSource nonzero{kNonzeroSeed};
    require(nonzero.seed() == kNonzeroSeed, "Nonzero source did not retain its seed");
    require_vector(nonzero, kNonzeroSeedBytes, "SplitMix64 nonzero vector differs");
}

void test_reset_after_long_advance() {
    ApproximateActivationSource source{0};
    for (unsigned opportunity = 0; opportunity < 256; ++opportunity) {
        (void)source.next();
    }
    source.reset();
    require(source.seed() == 0, "Reset changed the owned seed");
    require_vector(source, kSeedZero, "Reset did not restore the initial stream");

    source.reset(kNonzeroSeed);
    require(source.seed() == kNonzeroSeed, "Explicit reset did not replace the seed");
    require_vector(source, kNonzeroSeedBytes,
                   "Explicit reset did not restore the replacement stream");

    for (unsigned opportunity = 0; opportunity < 256; ++opportunity) {
        (void)source.next();
    }
    source.reset();
    require(source.seed() == kNonzeroSeed,
            "Implicit reset discarded the explicitly selected seed");
    require_vector(source, kNonzeroSeedBytes,
                   "Implicit reset did not restore the explicitly selected stream");
}

void test_default_seed_is_stable() {
    ApproximateActivationSource source;
    require(source.seed() == kDefaultActivationSeed,
            "Default activation source seed changed unexpectedly");
}

} // namespace

int main() {
    try {
        test_known_vectors_and_seed_ownership();
        test_reset_after_long_advance();
        test_default_seed_is_stable();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
