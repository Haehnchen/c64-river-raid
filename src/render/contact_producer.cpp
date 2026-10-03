#include "render/contact_producer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace river_raid {
namespace {

using Wide = std::int64_t;

struct Bounds {
    Wide left{};
    Wide top{};
    Wide right{};
    Wide bottom{};
};

void validate_mask(const OccupancyMask& mask) {
    if (mask.width <= 0 || mask.height <= 0) {
        throw std::invalid_argument("Contact occupancy dimensions must be positive");
    }

    const auto width = static_cast<std::size_t>(mask.width);
    const auto height = static_cast<std::size_t>(mask.height);
    if (width > std::numeric_limits<std::size_t>::max() / height ||
        mask.pixels.size() != width * height) {
        throw std::invalid_argument("Malformed contact occupancy mask");
    }
    for (const auto pixel : mask.pixels) {
        if (pixel > 1) {
            throw std::invalid_argument("Contact occupancy mask is not binary");
        }
    }
}

void validate_placement(const MaskPlacement& placement) {
    if (placement.mask == nullptr) {
        throw std::invalid_argument("Contact placement has no occupancy mask");
    }
    validate_mask(*placement.mask);
}

[[nodiscard]] constexpr bool is_one_hot(std::uint8_t value) noexcept {
    return value != 0 && (value & static_cast<std::uint8_t>(value - 1)) == 0;
}

[[nodiscard]] Bounds placement_bounds(const MaskPlacement& placement) noexcept {
    return {
        placement.left,
        placement.top,
        static_cast<Wide>(placement.left) + placement.mask->width,
        static_cast<Wide>(placement.top) + placement.mask->height,
    };
}

[[nodiscard]] Bounds clip_bounds(MaskClip clip) noexcept {
    return {
        clip.left,
        clip.top,
        static_cast<Wide>(clip.left) + clip.width,
        static_cast<Wide>(clip.top) + clip.height,
    };
}

[[nodiscard]] Bounds intersect(Bounds first, Bounds second) noexcept {
    return {
        std::max(first.left, second.left),
        std::max(first.top, second.top),
        std::min(first.right, second.right),
        std::min(first.bottom, second.bottom),
    };
}

[[nodiscard]] bool empty(Bounds bounds) noexcept {
    return bounds.right <= bounds.left || bounds.bottom <= bounds.top;
}

[[nodiscard]] bool occupied_at(const MaskPlacement& placement,
                               Wide x, Wide y) noexcept {
    const auto local_x = static_cast<std::size_t>(x - placement.left);
    const auto local_y = static_cast<std::size_t>(y - placement.top);
    const auto index = local_y * static_cast<std::size_t>(placement.mask->width) +
                       local_x;
    return placement.mask->pixels[index] != 0;
}

[[nodiscard]] bool masks_touch(const MaskPlacement& first,
                               const MaskPlacement& second,
                               Bounds scan_bounds) noexcept {
    auto overlap = intersect(scan_bounds, placement_bounds(first));
    overlap = intersect(overlap, placement_bounds(second));
    if (empty(overlap)) return false;

    for (Wide y = overlap.top; y < overlap.bottom; ++y) {
        for (Wide x = overlap.left; x < overlap.right; ++x) {
            if (occupied_at(first, x, y) && occupied_at(second, x, y)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

void PixelContactAccumulator::scan(
    std::span<const ContactSpritePlacement> sprites,
    MaskPlacement terrain, MaskClip scan_region) {
    if (scan_region.width < 0 || scan_region.height < 0) {
        throw std::invalid_argument("Contact scan dimensions must be nonnegative");
    }
    validate_placement(terrain);

    std::uint8_t identities = 0;
    for (const auto& sprite : sprites) {
        validate_placement(sprite.occupancy);
        if (!is_one_hot(sprite.participant)) {
            throw std::invalid_argument("Contact participant must be one-hot");
        }
        if ((identities & sprite.participant) != 0) {
            throw std::invalid_argument("Duplicate contact participant");
        }
        identities = static_cast<std::uint8_t>(identities | sprite.participant);
    }

    // Work locally so every validation failure leaves both latches untouched.
    std::uint8_t object_participants = 0;
    std::uint8_t terrain_participants = 0;
    const auto scan_bounds = clip_bounds(scan_region);

    for (std::size_t first = 0; first < sprites.size(); ++first) {
        if (masks_touch(sprites[first].occupancy, terrain, scan_bounds)) {
            terrain_participants = static_cast<std::uint8_t>(
                terrain_participants | sprites[first].participant);
        }
        for (std::size_t second = first + 1; second < sprites.size(); ++second) {
            if (!masks_touch(sprites[first].occupancy,
                             sprites[second].occupancy, scan_bounds)) {
                continue;
            }
            object_participants = static_cast<std::uint8_t>(
                object_participants | sprites[first].participant |
                sprites[second].participant);
        }
    }

    pending_.object_participants = static_cast<std::uint8_t>(
        pending_.object_participants | object_participants);
    pending_.terrain_participants = static_cast<std::uint8_t>(
        pending_.terrain_participants | terrain_participants);
}

std::uint8_t PixelContactAccumulator::consume_object_participants() noexcept {
    const auto participants = pending_.object_participants;
    pending_.object_participants = 0;
    return participants;
}

std::uint8_t PixelContactAccumulator::consume_terrain_participants() noexcept {
    const auto participants = pending_.terrain_participants;
    pending_.terrain_participants = 0;
    return participants;
}

void PixelContactAccumulator::sample_into(ContactSamplingBuffer& buffer, ContactSectionIndex section,
                                          ContactSampleChannels channels) {
    if (section.value >= kShotContactSlotCount) throw std::out_of_range("Contact section index out of range");
    switch (channels) {
    case ContactSampleChannels::Both:
        buffer.replace_sample(section, pending_);
        pending_ = {};
        break;
    case ContactSampleChannels::Objects:
        buffer.replace_object_participants(section, consume_object_participants());
        break;
    case ContactSampleChannels::Terrain:
        buffer.replace_terrain_participants(section, consume_terrain_participants());
        break;
    default: throw std::invalid_argument("Invalid contact sample channels");
    }
}

void PixelContactAccumulator::publish_into(ContactSamplingBuffer& buffer) noexcept {
    buffer.rollover_for_foreground();
    pending_ = {};
}

} // namespace river_raid
