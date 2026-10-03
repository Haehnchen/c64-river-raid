#include "game/title_delay.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using namespace std::chrono_literals;
using river_raid::TitleDelay;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_initial_release_gates_start_only() {
    TitleDelay delay(10ns);
    require(!delay.advance_elapsed(4ns, false) && delay.remaining() == 10ns,
            "Held controls must not start the title delay");
    require(!delay.advance_elapsed(4ns, true) && delay.remaining() == 6ns,
            "The first fully released sample must start the countdown");
    require(!delay.advance_elapsed(3ns, false) && delay.remaining() == 3ns,
            "A started countdown must continue while controls are held");
    require(delay.advance_elapsed(3ns, false) && delay.expired() &&
                delay.remaining() == 0ns,
            "Title delay did not expire at its configured duration");
    require(delay.advance_elapsed(0ns, false), "Expiry must remain latched");
}

void test_release_sample_can_consume_elapsed() {
    TitleDelay delay(5ns);
    require(delay.advance_elapsed(5ns, true) && delay.expired(),
            "Released sample elapsed time was not consumed");
}

void test_split_elapsed_matches_single_interval() {
    TitleDelay split(17ns);
    TitleDelay single(17ns);
    require(!split.advance_elapsed(0ns, true), "Zero-duration release sample expired early");
    require(!single.advance_elapsed(0ns, true), "Zero-duration release sample expired early");
    require(!split.advance_elapsed(4ns, false), "Split interval expired early");
    require(!split.advance_elapsed(6ns, true), "Split interval expired early");
    require(split.advance_elapsed(7ns, false), "Split intervals did not sum to the duration");
    require(single.advance_elapsed(17ns, false), "Single interval did not expire");
    require(split.remaining() == single.remaining() && split.expired() == single.expired(),
            "Elapsed-time partition changed title delay state");
}

void test_negative_elapsed_is_rejected_without_mutation() {
    TitleDelay delay(9ns);
    bool rejected = false;
    try {
        (void)delay.advance_elapsed(-1ns, true);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && delay.remaining() == 9ns && !delay.expired(),
            "Negative elapsed time mutated or was accepted by the delay");
}

void test_duration_validation_and_saturation() {
    for (const auto invalid : {0ns, -1ns}) {
        bool rejected = false;
        try {
            (void)TitleDelay(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Nonpositive title duration must be rejected");
    }

    TitleDelay delay(12ns);
    require(delay.advance_elapsed(std::chrono::nanoseconds::max(), true) &&
                delay.remaining() == 0ns,
            "Large elapsed duration did not saturate at expiry");
}

} // namespace

int main() {
    try {
        test_initial_release_gates_start_only();
        test_release_sample_can_consume_elapsed();
        test_split_elapsed_matches_single_interval();
        test_negative_elapsed_is_rejected_without_mutation();
        test_duration_validation_and_saturation();
    } catch (const std::exception& error) {
        std::cerr << "title delay test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "title delay tests passed\n";
    return 0;
}
