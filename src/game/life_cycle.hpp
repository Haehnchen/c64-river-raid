#pragma once

#include <cstdint>
#include <optional>

namespace river_raid {

inline constexpr std::uint8_t kInitialReserveAircraft = 4;
inline constexpr std::uint8_t kMaximumReserveAircraft = 9;

enum class LifeCyclePlayerMode : std::uint8_t {
    Single,
    TwoPlayer,
};

enum class LifeCyclePhase : std::uint8_t {
    Active,
    Exploding,
    Rebuilding,
    AwaitInput,
    Terminal,
};

enum class LifeEndCause : std::uint8_t {
    Collision,
    FuelExhausted,
};

enum class LifeCycleImage : std::uint8_t {
    Straight,
    ExplosionA,
    ExplosionB,
    Hidden,
};

struct LifeCycleState {
    LifeCyclePhase phase{LifeCyclePhase::Active};
    LifeCycleImage image{LifeCycleImage::Straight};
    std::uint8_t timer{};
    std::uint8_t image_changes_remaining{};
    std::uint8_t reserve_aircraft{kInitialReserveAircraft};
    std::uint8_t inactive_reserve_aircraft{};
    std::uint8_t active_player{};
    LifeCyclePlayerMode player_mode{LifeCyclePlayerMode::Single};
    std::optional<LifeEndCause> cause;

    friend bool operator==(const LifeCycleState&, const LifeCycleState&) = default;
};

struct LifeCycleTransition {
    std::optional<LifeEndCause> start_audio;
    std::uint8_t forced_scroll_rows{};
    bool clear_player_shot{};
    bool restore_checkpoint{};
    bool reveal_aircraft_and_clear_transients{};
    bool reserve_decremented{};
    bool player_changed{};
    bool resume_controls{};
    bool terminal{};

    friend bool operator==(const LifeCycleTransition&,
                           const LifeCycleTransition&) = default;
};

class LifeCycle {
public:
    explicit LifeCycle(
        std::uint8_t reserve_aircraft = kInitialReserveAircraft);

    void start_round(LifeCyclePlayerMode mode = LifeCyclePlayerMode::Single) noexcept;
    void enter_idle() noexcept;
    void clear_session() noexcept;
    [[nodiscard]] LifeCycleTransition begin_life_end(LifeEndCause cause) noexcept;
    [[nodiscard]] LifeCycleTransition advance(bool any_control_pressed) noexcept;
    [[nodiscard]] bool add_reserve_aircraft() noexcept;
    void exhaust_reserves() noexcept;
    void reset() noexcept;

    [[nodiscard]] const LifeCycleState& state() const noexcept;
    [[nodiscard]] bool controls_active() const noexcept;

private:
    std::uint8_t initial_reserve_aircraft_{};
    LifeCycleState state_;
};

} // namespace river_raid
