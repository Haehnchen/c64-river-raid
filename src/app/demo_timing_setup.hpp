#pragma once

#include "game/timed_demo.hpp"

namespace river_raid {

enum class DemoRegion { Pal, Ntsc };

[[nodiscard]] FrameRate demo_frame_rate(DemoRegion region);
[[nodiscard]] std::chrono::nanoseconds title_delay_duration(DemoRegion region);
[[nodiscard]] TimedDemo make_timed_demo(DemoRegion region);

} // namespace river_raid
