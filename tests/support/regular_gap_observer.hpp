#pragma once

#include "game/bridge_sprites.hpp"
#include "game/life_cycle.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace river_raid::test {

// Read-only observations; no FlightPreview state is constructed or changed here.
struct RegularGapSnapshot {
    BridgeSpriteState mechanics;
    BridgeSpriteState published;
    std::uint16_t bridge{};
    LifeCyclePhase life_phase{LifeCyclePhase::Terminal};
    int lives{};
    bool flying{};
};

class RegularGapObserver {
public:
    // Hold ordinary controls across observation steps. Multiple admitted updates
    // invalidate provenance; zero-update calls cannot confirm a survival probe.
    [[nodiscard]] bool observe(const RegularGapSnapshot& before,
                               const RegularGapSnapshot& after,
                               std::size_t admitted_updates) noexcept {
        if (admitted_updates > 1 || !first_life(after) || !enabled_normal(after.mechanics)) {
            invalidate();
            return false;
        }
        if (admitted_updates == 0) {
            if (armed() && (!first_life(before) || before.bridge != bridge_ ||
                            after.bridge != bridge_)) invalidate();
            return false;
        }

        const bool new_crossing = after.mechanics.gap_animation_counter == 0xff &&
            !before.mechanics.crossing && before.bridge == after.bridge;
        if (new_crossing) {
            phase_ = Phase::Armed;
            bridge_ = after.bridge; // Earlier bridge progression is allowed.
            witness_.reset();
            return false;
        }
        if (phase_ == Phase::Unarmed || phase_ == Phase::Invalid) return false;
        if (!first_life(before) || !enabled_normal(before.mechanics) ||
            before.bridge != bridge_ || after.bridge != bridge_) {
            // Eligibility can remain true after a proximity destruction. Do not
            // re-arm that same crossing merely because its counter remains FF.
            invalidate();
            return false;
        }
        if (phase_ == Phase::PendingSurvival) {
            phase_ = Phase::Confirmed;
            return true;
        }
        if (phase_ == Phase::Confirmed) return true;

        const auto counter = after.mechanics.gap_animation_counter;
        if (phase_ == Phase::Armed) {
            if (counter == 0xff) return false;
            if (before.mechanics.gap_animation_counter != 0xff || counter > 1) {
                invalidate();
                return false;
            }
            phase_ = Phase::EntryObserved;
        }
        if (counter > 17) {
            invalidate();
            return false;
        }
        if (counter >= 1 && enabled_normal(after.published) &&
            gap_flight(after.mechanics) && gap_flight(after.published)) {
            witness_ = after;
            phase_ = Phase::PendingSurvival;
        }
        return false;
    }

    [[nodiscard]] bool armed() const noexcept {
        return phase_ != Phase::Unarmed && phase_ != Phase::Invalid;
    }
    [[nodiscard]] bool awaiting_survival() const noexcept {
        return phase_ == Phase::PendingSurvival;
    }
    [[nodiscard]] bool confirmed() const noexcept { return phase_ == Phase::Confirmed; }
    [[nodiscard]] const std::optional<RegularGapSnapshot>& witness() const noexcept {
        return witness_;
    }

private:
    enum class Phase { Unarmed, Armed, EntryObserved, PendingSurvival, Confirmed, Invalid };

    [[nodiscard]] static bool normal_image(BridgeSpriteImage image) noexcept {
        return image == BridgeSpriteImage::CrossingRightA ||
               image == BridgeSpriteImage::CrossingRightB ||
               image == BridgeSpriteImage::CrossingLeftA ||
               image == BridgeSpriteImage::CrossingLeftB;
    }
    [[nodiscard]] static bool enabled_normal(const BridgeSpriteState& state) noexcept {
        return state.automatic_entry_enabled && state.crossing &&
               normal_image(state.crossing->image);
    }
    [[nodiscard]] static bool first_life(const RegularGapSnapshot& snapshot) noexcept {
        return snapshot.flying && snapshot.life_phase == LifeCyclePhase::Active &&
               snapshot.lives == 3;
    }
    [[nodiscard]] static bool gap_flight(const BridgeSpriteState& state) noexcept {
        return state.gap && state.gap->image == BridgeSpriteImage::GapFlight;
    }
    void invalidate() noexcept {
        phase_ = Phase::Invalid;
        witness_.reset();
    }

    Phase phase_{Phase::Unarmed};
    std::uint16_t bridge_{};
    std::optional<RegularGapSnapshot> witness_;
};

} // namespace river_raid::test
