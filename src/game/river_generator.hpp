#pragma once

#include "game/river_row.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace river_raid {

enum class RiverChannelMode {
    Single,
    PreparingSplit,
    SplitOpening,
    SplitClosing,
};

enum class RiverBankSide { Left, Right };

enum class RiverCheckpointSelection {
    Current,
    Advanced,
    Previous,
};

struct RiverCheckpointRestore {
    RiverCheckpointSelection selection{RiverCheckpointSelection::Current};
    std::uint8_t player_horizontal_position{};

    friend bool operator==(const RiverCheckpointRestore&, const RiverCheckpointRestore&) = default;
};

struct RiverRandomState {
    std::uint8_t low{};
    std::uint8_t middle{};
    std::uint8_t high{};

    friend bool operator==(const RiverRandomState&, const RiverRandomState&) = default;
};

struct RiverSectionAnchor {
    RiverRandomState random{};
    std::uint8_t course{};

    friend bool operator==(const RiverSectionAnchor&, const RiverSectionAnchor&) = default;
};

struct RiverCheckpoint {
    std::uint8_t section{};
    RiverSectionAnchor anchor{};

    friend bool operator==(const RiverCheckpoint&, const RiverCheckpoint&) = default;
};

struct TerrainStampPattern {
    std::uint8_t id{};
    std::uint8_t width{};
    std::uint8_t height{};
    std::span<const std::uint8_t> pixels;
};

struct TerrainFeatureChoice {
    std::uint8_t margin{};
    std::uint8_t stamp_id{};
};

struct RiverGeneratorTables {
    std::span<const std::int8_t, 32> scripted_course_deltas;
    std::span<const std::uint8_t, 32> bridge_lengths;
    std::span<const TerrainFeatureChoice, 26> regular_features;
    std::span<const TerrainFeatureChoice, 8> early_section_features;
    std::span<const TerrainStampPattern> stamps;
};

struct RiverObjectSample {
    std::uint8_t kind{};
    std::uint8_t progress{};
    std::uint8_t horizontal_coordinate{};
    bool extended_clearance{};
};

struct RiverObjectHistory {
    std::array<RiverObjectSample, 12> entries{};
    std::uint8_t cursor{};
    bool split_collision_wrap{};
};

struct TerrainFeatureState {
    bool active{};
    std::uint8_t stamp_id{};
    std::uint8_t row_index{};
    std::uint8_t margin{};
    RiverBankSide side{RiverBankSide::Right};

    friend bool operator==(const TerrainFeatureState&, const TerrainFeatureState&) = default;
};

struct RiverGeneratorState {
    std::uint8_t phase{};
    std::uint8_t scroll_buffer{};
    std::uint8_t section{};
    std::uint8_t course{};
    std::uint8_t channel_span{};
    std::uint8_t bank_inset{};
    RiverRandomState random{};

    bool scripted_course{};
    RiverChannelMode channel_mode{RiverChannelMode::Single};
    bool section_transition_pending{};
    std::uint8_t transition_bias{};

    std::uint8_t target_width_units{};
    std::uint8_t previous_target_width_units{};
    std::uint8_t width_interval{};
    std::uint8_t section_interval{};
    std::uint8_t target_bank_inset{};
    std::int8_t bank_inset_step{};
    std::int8_t course_step{};

    std::uint8_t bridge_rows_remaining{};
    TerrainFeatureState feature{};
    RiverSectionAnchor section_anchor{};
    RiverSectionAnchor previous_section_anchor{};

    friend bool operator==(const RiverGeneratorState&, const RiverGeneratorState&) = default;
};

class RiverGenerator {
public:
    RiverGenerator(RiverGeneratorState initial_state, RiverGeneratorTables tables);

    [[nodiscard]] RiverRowInput advance();
    [[nodiscard]] RiverRowInput advance(const RiverObjectHistory& objects);
    [[nodiscard]] const RiverGeneratorState& state() const noexcept;
    void begin_life() noexcept;
    void mark_bridge_destroyed() noexcept;
    void set_checkpoint(RiverCheckpoint checkpoint) noexcept;
    [[nodiscard]] RiverCheckpoint selected_checkpoint() const noexcept;
    [[nodiscard]] RiverCheckpoint commit_checkpoint() noexcept;
    RiverCheckpointRestore restore_checkpoint() noexcept;
    RiverCheckpointRestore restore_checkpoint(RiverCheckpoint checkpoint) noexcept;
    void reset() noexcept;

private:
    struct CheckpointChoice {
        RiverCheckpointSelection selection{RiverCheckpointSelection::Current};
        RiverCheckpoint checkpoint{};
        RiverSectionAnchor previous_section_anchor{};
    };

    [[nodiscard]] CheckpointChoice checkpoint_choice() const noexcept;
    RiverCheckpoint commit_checkpoint_choice(const CheckpointChoice& choice) noexcept;
    [[nodiscard]] const TerrainStampPattern& stamp(std::uint8_t id) const;

    RiverGeneratorState initial_state_;
    RiverGeneratorState state_;
    RiverGeneratorTables tables_;
};

[[nodiscard]] bool is_split_channel(RiverChannelMode mode) noexcept;

} // namespace river_raid
