#pragma once

#include <array>
#include <cstdint>

namespace river_raid::assets {

inline constexpr std::array<std::uint32_t, 16> presentation_palette{
    0xff000000, 0xffffffff, 0xffab3c65, 0xff87f0cb,
    0xffb23def, 0xff5ed638, 0xff3a31ff, 0xffffff3b,
    0xffb76418, 0xff814c00, 0xffea79a3, 0xff626262,
    0xff949494, 0xffb2ff8d, 0xff8178ff, 0xffcdcdcd,
};

} // namespace river_raid::assets
