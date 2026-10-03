#pragma once

#include "game/river_generator.hpp"
#include "game/river_row.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace river_raid {

inline constexpr std::size_t kWorldObjectCapacity = 12;
inline constexpr std::size_t kWorldObjectKindCount = 16;

struct WorldObjectRecord {
    std::uint8_t vertical_position{};
    std::uint8_t horizontal_coordinate{};
    std::uint8_t behavior_flags{};
    std::uint8_t animated_kind{};
    std::uint8_t base_kind{};
    std::uint8_t left_bound{};
    std::uint8_t right_bound{};
    std::uint8_t animation_count{};

    friend bool operator==(const WorldObjectRecord&, const WorldObjectRecord&) = default;
};

struct WorldObjectState {
    std::array<WorldObjectRecord, kWorldObjectCapacity> records{};
    std::uint8_t active_begin{kWorldObjectCapacity};
    std::uint8_t generator_cursor{kWorldObjectCapacity};
    std::uint8_t special_cursor{kWorldObjectCapacity};
    std::uint8_t spawn_cooldown{};
    std::uint8_t spawn_behavior_flags{};
    bool alternating_update{};
    bool split_collision_wrap{};

    friend bool operator==(const WorldObjectState&, const WorldObjectState&) = default;
};

struct WorldObjectTables {
    std::span<const std::uint8_t, kWorldObjectKindCount> spawn_delay_by_kind;
    std::span<const std::uint8_t, 8> random_kind_choices;
    std::span<const std::uint8_t, kWorldObjectKindCount> kind_clearance;
};

struct WorldObjectMotionTick {
    std::uint8_t animation_phase{};
    bool movement_enabled{};
    std::optional<std::uint8_t> activation_candidate{};
};

enum class WorldObjectHitSlot { Primary, Alternate };

struct WorldObjectHitEffect {
    std::size_t record_index{};
    std::uint8_t destroyed_kind{};
    std::optional<std::uint8_t> score_kind;
    std::uint8_t sound_countdown{};
};

struct GapContactHitEffect {
    std::size_t record_index{};
    std::uint8_t destroyed_kind{};

    friend bool operator==(const GapContactHitEffect&, const GapContactHitEffect&) = default;
};

struct WorldObjectRowResult {
    bool history_shifted{};
    bool auxiliary_selected{};
};

class WorldObjectHistory {
public:
    WorldObjectHistory(WorldObjectState initial_state, WorldObjectTables tables);

    void advance_motion(const WorldObjectMotionTick& tick);
    WorldObjectRowResult advance_after_row(const RiverGeneratorState& river,
                                           const RiverGuides& guides);
    // Mutates the contacted object; round progression belongs to the caller.
    [[nodiscard]] std::optional<WorldObjectHitEffect> apply_projectile_hit(
        std::size_t record_index, WorldObjectHitSlot slot, bool round_event_pending);
    [[nodiscard]] std::optional<GapContactHitEffect> apply_gap_contact_hit(
        std::size_t record_index, WorldObjectHitSlot slot);
    [[nodiscard]] RiverObjectHistory generator_view() const noexcept;
    [[nodiscard]] const WorldObjectState& state() const noexcept;
    void reset() noexcept;

private:
    bool spawn(const RiverGeneratorState& river, const RiverGuides& guides);

    WorldObjectState initial_state_;
    WorldObjectState state_;
    WorldObjectTables tables_;
};

} // namespace river_raid
