#pragma once

#include "app/auxiliary_contacts.hpp"
#include "app/player_contact.hpp"
#include "game/contact_sampling.hpp"
#include "game/player_shot.hpp"
#include "game/gap_contact.hpp"
#include "render/world_object_schedule.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace river_raid {

struct ForegroundContactSnapshot {
    std::array<ShotContactSlot, kShotContactSlotCount> shot{};
    PlayerContactSnapshot player;
    AuxiliaryContactSnapshot auxiliary;
    bool shot_presented{};
    GapContactSnapshot gap;

    friend bool operator==(const ForegroundContactSnapshot&,
                           const ForegroundContactSnapshot&) = default;
};

// Geometry ownership accompanying one working contact generation. Object
// indices remain only in ContactSamplingBuffer so row shifts can rebase them.
struct ContactPresentationMetadata {
    bool shot_presented{};
    bool player_straight{};
    bool player_fully_offscreen{};
    bool gap_presented{};
};

class ForegroundContactPipeline {
public:
    // Publish the prior working generation before current object motion.
    [[nodiscard]] ForegroundContactSnapshot publish() noexcept;
    // Generate current working contacts before rows and return the exact object
    // bands sampled. Top objects precede motion; later records include effects.
    [[nodiscard]] std::vector<WorldObjectBandSchedule> produce(
        const RiverWorld& world, const WorldObjectState& top_objects,
        std::uint8_t phase, const PlayerSteeringState& player, FlightPose pose,
        const PlayerShotResult& shot, LifeCycleImage image);
    [[nodiscard]] ContactSamplingBuffer& sampling_buffer() noexcept { return samples_; }
    void reset() noexcept;

private:
    ContactSamplingBuffer samples_;
    ContactPresentationMetadata working_{};
    ContactPresentationMetadata published_{};
};

// Isolated one-generation geometric sampler; FlightPreview uses the persistent
// pipeline above, not this convenience composition.
[[nodiscard]] ForegroundContactSnapshot sample_foreground_contacts(
    const RiverWorld& world, const PlayerSteeringState& player, FlightPose pose,
    const PlayerShotResult& shot, LifeCycleImage image = LifeCycleImage::Straight);

} // namespace river_raid
