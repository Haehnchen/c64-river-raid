#pragma once

#include <cstdint>

namespace river_raid {

inline constexpr std::uint16_t kEmptyFuel = 0x0000;
inline constexpr std::uint16_t kFullFuel = 0xFFFF;
inline constexpr std::uint16_t kInitialFuel = kFullFuel;
inline constexpr std::uint16_t kFuelDrainPerTick = 0x0020;
inline constexpr std::uint16_t kRefuelPerContact = 0x0300;

struct FuelState {
    std::uint16_t amount{kInitialFuel};

    friend bool operator==(const FuelState&, const FuelState&) = default;
};

struct FuelConsumeResult {
    FuelState state;
    // An unpayable drain retains the tank. Reaching exactly zero reports
    // exhaustion on the following admitted drain.
    bool exhausted{};
};

// One admitted active-flight drain. Lifecycle and scheduling gates belong to
// the caller.
[[nodiscard]] constexpr FuelConsumeResult consume_fuel(FuelState state) noexcept {
    if (state.amount < kFuelDrainPerTick) {
        return {state, true};
    }
    state.amount = static_cast<std::uint16_t>(state.amount - kFuelDrainPerTick);
    return {state, false};
}

// One admitted live-depot contact. Contact geometry and destruction state
// belong to the caller.
[[nodiscard]] constexpr FuelState refuel_fuel(FuelState state) noexcept {
    constexpr auto maximum_before_refill =
        static_cast<std::uint16_t>(kFullFuel - kRefuelPerContact);
    if (state.amount > maximum_before_refill) return {kFullFuel};
    return {static_cast<std::uint16_t>(state.amount + kRefuelPerContact)};
}

} // namespace river_raid
