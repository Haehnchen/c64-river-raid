#include "game/session_idle.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

using river_raid::SessionIdle;
using river_raid::SessionIdlePhase;
using river_raid::SessionIdleState;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_inactive_and_phase_filter() {
    SessionIdle idle;
    require(idle.state() == SessionIdleState{SessionIdlePhase::Inactive, 0},
            "idle should start inactive");
    require(!idle.advance(0), "inactive idle must not enter attract");

    idle.begin_terminal();
    for (const auto phase : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{7},
                             std::uint8_t{15}}) {
        require(!idle.advance(phase), "nonzero low phase bits must be ignored");
        require(idle.state() == SessionIdleState{SessionIdlePhase::Waiting, 0xFF},
                "ignored phase must not decrement countdown");
    }
    require(!idle.advance(8), "first qualifying sample must not enter attract");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Waiting, 0xFE},
            "phase with low bits clear must decrement countdown");
}

void test_terminal_countdown() {
    SessionIdle idle;
    idle.begin_terminal();
    for (std::uint16_t sample = 1; sample < 255; ++sample) {
        require(!idle.advance(0), "terminal countdown entered attract too early");
        require(idle.state() == SessionIdleState{
                                    SessionIdlePhase::Waiting,
                                    static_cast<std::uint8_t>(255 - sample)},
                "terminal countdown has an unexpected value");
    }
    require(idle.advance(0), "255th qualifying terminal sample must enter attract");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFF},
            "terminal transition must reload countdown in attract phase");
}

void test_option_countdown() {
    SessionIdle idle;
    idle.begin_option_wait();
    for (std::uint16_t sample = 1; sample < 37; ++sample) {
        require(!idle.advance(0), "option wait entered attract too early");
        require(idle.state() == SessionIdleState{
                                    SessionIdlePhase::Waiting,
                                    static_cast<std::uint8_t>(37 - sample)},
                "option wait countdown has an unexpected value");
    }
    require(idle.advance(0), "37th qualifying option sample must enter attract");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFF},
            "option transition must reload countdown in attract phase");
}

void test_initial_countdown() {
    SessionIdle idle;
    idle.begin_initial_wait();
    require(idle.state() == SessionIdleState{SessionIdlePhase::Waiting, 1},
            "initial wait must use its source countdown");
    require(idle.advance(0), "first qualifying initial sample must enter attract");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFF},
            "initial transition must reload countdown in attract phase");
}

void test_attract_reload_without_reentry() {
    SessionIdle idle;
    idle.begin_option_wait();
    for (int sample = 0; sample < 37; ++sample) {
        require(idle.advance(0) == (sample == 36),
                "option wait setup returned an unexpected transition");
    }
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFF},
            "test setup did not enter attract");

    for (int sample = 1; sample < 255; ++sample) {
        require(!idle.advance(0), "attract countdown must not re-enter attract");
    }
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 1},
            "attract countdown should reach one before reloading");
    require(!idle.advance(0), "attract reload must not signal a new transition");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFF},
            "attract countdown must reload while remaining in attract");
    require(!idle.advance(0), "continued attract countdown must not signal entry");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Attract, 0xFE},
            "attract countdown should continue after reload");
}

void test_stop_and_restart() {
    SessionIdle idle;
    idle.begin_terminal();
    idle.stop();
    require(idle.state() == SessionIdleState{SessionIdlePhase::Inactive, 0},
            "stop must reset idle state");
    require(!idle.advance(0), "stopped idle must ignore qualifying samples");
    require(idle.state() == SessionIdleState{SessionIdlePhase::Inactive, 0},
            "inactive countdown changed after stop");

    idle.begin_option_wait();
    require(idle.state() == SessionIdleState{SessionIdlePhase::Waiting, 37},
            "option wait must restart with its source countdown");
    for (int sample = 0; sample < 37; ++sample) {
        require(idle.advance(0) == (sample == 36),
                "option wait restart returned an unexpected transition");
    }
    require(idle.state().phase == SessionIdlePhase::Attract,
            "option wait restart did not enter attract");

    idle.begin_terminal();
    require(idle.state() == SessionIdleState{SessionIdlePhase::Waiting, 255},
            "terminal begin must restart an existing attract countdown");
}

} // namespace

int main() {
    try {
        test_inactive_and_phase_filter();
        test_terminal_countdown();
        test_option_countdown();
        test_initial_countdown();
        test_attract_reload_without_reentry();
        test_stop_and_restart();
    } catch (const std::exception& error) {
        std::cerr << "session idle test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "session idle tests passed\n";
    return 0;
}
