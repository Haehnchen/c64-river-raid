#pragma once

#include "game/river_generator.hpp"
#include "game/river_row.hpp"
#include "game/world_objects.hpp"

#include <cstdint>
#include <optional>

namespace river_raid {

enum class BridgeSpriteImage : std::uint8_t {
    CrossingRightA,
    CrossingRightB,
    CrossingLeftA,
    CrossingLeftB,
    GapFlight,
    GapExplosion0,
    GapExplosion1,
    GapExplosion2,
    GapExplosion3,
    GapExplosion4,
    JointExplosionA,
    JointExplosionB,
};

// Coordinates use VIC pixels. The native 320x200 viewport origin is (24, 50).
struct BridgeSpritePresentation {
    BridgeSpriteImage image{};
    std::uint16_t vic_x{};
    std::uint8_t vic_y{};
    std::uint8_t color{};

    friend bool operator==(const BridgeSpritePresentation&,
                           const BridgeSpritePresentation&) = default;
};

struct BridgeSpriteRowInput {
    const RiverGeneratorState& river;
    const WorldObjectState& objects;
    RiverGuides guides;
};

struct BridgeSpriteTick {
    std::uint8_t animation_phase{};
    bool round_active{};
    bool bridge_progressed{};
    bool demo_mode{};
};

struct BridgeSpriteDestruction {
    bool joint_contact{};
    // Source comparison occurs after the displayed bridge number is incremented.
    std::uint16_t displayed_bridge_number{};
};

struct BridgeSpriteDestructionResult {
    bool joint_explosion{};
    bool gap_triggered{};

    friend bool operator==(const BridgeSpriteDestructionResult&,
                           const BridgeSpriteDestructionResult&) = default;
};

struct BridgeSpriteState {
    std::optional<BridgeSpritePresentation> crossing;
    std::optional<BridgeSpritePresentation> gap;
    std::uint8_t gap_animation_counter{0xff};
    std::uint8_t target_horizontal_coordinate{};
    // Current normal crossing's entry rule, not the cause of an active gap.
    bool automatic_entry_enabled{};

    friend bool operator==(const BridgeSpriteState&, const BridgeSpriteState&) = default;
};

class BridgeSpriteSystem {
public:
    // Scroll stored coordinates, then allow a new crossing. Neither operation
    // changes the published sprites until their foreground update.
    [[nodiscard]] bool advance_after_row(const BridgeSpriteRowInput& input);
    void advance_tick(const BridgeSpriteTick& tick);
    void advance_crossing(const BridgeSpriteTick& tick);
    void advance_gap(const BridgeSpriteTick& tick);
    [[nodiscard]] BridgeSpriteDestructionResult bridge_destroyed(
        const BridgeSpriteDestruction& destruction);
    void retire_crossing_for_pending_life_end() noexcept;

    [[nodiscard]] BridgeSpriteState state() const noexcept;
    // Sprite optionals are last programmed; counters and target remain current
    // mechanics. Row scrolling and contact effects do not publish sprites.
    [[nodiscard]] BridgeSpriteState presentation_state() const noexcept;
    void hide_for_life_reveal() noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] bool spawn(const BridgeSpriteRowInput& input);
    void publish_crossing() noexcept;
    void publish_gap() noexcept;

    bool crossing_active_{};
    BridgeSpriteImage crossing_image_{BridgeSpriteImage::CrossingRightA};
    std::uint8_t crossing_horizontal_coordinate_{};
    std::uint8_t crossing_vertical_position_{};

    bool gap_active_{};
    BridgeSpriteImage gap_image_{BridgeSpriteImage::GapFlight};
    std::uint8_t gap_horizontal_coordinate_{};
    std::uint8_t gap_horizontal_fraction_{};
    std::uint8_t gap_vertical_position_{};

    std::uint8_t horizontal_step_{};
    std::uint8_t horizontal_fraction_step_{};
    std::uint8_t target_horizontal_coordinate_{};
    std::uint8_t gap_animation_counter_{0xff};
    bool trigger_gap_at_entry_{};

    std::optional<BridgeSpritePresentation> presented_crossing_;
    std::optional<BridgeSpritePresentation> presented_gap_;
};

} // namespace river_raid
