#include "game/frame_pacer.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using namespace std::chrono_literals;
using river_raid::FramePacer;
using river_raid::FrameRate;

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Function>
void expect_throw(Function&& function, const char* message) {
    try {
        function();
    } catch (const Exception&) {
        return;
    } catch (...) {
        throw std::runtime_error(message);
    }
    throw std::runtime_error(message);
}

void test_exact_boundaries_and_fraction() {
    FramePacer pacer({25, 1});
    expect(pacer.advance_elapsed(39'999'999ns) == 0, "tick arrived before exact boundary");
    expect(pacer.advance_elapsed(1ns) == 1, "tick missed exact 1/25-second boundary");
    expect(pacer.remainder() == 0, "exact boundary left a remainder");
    FramePacer fractional({3, 1});
    expect(fractional.advance_elapsed(333'333'333ns) == 0,
           "tick arrived before fractional boundary");
    expect(fractional.advance_elapsed(1ns) == 1, "fraction was not retained");
    expect(fractional.remainder() == 2, "unexpected nanosecond remainder");
}

void test_chunk_invariance() {
    constexpr FrameRate rate{3, 2};
    FramePacer one_call(rate);
    FramePacer chunks(rate);
    const auto duration = 123'456'789'012'345ns;
    const auto expected = one_call.advance_elapsed(duration);

    std::uint64_t actual = 0;
    for (const auto part : {100'000'000'000'000ns, 23'456'789'012'345ns}) {
        actual += chunks.advance_elapsed(part);
    }
    expect(actual == expected, "chunked elapsed time changed due ticks");
    expect(chunks.remainder() == one_call.remainder(), "chunked elapsed time changed remainder");
}

void test_long_duration_and_high_rate() {
    FramePacer high_rate({4'000'000'001U, 3U});
    const auto count = high_rate.advance_elapsed(std::chrono::nanoseconds{9'000'000'000'000'000'000LL});
    const auto expected = (4'000'000'001ULL / 3ULL) * 9'000'000'000ULL +
                          ((4'000'000'001ULL % 3ULL) * 9'000'000'000ULL) / 3ULL;
    expect(count == expected, "long duration/high rate calculation was inaccurate");

    FramePacer max_rate({std::numeric_limits<std::uint32_t>::max(), 1});
    expect(max_rate.advance_elapsed(1ns) == 4,
           "subsecond high rate did not produce its integral ticks");
    expect(max_rate.remainder() == 294'967'295,
           "subsecond high rate remainder was inaccurate");
    expect(max_rate.advance_elapsed(1s) == std::numeric_limits<std::uint32_t>::max(),
           "rate above one billion was not supported");

    FramePacer max_members({std::numeric_limits<std::uint32_t>::max(),
                            std::numeric_limits<std::uint32_t>::max()});
    expect(max_members.advance_elapsed(1s) == 1,
           "maximum numerator/denominator members were not accepted");

    // Exercise all three terms in the bounded fractional sum at once.  These
    // values are the exact integer quotient/remainder for the stated inputs.
    FramePacer maximum_fraction({std::numeric_limits<std::uint32_t>::max(),
                                 std::numeric_limits<std::uint32_t>::max() - 1});
    expect(maximum_fraction.advance_elapsed(1ns) == 0,
           "maximum-member setup unexpectedly emitted a tick");
    expect(maximum_fraction.advance_elapsed(std::chrono::nanoseconds::max()) ==
               9'223'372'039ULL,
           "maximum-member long duration quotient was inaccurate");
    expect(maximum_fraction.remainder() == 9'704'293'917'199'360ULL,
           "maximum-member long duration remainder was inaccurate");
}

void test_overflow_and_state_unchanged() {
    FramePacer pacer({std::numeric_limits<std::uint32_t>::max(), 1});
    (void)pacer.advance_elapsed(1ns);
    const auto before_overflow = pacer.remainder();
    expect_throw<std::overflow_error>(
        [&] { (void)pacer.advance_elapsed(std::chrono::nanoseconds::max()); },
        "unrepresentable tick count did not throw");
    expect(pacer.remainder() == before_overflow, "overflow changed pacer state");
}

void test_invalid_input_and_reset() {
    expect_throw<std::invalid_argument>([] { FramePacer invalid({0, 1}); },
                                        "zero numerator was accepted");
    expect_throw<std::invalid_argument>([] { FramePacer invalid({1, 0}); },
                                        "zero denominator was accepted");

    FramePacer pacer({7, 3});
    expect_throw<std::invalid_argument>([&] { (void)pacer.advance_elapsed(-1ns); },
                                        "negative duration was accepted");
    expect(pacer.advance_elapsed(1ns) == 0, "negative duration changed state");
    (void)pacer.advance_elapsed(1s);
    expect(pacer.remainder() != 0, "test did not establish fractional state");
    pacer.reset();
    expect(pacer.remainder() == 0, "reset did not clear fractional state");
    expect(pacer.advance_elapsed(1s) == 2, "reset changed configured rate");
}

} // namespace

int main() {
    try {
        test_exact_boundaries_and_fraction();
        test_chunk_invariance();
        test_long_duration_and_high_rate();
        test_overflow_and_state_unchanged();
        test_invalid_input_and_reset();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
