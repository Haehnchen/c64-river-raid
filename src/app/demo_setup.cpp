#include "app/demo_setup.hpp"
#include "app/world_setup.hpp"
#include "demo_clock_data.hpp"

namespace river_raid {

DemoSimulation make_demo_simulation() {
    namespace data = assets::demo_clock;
    return {make_river_world(),
            DemoClock{{data::initial_animation_phase, data::initial_startup_ticks,
                       data::initial_scroll_speed, data::initial_scroll_fraction},
                      {data::startup_scroll_speed, data::steady_scroll_speed}},
            data::initial_terrain_rows};
}

} // namespace river_raid
