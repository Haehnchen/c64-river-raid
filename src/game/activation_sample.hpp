#pragma once

#include <cstdint>
#include <functional>

namespace river_raid {

// The owner supplies and resets the source; no implicit random fallback.
using ActivationSampleSource = std::function<std::uint8_t()>;

[[nodiscard]] constexpr bool is_activation_phase(std::uint8_t phase) noexcept {
    return (phase & 15U) == 0;
}

[[nodiscard]] constexpr std::uint8_t activation_candidate_from_sample(std::uint8_t sample) noexcept {
    const auto nibble = static_cast<std::uint8_t>(sample & 15U);
    return nibble < 12 ? nibble : static_cast<std::uint8_t>(nibble - 4);
}

} // namespace river_raid
