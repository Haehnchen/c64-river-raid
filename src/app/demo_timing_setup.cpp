#include "app/demo_timing_setup.hpp"
#include "app/demo_setup.hpp"
#include "regional_timing.hpp"

#include <stdexcept>

namespace river_raid {

FrameRate demo_frame_rate(DemoRegion region) {
    namespace data = assets::regional_timing;
    switch (region) {
    case DemoRegion::Pal: return {data::pal_ticks_numerator, data::pal_ticks_denominator};
    case DemoRegion::Ntsc: return {data::ntsc_ticks_numerator, data::ntsc_ticks_denominator};
    }
    throw std::invalid_argument("Unknown demo timing region");
}

TimedDemo make_timed_demo(DemoRegion region) {
    return {make_demo_simulation(), FramePacer(demo_frame_rate(region))};
}

std::chrono::nanoseconds title_delay_duration(DemoRegion region) {
    // Instruction-count approximation; excludes setup, IRQ work, and VIC stalls.
    constexpr std::uint64_t delay_cycles = 12'149'919;
    return std::chrono::nanoseconds{static_cast<std::int64_t>(
        delay_cycles * 1'000'000'000ULL / demo_frame_rate(region).numerator)};
}

} // namespace river_raid
