#pragma once

namespace river_raid::test {

// One ordinary input held for this many 20-ms elapsed slices. A slice may
// admit more than one native update. Directions: 0 neutral,
// 1 right, 2 left. The recorded prefix holds fire throughout.
struct SessionInputRun {
    int direction;
    int slices;
    bool fire;
};

// Replays from the normal first-option start to the first shot bridge clear.
// Kept separate from the search suffix so either part can be checked alone.
inline constexpr SessionInputRun first_bridge_session_prefix[]{
    {0,19,true},{2,1,true},{0,13,true},{1,8,true},{0,7,true},{2,1,true},
    {0,5,true},{2,10,true},{0,49,true},{1,11,true},{0,9,true},{1,6,true},
    {0,4,true},{2,11,true},{0,6,true},{1,6,true},{0,3,true},{2,1,true},
    {0,9,true},{1,6,true},{0,17,true},{2,1,true},{0,18,true},{1,14,true},
    {0,22,true},{2,1,true},{0,2,true},{2,1,true},{0,1,true},{2,1,true},
    {0,8,true},{1,8,true},{0,5,true},{2,12,true},{0,8,true},{1,6,true},
    {0,5,true},{1,13,true},{0,36,true},{2,1,true},{0,12,true},{1,5,true},
    {0,24,true},{2,1,true},{0,1,true},{2,1,true},{0,3,true},{2,1,true},
    {0,5,true},{2,1,true},{0,4,true},{2,1,true},{0,6,true},{1,10,true},
    {0,2,true},{1,6,true},{0,7,true},{1,6,true},{0,6,true},{1,1,true},
    {0,8,true},{2,1,true},{0,6,true},{1,8,true},{0,27,true},{2,1,true},
    {0,1,true},{2,1,true},{0,3,true},{2,1,true},
    {0,1,true},{2,1,true},{0,1,true},{2,1,true},{0,8,true},{1,8,true},
    {0,3,true},{2,1,true},{0,2,true},{2,1,true},{0,2,true},{2,1,true},
    {0,40,true},{1,14,true},{0,30,true},{2,1,true},{0,1,true},{2,1,true},
    {0,3,true},{2,1,true},{0,7,true},{1,8,true},{0,6,true},{2,11,true},
    {0,13,true},{1,13,true},{0,10,true},{2,1,true},{0,7,true},{1,8,true},
    {0,5,true},{2,11,true},{0,14,true},{1,10,true},{0,3,true},{2,1,true},
    {0,3,true},{2,8,true},{0,13,true},{1,13,true},{0,8,true},{2,1,true},
    {0,1,true},{2,1,true},{0,7,true},{2,10,true},{0,31,true},{1,5,true},
    {0,12,true},
};

// Fixed ordinary-input continuation from the first bridge to the second shot
// clear, without a life loss, after the presentation-only reset interval.
inline constexpr SessionInputRun second_bridge_session_suffix[]{
    {1,16,true},{2,16,true},{0,32,true},{1,32,true},{2,16,true},{0,112,true},
    {1,16,true},{2,48,true},{0,32,true},{2,16,true},{1,32,true},{2,16,true},
    {0,16,true},{1,16,true},{0,16,true},{1,16,true},{0,16,true},{1,16,true},
    {0,48,true},{2,16,true},{1,16,true},{0,48,true},{1,16,true},{0,80,true},
    {2,16,true},{0,160,true},{2,48,true},{0,144,true},{1,48,true},{0,9,true},
};

// Ordinary continuation from bridge three to dry-tank entry in the same life.
inline constexpr SessionInputRun fuel_exhaustion_session_suffix[]{
    {2,16,true},{1,16,true},{0,16,true},{2,16,true},{1,16,true},{2,16,true},
    {0,5,true},
};

} // namespace river_raid::test
