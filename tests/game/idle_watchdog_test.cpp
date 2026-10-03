#include "game/idle_watchdog.hpp"

#include <iostream>
#include <stdexcept>

namespace {

using river_raid::IdleWatchdog;
using river_raid::IdleWatchdogState;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_initial_state_and_exact_threshold() {
    IdleWatchdog watchdog;
    require(watchdog.state() == IdleWatchdogState{} && !watchdog.blanked(),
            "Watchdog should start clear");
    for (int wrap = 0; wrap < 1023; ++wrap) {
        require(!watchdog.notify_phase_wrap(), "Watchdog blanked before 1024 phase wraps");
    }
    require(watchdog.state() == IdleWatchdogState{7, 127, false},
            "Watchdog counters differ before the threshold");
    require(watchdog.notify_phase_wrap(), "1024th phase wrap must first blank the display");
    require(watchdog.state() == IdleWatchdogState{0, 128, true} && watchdog.blanked(),
            "Watchdog did not latch the blank state at the threshold");
}

void test_activity_resets_idle_count_only() {
    IdleWatchdog watchdog;
    for (int wrap = 0; wrap < 11; ++wrap) {
        require(!watchdog.notify_phase_wrap(), "Short idle interval blanked unexpectedly");
    }
    require(watchdog.state() == IdleWatchdogState{3, 1, false},
            "Watchdog phase subdivision setup differs");
    watchdog.note_activity();
    require(watchdog.state() == IdleWatchdogState{3, 0, false},
            "Activity must reset only the idle count");

    for (int wrap = 0; wrap < 5; ++wrap) {
        require(!watchdog.notify_phase_wrap(), "Watchdog blanked before a full eighth wrap");
    }
    require(watchdog.state() == IdleWatchdogState{0, 1, false},
            "Activity reset the phase subdivision or lost its subsequent count");
}

void test_blank_is_sticky_and_counters_do_not_overflow() {
    IdleWatchdog watchdog;
    for (int wrap = 0; wrap < 1024; ++wrap) (void)watchdog.notify_phase_wrap();
    require(watchdog.blanked(), "Watchdog setup did not blank");

    for (int wrap = 0; wrap < 10000; ++wrap) {
        require(!watchdog.notify_phase_wrap(), "Sticky blank emitted a second transition");
    }
    require(watchdog.state() == IdleWatchdogState{0, 128, true},
            "Blanked watchdog counters advanced or overflowed");
    watchdog.note_activity();
    require(watchdog.blanked(), "Activity must not clear the sticky blank state");
}

void test_reset_clears_both_counters_and_blank() {
    IdleWatchdog watchdog;
    for (int wrap = 0; wrap < 1024; ++wrap) (void)watchdog.notify_phase_wrap();
    watchdog.reset();
    require(watchdog.state() == IdleWatchdogState{} && !watchdog.blanked(),
            "Reset must clear subdivision, idle count, and blank state");
    for (int wrap = 0; wrap < 1023; ++wrap) {
        require(!watchdog.notify_phase_wrap(), "Reset watchdog blanked early");
    }
}

} // namespace

int main() {
    try {
        test_initial_state_and_exact_threshold();
        test_activity_resets_idle_count_only();
        test_blank_is_sticky_and_counters_do_not_overflow();
        test_reset_clears_both_counters_and_blank();
    } catch (const std::exception& error) {
        std::cerr << "idle watchdog test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "idle watchdog tests passed\n";
    return 0;
}
