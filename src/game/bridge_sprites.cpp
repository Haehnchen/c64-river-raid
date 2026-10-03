#include "game/bridge_sprites.hpp"

#include <array>
#include <utility>

namespace river_raid {
namespace {

using Byte = std::uint8_t;

constexpr Byte kInitialVerticalPosition = 0x21;
constexpr Byte kGapEntryVerticalPosition = 0x3b;
constexpr Byte kGapVisibleVerticalPosition = 0x3c;
constexpr Byte kRetireVerticalPosition = 0xce;
constexpr Byte kInactiveCounter = 0xff;
constexpr Byte kGapFlightTicks = 0x11;
constexpr Byte kGapRetireCounter = 0x39;

constexpr Byte wrapping_add(Byte value, int amount) noexcept {
    return static_cast<Byte>(static_cast<int>(value) + amount);
}

constexpr Byte wrapping_subtract(Byte value, int amount) noexcept {
    return static_cast<Byte>(static_cast<int>(value) - amount);
}

constexpr bool negative(Byte value) noexcept {
    return (value & 0x80U) != 0;
}

constexpr bool moves_right(BridgeSpriteImage image) noexcept {
    return image == BridgeSpriteImage::CrossingRightA ||
           image == BridgeSpriteImage::CrossingRightB;
}

constexpr bool is_crossing_image(BridgeSpriteImage image) noexcept {
    return image == BridgeSpriteImage::CrossingRightA ||
           image == BridgeSpriteImage::CrossingRightB ||
           image == BridgeSpriteImage::CrossingLeftA ||
           image == BridgeSpriteImage::CrossingLeftB;
}

constexpr BridgeSpriteImage next_crossing_frame(BridgeSpriteImage image) noexcept {
    switch (image) {
    case BridgeSpriteImage::CrossingRightA:
        return BridgeSpriteImage::CrossingRightB;
    case BridgeSpriteImage::CrossingRightB:
        return BridgeSpriteImage::CrossingRightA;
    case BridgeSpriteImage::CrossingLeftA:
        return BridgeSpriteImage::CrossingLeftB;
    case BridgeSpriteImage::CrossingLeftB:
        return BridgeSpriteImage::CrossingLeftA;
    case BridgeSpriteImage::JointExplosionA:
        return BridgeSpriteImage::JointExplosionB;
    case BridgeSpriteImage::JointExplosionB:
        return BridgeSpriteImage::JointExplosionA;
    default:
        return image;
    }
}

constexpr BridgeSpriteImage gap_explosion_frame(Byte counter) noexcept {
    constexpr std::array frames{
        BridgeSpriteImage::GapExplosion0,
        BridgeSpriteImage::GapExplosion1,
        BridgeSpriteImage::GapExplosion2,
        BridgeSpriteImage::GapExplosion3,
        BridgeSpriteImage::GapExplosion4,
    };
    return frames[static_cast<std::size_t>((counter - kGapFlightTicks) >> 3U)];
}

struct CrossingPlacement {
    BridgeSpriteImage image;
    Byte horizontal_coordinate;
};

CrossingPlacement regular_placement(const RiverGeneratorState& river, Byte width) {
    constexpr std::array<Byte, 14> placements{
        0x00, 0x00, 0x96, 0x04, 0x8c, 0x10, 0x8e,
        0x0c, 0x7c, 0x20, 0x86, 0x14, 0x6c, 0x30,
    };

    auto image = BridgeSpriteImage::CrossingRightA;
    Byte horizontal{};
    const auto feature_left = river.feature.active &&
                              river.feature.side == RiverBankSide::Left;
    const auto feature_right = river.feature.active &&
                               river.feature.side == RiverBankSide::Right;

    if ((river.section & 1U) != 0) {
        horizontal = static_cast<Byte>(river.random.high & 0x1fU);
        if (horizontal >= 0x10U) {
            horizontal = wrapping_add(horizontal, 0x58);
            if (!feature_right) {
                image = BridgeSpriteImage::CrossingLeftA;
            } else {
                horizontal = wrapping_subtract(0x78, horizontal);
            }
        } else if (feature_left) {
            image = BridgeSpriteImage::CrossingLeftA;
            horizontal = wrapping_subtract(0x78, horizontal);
        }
        horizontal = wrapping_add(horizontal, river.course);
        horizontal = wrapping_subtract(horizontal, 4);
        return {image, horizontal};
    }

    horizontal = placements[width];
    if (horizontal >= 0x50U) {
        if (!feature_right) {
            image = BridgeSpriteImage::CrossingLeftA;
        } else {
            horizontal = wrapping_subtract(0x98, horizontal);
        }
    } else if (feature_left) {
        image = BridgeSpriteImage::CrossingLeftA;
        horizontal = wrapping_subtract(0x98, horizontal);
    }
    return {image, wrapping_add(horizontal, 0x0c)};
}

std::pair<Byte, Byte> regular_trajectory(const RiverGeneratorState& river, Byte width) {
    auto selector = static_cast<Byte>(river.random.high >> 3U);
    if (river.section == 0x0aU) selector ^= 0x07U;
    selector &= 0x06U;
    if (selector == 0 && width < 8) selector = 8;

    switch (selector) {
    case 6:
        return {2, 0};
    case 8:
        return {4, 0};
    default:
        return {1, 0x80};
    }
}

} // namespace

bool BridgeSpriteSystem::advance_after_row(const BridgeSpriteRowInput& input) {
    ++crossing_vertical_position_;
    ++gap_vertical_position_;
    return spawn(input);
}

bool BridgeSpriteSystem::spawn(const BridgeSpriteRowInput& input) {
    const auto& river = input.river;
    if (river.phase != 0 || crossing_active_) return false;

    if (river.scripted_course) {
        crossing_vertical_position_ = kInitialVerticalPosition;
        trigger_gap_at_entry_ = false;
        horizontal_fraction_step_ = 0;
        horizontal_step_ = 2;
        gap_animation_counter_ = kInactiveCounter;

        const auto anchor = static_cast<Byte>(river.random.low & 0x7fU);
        crossing_image_ = anchor < 0x40U ? BridgeSpriteImage::CrossingRightA
                                        : BridgeSpriteImage::CrossingLeftA;
        crossing_horizontal_coordinate_ = wrapping_add(anchor, 0x1c);
    } else {
        if (is_split_channel(river.channel_mode) ||
            input.objects.generator_cursor > input.objects.records.size()) {
            return false;
        }
        // The source's one-past cursor intentionally reaches the oldest
        // record's retained base kind in its packed history layout.
        const auto kind = input.objects.generator_cursor == input.objects.records.size()
                              ? input.objects.records.front().base_kind
                              : input.objects.records[input.objects.generator_cursor].animated_kind;
        if (kind < 8 || river.section < 7 || river.section_interval < 5 ||
            river.section_interval >= 14 || (river.random.high & 0xc0U) != 0) {
            return false;
        }

        const auto width = static_cast<Byte>(
            river.target_width_units < river.previous_target_width_units
                ? river.target_width_units
                : river.previous_target_width_units);
        if (width < 5 || width >= 14) return false;

        const auto trajectory = regular_trajectory(river, width);
        horizontal_step_ = trajectory.first;
        horizontal_fraction_step_ = trajectory.second;
        const auto placement = regular_placement(river, width);
        crossing_image_ = placement.image;
        crossing_horizontal_coordinate_ = placement.horizontal_coordinate;
        gap_animation_counter_ = kInactiveCounter;
        trigger_gap_at_entry_ = true;
        crossing_vertical_position_ = kInitialVerticalPosition;
    }

    const auto crossing_right = moves_right(crossing_image_);
    const auto left_guide = is_split_channel(river.channel_mode)
                                ? input.guides.second_left
                                : input.guides.first_left;
    target_horizontal_coordinate_ = crossing_right
                                        ? wrapping_subtract(left_guide, 14)
                                        : wrapping_add(input.guides.first_right, 14);
    if (crossing_right) {
        if (crossing_horizontal_coordinate_ >= target_horizontal_coordinate_) {
            crossing_horizontal_coordinate_ = target_horizontal_coordinate_;
        }
    } else if (target_horizontal_coordinate_ >= crossing_horizontal_coordinate_) {
        crossing_horizontal_coordinate_ = target_horizontal_coordinate_;
    }

    if (river.section == 0x34U && river.section_interval >= 4) {
        crossing_horizontal_coordinate_ = wrapping_add(crossing_horizontal_coordinate_, 0x14);
    }

    const auto visible_coordinate = wrapping_subtract(crossing_horizontal_coordinate_, 0x10);
    if (visible_coordinate >= 0xa0U) {
        crossing_active_ = false;
        crossing_horizontal_coordinate_ = visible_coordinate;
        crossing_vertical_position_ = visible_coordinate;
        return false;
    }

    crossing_active_ = true;
    gap_active_ = false;
    return true;
}

void BridgeSpriteSystem::advance_tick(const BridgeSpriteTick& tick) {
    advance_crossing(tick);
    advance_gap(tick);
}

void BridgeSpriteSystem::advance_crossing(const BridgeSpriteTick& tick) {
    if (crossing_active_) {
        const auto cadence_mask = is_crossing_image(crossing_image_) ? Byte{0x03}
                                                                     : Byte{0x0f};
        if ((tick.animation_phase & cadence_mask) == 0) {
            if (negative(gap_animation_counter_) &&
                (tick.round_active || tick.demo_mode)) {
                auto skip_motion = false;
                if (trigger_gap_at_entry_ &&
                    crossing_vertical_position_ >= kGapEntryVerticalPosition) {
                    ++gap_animation_counter_;
                    skip_motion = !negative(gap_animation_counter_);
                }
                if (!skip_motion && !tick.bridge_progressed) {
                    if (moves_right(crossing_image_)) {
                        ++crossing_horizontal_coordinate_;
                    } else {
                        --crossing_horizontal_coordinate_;
                    }
                }
            }
            crossing_image_ = next_crossing_frame(crossing_image_);
        }

        if (crossing_vertical_position_ >= kRetireVerticalPosition) {
            crossing_active_ = false;
            gap_active_ = false;
            gap_animation_counter_ = kInactiveCounter;
        }
    }
    publish_crossing();
}

void BridgeSpriteSystem::advance_gap(const BridgeSpriteTick& tick) {
    if (!crossing_active_ || negative(gap_animation_counter_)) {
        gap_active_ = false;
        publish_gap();
        return;
    }

    // The source can reach or skip past the final counter through another
    // proximity hit. Its pointer arithmetic is harmless; the typed image
    // inventory must retire before indexing a semantic explosion frame.
    if (gap_animation_counter_ >= kGapRetireCounter - 1U) {
        gap_active_ = false;
        gap_animation_counter_ = tick.round_active ? Byte{0} : kInactiveCounter;
        publish_gap();
        return;
    }

    if (gap_animation_counter_ == 0) {
        if (crossing_vertical_position_ < kGapVisibleVerticalPosition) {
            gap_active_ = false;
            publish_gap();
            return;
        }
        gap_vertical_position_ = wrapping_subtract(crossing_vertical_position_, 4);
        gap_horizontal_fraction_ = 0;
        gap_horizontal_coordinate_ = moves_right(crossing_image_)
                                         ? wrapping_add(crossing_horizontal_coordinate_, 7)
                                         : wrapping_subtract(crossing_horizontal_coordinate_, 7);
        gap_image_ = BridgeSpriteImage::GapFlight;
        gap_active_ = true;
    } else if (gap_animation_counter_ < kGapFlightTicks) {
        constexpr std::array<Byte, kGapFlightTicks> vertical_steps{
            0x00, 0xff, 0x00, 0xff, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
        };
        gap_vertical_position_ = wrapping_add(
            gap_vertical_position_, vertical_steps[gap_animation_counter_]);

        auto position = static_cast<std::uint16_t>(gap_horizontal_coordinate_) << 8U;
        position |= gap_horizontal_fraction_;
        auto delta = static_cast<std::uint16_t>(horizontal_step_) << 8U;
        delta |= horizontal_fraction_step_;
        position = moves_right(crossing_image_)
                       ? static_cast<std::uint16_t>(position + delta)
                       : static_cast<std::uint16_t>(position - delta);
        gap_horizontal_coordinate_ = static_cast<Byte>(position >> 8U);
        gap_horizontal_fraction_ = static_cast<Byte>(position);
        gap_image_ = BridgeSpriteImage::GapFlight;
        gap_active_ = true;
    } else {
        gap_image_ = gap_explosion_frame(gap_animation_counter_);
        gap_active_ = true;
    }

    ++gap_animation_counter_;
    publish_gap();
}

BridgeSpriteDestructionResult BridgeSpriteSystem::bridge_destroyed(
    const BridgeSpriteDestruction& destruction) {
    if (destruction.joint_contact) {
        crossing_active_ = true;
        crossing_image_ = BridgeSpriteImage::JointExplosionA;
        return {true, false};
    }

    if (destruction.displayed_bridge_number < 5) return {};

    const auto crossing_right = !crossing_active_ || moves_right(crossing_image_);
    const auto close = crossing_right
                           ? wrapping_subtract(target_horizontal_coordinate_,
                                               crossing_horizontal_coordinate_) < 0x29U
                           : wrapping_subtract(crossing_horizontal_coordinate_,
                                               target_horizontal_coordinate_) < 0x41U;
    if (!close) return {};
    ++gap_animation_counter_;
    return {false, true};
}

void BridgeSpriteSystem::retire_crossing_for_pending_life_end() noexcept {
    // The private joint image is retired after the crossing publication, so
    // the last programmed sprite remains until the next crossing update.
    if (crossing_active_ && !is_crossing_image(crossing_image_)) {
        crossing_active_ = false;
    }
}

BridgeSpriteState BridgeSpriteSystem::state() const noexcept {
    BridgeSpriteState result{};
    if (crossing_active_) {
        result.crossing = BridgeSpritePresentation{
            crossing_image_,
            static_cast<std::uint16_t>(crossing_horizontal_coordinate_ * 2U),
            crossing_vertical_position_,
            0,
        };
    }
    if (gap_active_) {
        result.gap = BridgeSpritePresentation{
            gap_image_,
            static_cast<std::uint16_t>(gap_horizontal_coordinate_ * 2U),
            gap_vertical_position_,
            1,
        };
    }
    result.gap_animation_counter = gap_animation_counter_;
    result.target_horizontal_coordinate = target_horizontal_coordinate_;
    result.automatic_entry_enabled = crossing_active_ && trigger_gap_at_entry_ &&
                                     is_crossing_image(crossing_image_);
    return result;
}

void BridgeSpriteSystem::publish_crossing() noexcept {
    if (crossing_active_) {
        presented_crossing_ = BridgeSpritePresentation{
            crossing_image_,
            static_cast<std::uint16_t>(crossing_horizontal_coordinate_ * 2U),
            crossing_vertical_position_,
            0,
        };
    } else {
        presented_crossing_.reset();
    }
}

void BridgeSpriteSystem::publish_gap() noexcept {
    if (gap_active_) {
        presented_gap_ = BridgeSpritePresentation{
            gap_image_,
            static_cast<std::uint16_t>(gap_horizontal_coordinate_ * 2U),
            gap_vertical_position_,
            1,
        };
    } else {
        presented_gap_.reset();
    }
}

BridgeSpriteState BridgeSpriteSystem::presentation_state() const noexcept {
    return {presented_crossing_, presented_gap_, gap_animation_counter_,
            target_horizontal_coordinate_,
            crossing_active_ && trigger_gap_at_entry_ && is_crossing_image(crossing_image_)};
}

void BridgeSpriteSystem::hide_for_life_reveal() noexcept {
    crossing_vertical_position_ = 0;
    gap_vertical_position_ = 0;
    if (presented_crossing_) presented_crossing_->vic_y = 0;
    if (presented_gap_) presented_gap_->vic_y = 0;
}

void BridgeSpriteSystem::reset() noexcept {
    *this = BridgeSpriteSystem{};
}

} // namespace river_raid
