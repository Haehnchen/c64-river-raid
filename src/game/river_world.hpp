#pragma once

#include "game/river_viewport.hpp"
#include "game/world_objects.hpp"
#include "game/bridge_sprites.hpp"
#include "game/auxiliary_projectile.hpp"

namespace river_raid {

class ContactSamplingBuffer;

class RiverWorld {
public:
    RiverWorld(RiverGenerator generator, WorldObjectHistory objects,
               std::span<const std::uint8_t, 32> edge_patterns);

    [[nodiscard]] PackedRiverRow advance();
    [[nodiscard]] PackedRiverRow advance(ContactSamplingBuffer& contacts);
    void advance_motion(const WorldObjectMotionTick& tick);
    void advance_bridge_sprites(std::uint8_t animation_phase, bool round_active,
                                bool demo_mode = false);
    void advance_bridge_crossing(std::uint8_t animation_phase, bool round_active,
                                 bool demo_mode = false);
    void advance_bridge_gap(std::uint8_t animation_phase, bool round_active);
    void apply_bridge_sprite_destruction(const BridgeSpriteDestruction& destruction);
    void retire_bridge_crossing_for_pending_life_end() noexcept;
    [[nodiscard]] BridgeSpriteState bridge_sprite_state() const noexcept;
    [[nodiscard]] BridgeSpriteState bridge_presentation_state() const noexcept;
    [[nodiscard]] const AuxiliaryProjectileState& auxiliary_projectile_state() const noexcept;
    [[nodiscard]] AuxiliaryProjectileTickResult advance_auxiliary_projectile(
        bool lifecycle_active, bool demo_mode, bool audio_effect_idle,
        const std::array<std::uint8_t, 6>& terrain_contacts = {},
        const std::array<std::uint8_t, 2>& player_contacts = {});
    void hide_life_transients() noexcept;
    [[nodiscard]] std::optional<WorldObjectHitEffect> apply_projectile_hit(
        std::size_t record_index, WorldObjectHitSlot slot, bool round_event_pending);
    [[nodiscard]] std::optional<GapContactHitEffect> apply_gap_contact_hit(
        std::size_t record_index, WorldObjectHitSlot slot);
    void begin_life() noexcept;
    void mark_bridge_destroyed() noexcept;
    void set_checkpoint(RiverCheckpoint checkpoint) noexcept;
    [[nodiscard]] RiverCheckpoint selected_checkpoint() const noexcept;
    [[nodiscard]] RiverCheckpoint commit_checkpoint() noexcept;
    RiverCheckpointRestore restore_checkpoint() noexcept;
    RiverCheckpointRestore restore_checkpoint(RiverCheckpoint checkpoint) noexcept;
    void reset() noexcept;
    [[nodiscard]] const RiverGeneratorState& river_state() const noexcept;
    [[nodiscard]] const WorldObjectState& object_state() const noexcept;
    [[nodiscard]] const RiverViewport& viewport() const noexcept;
    [[nodiscard]] std::size_t generated_rows() const noexcept;

private:
    [[nodiscard]] PackedRiverRow advance_row(ContactSamplingBuffer* contacts);
    void clear_checkpoint_state() noexcept;
    RiverGenerator generator_;
    WorldObjectHistory objects_;
    BridgeSpriteSystem bridge_sprites_;
    AuxiliaryProjectileSystem auxiliary_projectile_;
    std::span<const std::uint8_t, 32> edge_patterns_;
    RiverGuides guides_{};
    RiverViewport viewport_;
    std::size_t generated_rows_{};
};

} // namespace river_raid
