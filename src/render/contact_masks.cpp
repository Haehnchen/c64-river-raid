#include "render/contact_masks.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace river_raid {
namespace {

using Wide = std::int64_t;

[[nodiscard]] std::size_t checked_area(int width, int height,
                                        const char* message) {
    if (width <= 0 || height <= 0) throw std::invalid_argument(message);
    const auto wide_width = static_cast<std::size_t>(width);
    const auto wide_height = static_cast<std::size_t>(height);
    if (wide_width > std::numeric_limits<std::size_t>::max() / wide_height) {
        throw std::invalid_argument(message);
    }
    return wide_width * wide_height;
}

void validate_mask(const OccupancyMask& mask, const char* message) {
    const auto area = checked_area(mask.width, mask.height, message);
    if (mask.pixels.size() != area) throw std::invalid_argument(message);
    for (const auto value : mask.pixels) {
        if (value > 1) throw std::invalid_argument(message);
    }
}

[[nodiscard]] bool role_occupies(std::uint8_t role, SpriteMode mode) {
    switch (mode) {
    case SpriteMode::Monochrome:
        if (role > 1) throw std::invalid_argument("Invalid monochrome sprite role");
        return role != 0;
    case SpriteMode::Multicolor:
        if (role > 3) throw std::invalid_argument("Invalid multicolor sprite role");
        // Sprite/sprite collision uses every visible multicolor pair. Terrain
        // contact has separate foreground semantics in
        // make_terrain_occupancy_mask().
        return role != 0;
    }
    throw std::invalid_argument("Invalid sprite mode");
}

} // namespace

bool OccupancyMask::occupied_at(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x);
    return index < pixels.size() && pixels[index] != 0;
}

OccupancyMask make_sprite_occupancy_mask(
    int width, int height, std::span<const std::uint8_t> roles,
    SpriteMode mode, bool expand_horizontal) {
    const auto input_area = checked_area(width, height,
                                         "Sprite mask dimensions must be positive");
    if (roles.size() != input_area) {
        throw std::invalid_argument("Sprite role span has wrong dimensions");
    }
    if (expand_horizontal && width > std::numeric_limits<int>::max() / 2) {
        throw std::invalid_argument("Expanded sprite mask is too wide");
    }

    const auto output_width = expand_horizontal ? width * 2 : width;
    const auto output_area = checked_area(output_width, height,
                                          "Sprite mask is too large");
    OccupancyMask result{output_width, height, {}};
    result.pixels.reserve(output_area);
    for (const auto role : roles) {
        const auto value = static_cast<std::uint8_t>(role_occupies(role, mode));
        result.pixels.push_back(value);
        if (expand_horizontal) result.pixels.push_back(value);
    }
    return result;
}

OccupancyMask make_indexed_occupancy_mask(
    int width, int height, std::span<const std::uint8_t> pixels,
    std::uint8_t transparent) {
    const auto area = checked_area(width, height,
                                   "Indexed mask dimensions must be positive");
    if (pixels.size() != area) {
        throw std::invalid_argument("Indexed pixel span has wrong dimensions");
    }
    OccupancyMask result{width, height, {}};
    result.pixels.reserve(area);
    for (const auto pixel : pixels) {
        result.pixels.push_back(static_cast<std::uint8_t>(pixel != transparent));
    }
    return result;
}

OccupancyMask make_terrain_occupancy_mask(
    std::span<const std::uint8_t> packed_rows, int packed_width) {
    if (packed_width <= 0) {
        throw std::invalid_argument("Terrain packed width must be positive");
    }
    const auto row_width = static_cast<std::size_t>(packed_width);
    if (packed_rows.size() % row_width != 0) {
        throw std::invalid_argument("Terrain rows are not a whole number of rows");
    }
    if (packed_width > std::numeric_limits<int>::max() / 8) {
        throw std::invalid_argument("Terrain mask is too wide");
    }
    const auto width = packed_width * 8;
    const auto height_size = packed_rows.size() / row_width;
    if (height_size == 0 ||
        height_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Terrain mask has invalid height");
    }
    const auto height = static_cast<int>(height_size);
    const auto area = checked_area(width, height, "Terrain mask is too large");
    OccupancyMask result{width, height, std::vector<std::uint8_t>(area, 0)};

    std::size_t output = 0;
    for (const auto packed : packed_rows) {
        for (int pair = 0; pair < 4; ++pair) {
            const auto role = static_cast<std::uint8_t>((packed >> (6 - pair * 2)) & 3);
            const auto value = static_cast<std::uint8_t>(role >= 2);
            result.pixels[output++] = value;
            result.pixels[output++] = value;
        }
    }
    return result;
}

void stamp_occupancy_mask(OccupancyMask& destination,
                          const OccupancyMask& source, int left, int top,
                          MaskClip clip) {
    validate_mask(destination, "Malformed destination occupancy mask");
    validate_mask(source, "Malformed source occupancy mask");
    if (clip.width < 0 || clip.height < 0) {
        throw std::invalid_argument("Mask clip dimensions must be nonnegative");
    }

    const Wide destination_left = 0;
    const Wide destination_top = 0;
    const Wide destination_right = destination.width;
    const Wide destination_bottom = destination.height;
    const Wide clip_left = std::clamp<Wide>(clip.left, destination_left,
                                            destination_right);
    const Wide clip_top = std::clamp<Wide>(clip.top, destination_top,
                                           destination_bottom);
    const Wide clip_right = std::clamp<Wide>(static_cast<Wide>(clip.left) + clip.width,
                                             destination_left, destination_right);
    const Wide clip_bottom = std::clamp<Wide>(static_cast<Wide>(clip.top) + clip.height,
                                              destination_top, destination_bottom);
    if (clip_right <= clip_left || clip_bottom <= clip_top) return;

    const Wide source_left = std::max<Wide>(clip_left, left);
    const Wide source_top = std::max<Wide>(clip_top, top);
    const Wide source_right = std::min<Wide>(clip_right,
                                             static_cast<Wide>(left) + source.width);
    const Wide source_bottom = std::min<Wide>(clip_bottom,
                                              static_cast<Wide>(top) + source.height);
    if (source_right <= source_left || source_bottom <= source_top) return;

    for (Wide y = source_top; y < source_bottom; ++y) {
        for (Wide x = source_left; x < source_right; ++x) {
            const auto source_x = static_cast<std::size_t>(x - left);
            const auto source_y = static_cast<std::size_t>(y - top);
            const auto source_index = source_y * static_cast<std::size_t>(source.width) + source_x;
            if (source.pixels[source_index] == 0) continue;
            const auto destination_index = static_cast<std::size_t>(y) *
                                               static_cast<std::size_t>(destination.width) +
                                           static_cast<std::size_t>(x);
            destination.pixels[destination_index] = 1;
        }
    }
}

} // namespace river_raid
