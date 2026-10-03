#pragma once

#include <cstddef>
#include <cstdint>

namespace river_raid::test {

enum class Game7Vertical : std::uint8_t { Neutral, Up, Down };

struct Game7InputRun {
    int direction{}; // 0 neutral, 1 right, 2 left
    int slices{};    // held 20-ms elapsed slices
    bool fire{};
    Game7Vertical vertical{Game7Vertical::Neutral};
};

// Native ordinary-control search witness. This is not a source input trace.
inline constexpr Game7InputRun game7_first_bridge_route[]{
    {0,16,true,Game7Vertical::Neutral},
    {0,16,false,Game7Vertical::Neutral},
    {0,16,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Up},
    {2,16,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Up},
    {2,16,true,Game7Vertical::Up},
    {0,32,true,Game7Vertical::Up},
    {2,32,true,Game7Vertical::Up},
    {0,16,true,Game7Vertical::Up},
    {2,32,true,Game7Vertical::Up},
    {0,32,true,Game7Vertical::Up},
    {2,16,true,Game7Vertical::Up},
    {0,64,true,Game7Vertical::Up},
    {0,16,true,Game7Vertical::Neutral},
    {1,32,true,Game7Vertical::Up},
    {0,16,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Up},
    {0,16,true,Game7Vertical::Up},
    {2,16,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Up},
    {0,32,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Up},
    {1,16,true,Game7Vertical::Neutral},
    {0,3,true,Game7Vertical::Neutral},
};

inline constexpr std::size_t game7_first_bridge_route_slices = [] {
    std::size_t slices = 0;
    for (const auto run : game7_first_bridge_route) slices += run.slices;
    return slices;
}();
static_assert(game7_first_bridge_route_slices == 531);

} // namespace river_raid::test
