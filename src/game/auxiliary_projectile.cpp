#include "game/auxiliary_projectile.hpp"

#include <algorithm>

namespace river_raid {
namespace {

using Byte = std::uint8_t;

constexpr Byte kObjectKindMask = 0x0e;
constexpr Byte kEligibleObjectKind = 0x0a;
constexpr Byte kMovingLeft = 0x08;
constexpr Byte kFirstVisibleVerticalPosition = 0x3b;
constexpr Byte kRetireVerticalPosition = 0xd3;
constexpr Byte kProjectileParticipant = 0x08;
constexpr Byte kPlayerParticipant = 0x01;

constexpr Byte wrapping_add(Byte value, int amount) noexcept {
    return static_cast<Byte>(static_cast<int>(value) + amount);
}

template <std::size_t Size>
bool has_participant(const std::array<Byte, Size>& samples, Byte participant) {
    return std::any_of(samples.begin(), samples.end(), [participant](Byte sample) {
        return (sample & participant) != 0;
    });
}

} // namespace

void AuxiliaryProjectileSystem::advance_after_row(
    std::optional<AuxiliaryProjectileSpawn> spawn) {
    ++state_.vertical_position;
    if (!spawn) return;

    state_.horizontal_coordinate = 0;
    state_.vertical_position = spawn->generator_phase == 0 ? Byte{0x29} : Byte{0x28};
}

AuxiliaryProjectileTickResult AuxiliaryProjectileSystem::advance_tick(
    const AuxiliaryProjectileTick& tick) {
    const auto cursor = tick.objects.special_cursor;
    if (cursor >= tick.objects.records.size()) return publish(0, tick);

    const auto& source = tick.objects.records[cursor];
    if ((source.animated_kind & kObjectKindMask) != kEligibleObjectKind ||
        state_.vertical_position < kFirstVisibleVerticalPosition ||
        (!tick.demo_mode && !tick.lifecycle_active)) {
        return publish(0, tick);
    }

    if (has_participant(tick.terrain_contact_samples, kProjectileParticipant)) {
        state_.horizontal_coordinate = 0;
        return publish(0, tick);
    }

    AuxiliaryProjectileTickResult result{};
    if (state_.horizontal_coordinate == 0) {
        if (!tick.audio_effect_idle) return result;

        const auto offset = (source.behavior_flags & kMovingLeft) != 0 ? -8 : 8;
        state_.horizontal_coordinate = wrapping_add(source.horizontal_coordinate, offset);
        result.audio_started = true;
    } else {
        const auto step = (source.behavior_flags & kMovingLeft) != 0 ? -2 : 2;
        state_.horizontal_coordinate = wrapping_add(state_.horizontal_coordinate, step);
    }

    const auto published = publish(state_.horizontal_coordinate, tick);
    result.fatal_player_contact = published.fatal_player_contact;
    return result;
}

AuxiliaryProjectileTickResult AuxiliaryProjectileSystem::publish(
    Byte published_horizontal_coordinate, const AuxiliaryProjectileTick& tick) {
    if (state_.vertical_position >= kRetireVerticalPosition) {
        state_.horizontal_coordinate = 0;
        state_.vertical_position = 0;
    }

    if (state_.horizontal_coordinate == 0) {
        state_.presentation.reset();
        return {};
    }

    state_.presentation = AuxiliaryProjectilePresentation{
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(
            published_horizontal_coordinate) * 2U),
        state_.vertical_position,
    };

    constexpr Byte player_contact = kProjectileParticipant | kPlayerParticipant;
    const auto fatal = std::any_of(
        tick.player_contact_samples.begin(), tick.player_contact_samples.end(),
        [](Byte sample) { return (sample & player_contact) == player_contact; });
    return {false, fatal};
}

void AuxiliaryProjectileSystem::hide_for_life_reveal() noexcept {
    state_.vertical_position = 0;
    if (state_.presentation) state_.presentation->vic_y = 0;
}

void AuxiliaryProjectileSystem::reset() noexcept {
    state_ = {};
}

const AuxiliaryProjectileState& AuxiliaryProjectileSystem::state() const noexcept {
    return state_;
}

} // namespace river_raid
