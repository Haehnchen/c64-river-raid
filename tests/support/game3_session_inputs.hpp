#pragma once

#include "support/long_session_inputs.hpp"

namespace river_raid::test {

// Native pilot output, not original-game controls. Replay after normal Game 3
// startup and the 84-tick reset-work launch, without phase-alignment waits.
inline constexpr SessionInputRun game3_bridge_route[]{
    {0,48,true},{2,16,true},{1,16,true},{2,16,true},
    {1,16,false},{1,16,true},{2,16,false},{2,16,true},
    {0,96,true},{1,16,true},{0,48,true},{2,16,true},
    {0,16,true},{1,32,true},{0,32,true},{2,16,false},
    {2,16,true},{0,112,true},{2,16,true},{0,16,true},
    {2,16,true},{1,16,false},{1,16,true},{2,16,true},
    {0,16,true},{1,16,true},{0,16,true},{1,16,true},
    {2,32,true},{1,32,true},{0,16,true},{2,32,true},
    {0,48,true},{1,16,true},{0,8,true},
};

} // namespace river_raid::test
