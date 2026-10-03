#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace river_raid {

// A binary collision mask.  Values are deliberately not palette indices:
// black/opaque pixels must remain distinguishable from transparent pixels.
struct OccupancyMask {
    int width{};
    int height{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool occupied_at(int x, int y) const noexcept;
};

enum class SpriteMode : std::uint8_t {
    Monochrome,
    Multicolor,
};

// The input is one already decoded role per logical pixel.  Monochrome uses
// roles 0/1.  Multicolor uses C64 pair roles 0..3; role 0 (00) is transparent
// while roles 1 (01), 2 (10), and 3 (11) occupy the sprite collision mask.
// Terrain/background contact has different semantics and is decoded by
// make_terrain_occupancy_mask().
[[nodiscard]] OccupancyMask make_sprite_occupancy_mask(
    int width, int height, std::span<const std::uint8_t> roles,
    SpriteMode mode, bool expand_horizontal = false);

// Convert a final indexed image (where 255 is the renderer's transparent
// sentinel) to occupancy.  This is suitable for assets that contain only a
// transparent role and one or more opaque roles (for example the player and
// projectile assets).  A multicolor role image should use the function above
// so its pair roles remain available to the VIC-II-specific collision
// classification.
[[nodiscard]] OccupancyMask make_indexed_occupancy_mask(
    int width, int height, std::span<const std::uint8_t> pixels,
    std::uint8_t transparent = 255);

// Decode packed C64 multicolor terrain rows.  Each byte contains four
// two-bit pixels, and each pair expands to two native horizontal pixels.  By
// VIC-II collision semantics only pair roles 2 and 3 occupy the mask.
// packed_width is the number of packed bytes in each row (40 for the native
// 320-pixel river rows).
[[nodiscard]] OccupancyMask make_terrain_occupancy_mask(
    std::span<const std::uint8_t> packed_rows, int packed_width);

struct MaskClip {
    int left{};
    int top{};
    int width{};
    int height{};
};

// OR a source mask into a destination mask at (left, top), restricted to the
// destination and clip rectangle.  This is the mask equivalent of the
// renderer's transparent/clipped sprite placement and performs no palette or
// RGB conversion.
void stamp_occupancy_mask(OccupancyMask& destination,
                          const OccupancyMask& source, int left, int top,
                          MaskClip clip);

} // namespace river_raid
