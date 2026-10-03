#pragma once

#include "support/long_session_inputs.hpp"

#include <cstddef>

namespace river_raid::test {

// Native reset-work pilot output, not original-game controls. Replay after
// normal Game 5 startup and 84 launch updates; no phase-alignment wait.
inline constexpr SessionInputRun game5_regular_gap_route[]{
    {0,32,true},{1,64,true},{2,16,true},{0,80,true},
    {1,16,true},{0,16,true},{1,16,true},{2,16,true},
    {0,16,true},{1,16,true},{2,64,true},{1,16,true},
    {0,16,true},{1,16,true},{2,16,true},{0,48,true},
    {1,16,true},{0,16,true},{0,32,false},{0,16,true},
    {1,16,true},{2,16,true},{0,64,true},{2,16,true},
    {1,16,true},{0,48,true},{2,16,true},{1,16,true},
    {2,16,true},{0,32,true},{2,32,true},{0,16,true},
    {2,16,true},{0,16,true},{2,16,true},{1,16,true},
    {0,16,true},{2,16,true},{1,48,true},{0,32,true},
    {1,16,true},{2,16,true},{0,32,true},{2,16,true},
    {1,16,true},{0,48,true},{2,32,true},{0,32,true},
    {2,16,true},{0,16,true},{2,16,true},{1,32,true},
    {0,16,true},{2,32,true},{0,16,true},{2,16,true},
    {0,16,true},{1,32,true},{0,32,true},{1,16,true},
    {2,16,true},{1,16,true},{0,80,true},{1,16,true},
    {0,32,true},{1,16,true},{0,6,true},
};

inline constexpr std::size_t game5_regular_gap_route_slices = [] {
    std::size_t slices = 0;
    for (const auto run : game5_regular_gap_route) slices += run.slices;
    return slices;
}();
static_assert(game5_regular_gap_route_slices == 1670);

// Release fire and hold ordinary neutral input through two complete active
// gap cycles, crossing retirement, and its following production update.
inline constexpr SessionInputRun game5_regular_gap_completion[]{
    {0,145,false},
};

} // namespace river_raid::test
