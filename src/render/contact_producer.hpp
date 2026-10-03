#pragma once

#include "game/contact_sampling.hpp"
#include "render/contact_masks.hpp"

#include <cstdint>
#include <span>

namespace river_raid {

// Places one already classified occupancy mask in scan coordinates.
struct MaskPlacement {
    const OccupancyMask* mask{};
    int left{};
    int top{};
};

// participant is one of the eight one-hot sprite identities.
struct ContactSpritePlacement {
    MaskPlacement occupancy;
    std::uint8_t participant{};
};

enum class ContactSampleChannels { Both, Objects, Terrain };

// Accumulates the two global participant latches produced by exact pixel
// contact.  Geometry and scan timing remain explicit responsibilities of the
// caller.
class PixelContactAccumulator {
public:
    void scan(std::span<const ContactSpritePlacement> sprites,
              MaskPlacement terrain, MaskClip scan_region);

    [[nodiscard]] ContactSectionSample pending_sample() const noexcept {
        return pending_;
    }

    // Each read clears only its own channel, like separate latch reads.
    [[nodiscard]] std::uint8_t consume_object_participants() noexcept;
    [[nodiscard]] std::uint8_t consume_terrain_participants() noexcept;

    // Invalid destinations leave both latches and the buffer unchanged.
    void sample_into(ContactSamplingBuffer& buffer, ContactSectionIndex section,
                     ContactSampleChannels channels = ContactSampleChannels::Both);
    // Publishes recorded sections and discards unsampled contacts.
    void publish_into(ContactSamplingBuffer& buffer) noexcept;

private:
    ContactSectionSample pending_{};
};

} // namespace river_raid
