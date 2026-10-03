#include "game/river_world.hpp"
#include "game/contact_sampling.hpp"

#include <utility>

namespace river_raid {

RiverWorld::RiverWorld(RiverGenerator generator, WorldObjectHistory objects,
                       std::span<const std::uint8_t, 32> edge_patterns)
    : generator_(std::move(generator)), objects_(std::move(objects)), edge_patterns_(edge_patterns) {}

PackedRiverRow RiverWorld::advance() {
    return advance_row(nullptr);
}

PackedRiverRow RiverWorld::advance(ContactSamplingBuffer& contacts) {
    return advance_row(&contacts);
}

PackedRiverRow RiverWorld::advance_row(ContactSamplingBuffer* contacts) {
    const auto input = generator_.advance(objects_.generator_view());
    const auto row = compose_river_row(input, edge_patterns_);
    if (generator_.state().phase % 16 == 0) guides_ = river_guides(input);
    const auto object_row = objects_.advance_after_row(generator_.state(), guides_);
    auxiliary_projectile_.advance_after_row(object_row.auxiliary_selected
        ? std::optional{AuxiliaryProjectileSpawn{generator_.state().phase}} : std::nullopt);
    (void)bridge_sprites_.advance_after_row({generator_.state(), objects_.state(), guides_});
    if (object_row.history_shifted && contacts) contacts->shift_object_mappings();
    viewport_.push(row);
    ++generated_rows_;
    return row;
}

void RiverWorld::advance_motion(const WorldObjectMotionTick& tick) {
    objects_.advance_motion(tick);
}

void RiverWorld::advance_bridge_sprites(std::uint8_t animation_phase, bool round_active,
                                        bool demo_mode) {
    advance_bridge_crossing(animation_phase, round_active, demo_mode);
    advance_bridge_gap(animation_phase, round_active);
}

void RiverWorld::advance_bridge_crossing(std::uint8_t animation_phase, bool round_active,
                                         bool demo_mode) {
    bridge_sprites_.advance_crossing({animation_phase, round_active,
                                     generator_.state().section_transition_pending, demo_mode});
}

void RiverWorld::advance_bridge_gap(std::uint8_t animation_phase, bool round_active) {
    bridge_sprites_.advance_gap({animation_phase, round_active,
                                generator_.state().section_transition_pending});
}

void RiverWorld::apply_bridge_sprite_destruction(const BridgeSpriteDestruction& destruction) {
    (void)bridge_sprites_.bridge_destroyed(destruction);
}

void RiverWorld::retire_bridge_crossing_for_pending_life_end() noexcept {
    bridge_sprites_.retire_crossing_for_pending_life_end();
}

BridgeSpriteState RiverWorld::bridge_sprite_state() const noexcept {
    return bridge_sprites_.state();
}

BridgeSpriteState RiverWorld::bridge_presentation_state() const noexcept {
    return bridge_sprites_.presentation_state();
}

const AuxiliaryProjectileState& RiverWorld::auxiliary_projectile_state() const noexcept {
    return auxiliary_projectile_.state();
}

AuxiliaryProjectileTickResult RiverWorld::advance_auxiliary_projectile(
    bool lifecycle_active, bool demo_mode, bool audio_effect_idle,
    const std::array<std::uint8_t, 6>& terrain_contacts,
    const std::array<std::uint8_t, 2>& player_contacts) {
    return auxiliary_projectile_.advance_tick({objects_.state(), demo_mode,
        lifecycle_active, audio_effect_idle, terrain_contacts, player_contacts});
}

void RiverWorld::hide_life_transients() noexcept {
    bridge_sprites_.hide_for_life_reveal();
    auxiliary_projectile_.hide_for_life_reveal();
}

std::optional<WorldObjectHitEffect> RiverWorld::apply_projectile_hit(
    std::size_t record_index, WorldObjectHitSlot slot, bool round_event_pending) {
    return objects_.apply_projectile_hit(record_index, slot, round_event_pending);
}

std::optional<GapContactHitEffect> RiverWorld::apply_gap_contact_hit(
    std::size_t record_index, WorldObjectHitSlot slot) {
    return objects_.apply_gap_contact_hit(record_index, slot);
}

void RiverWorld::begin_life() noexcept {
    generator_.begin_life();
}

void RiverWorld::mark_bridge_destroyed() noexcept {
    generator_.mark_bridge_destroyed();
}

void RiverWorld::set_checkpoint(RiverCheckpoint checkpoint) noexcept {
    generator_.set_checkpoint(checkpoint);
}

RiverCheckpoint RiverWorld::selected_checkpoint() const noexcept {
    return generator_.selected_checkpoint();
}

RiverCheckpoint RiverWorld::commit_checkpoint() noexcept {
    return generator_.commit_checkpoint();
}

RiverCheckpointRestore RiverWorld::restore_checkpoint() noexcept {
    const auto restore = generator_.restore_checkpoint();
    clear_checkpoint_state();
    return restore;
}

RiverCheckpointRestore RiverWorld::restore_checkpoint(RiverCheckpoint checkpoint) noexcept {
    const auto restore = generator_.restore_checkpoint(checkpoint);
    clear_checkpoint_state();
    return restore;
}

void RiverWorld::clear_checkpoint_state() noexcept {
    bridge_sprites_.reset();
    objects_.reset();
    guides_ = {};
    viewport_.clear();
    generated_rows_ = 0;
}

void RiverWorld::reset() noexcept {
    auxiliary_projectile_.reset();
    generator_.reset();
    clear_checkpoint_state();
}

const RiverGeneratorState& RiverWorld::river_state() const noexcept { return generator_.state(); }
const WorldObjectState& RiverWorld::object_state() const noexcept { return objects_.state(); }
const RiverViewport& RiverWorld::viewport() const noexcept { return viewport_; }
std::size_t RiverWorld::generated_rows() const noexcept { return generated_rows_; }

} // namespace river_raid
