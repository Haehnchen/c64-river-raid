#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace river_raid {

inline constexpr std::uint32_t kMaximumScore = 9999990;

[[nodiscard]] constexpr std::optional<std::uint8_t> bridge_score_kind_write(
    bool joint_contact, bool round_event_pending) noexcept {
    if (!joint_contact) return std::nullopt;
    return round_event_pending ? std::uint8_t{0} : std::uint8_t{16};
}

[[nodiscard]] constexpr std::uint32_t object_score(std::uint8_t kind) noexcept {
    constexpr std::array<std::uint32_t, 17> points{
        0, 0, 0, 0, 0, 0, 0, 100, 60, 60, 150, 150, 60, 30, 500, 80, 750};
    return kind < points.size() ? points[kind] : 0;
}

struct ScoreAward {
    std::uint32_t total{};
    bool extra_life{};
    bool exhausted{};
};

[[nodiscard]] constexpr ScoreAward award_object_score(
    std::uint32_t current, std::uint8_t kind) noexcept {
    const auto total = static_cast<std::uint64_t>(current) + object_score(kind);
    if (total > kMaximumScore) return {kMaximumScore, false, true};
    const auto next = static_cast<std::uint32_t>(total);
    return {next, current / 10000 != next / 10000, false};
}

} // namespace river_raid
