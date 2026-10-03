#pragma once

#include "app/approximate_activation_source.hpp"
#include "app/flight_audio.hpp"
#include "app/foreground_contacts.hpp"
#include "game/flight_controls.hpp"
#include "game/frame_pacer.hpp"
#include "game/fuel.hpp"
#include "game/life_cycle.hpp"
#include "game/session_idle.hpp"
#include "game/idle_watchdog.hpp"
#include "game/player_steering.hpp"
#include "game/player_shot.hpp"
#include "game/river_world.hpp"

#include <chrono>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace river_raid {

enum class FlightStatus : std::uint8_t {
    Ready,
    Preparing,
    Flying,
    Crashed,
    GameOver,
    OptionWait,
    Attract,
    Blank,
};

struct CheckpointResetWorkState {
    std::uint8_t presentation_ticks_remaining{};
    std::uint8_t deferred_scroll_rows{};

    friend bool operator==(const CheckpointResetWorkState&,
                           const CheckpointResetWorkState&) = default;
};

// Native round; original active-round timing remains incomplete.
class FlightPreview {
public:
    FlightPreview();

    void start(std::size_t selected_option = 0);
    void show_options(std::size_t selected_option, bool initial_entry = false);
    void set_controls(FlightControls controls) noexcept;
    void set_joystick_controls(JoystickControls controls) noexcept;
    void note_function_key() noexcept;
    void return_to_title();
    std::size_t advance_elapsed(std::chrono::nanoseconds elapsed);
    [[nodiscard]] std::vector<float> take_audio() noexcept;
    [[nodiscard]] const AudioControllerState& audio_state() const noexcept;

    [[nodiscard]] const RiverWorld& world() const noexcept;
    [[nodiscard]] const PlayerSteeringState& steering() const noexcept;
    [[nodiscard]] FlightPose pose() const noexcept;
    // Phase used by the next admitted presentation/foreground update.
    [[nodiscard]] std::uint8_t animation_phase() const noexcept;
    [[nodiscard]] const std::optional<CheckpointResetWorkState>&
    checkpoint_reset_work() const noexcept;
    [[nodiscard]] const PlayerSteeringState& presented_steering() const noexcept;
    [[nodiscard]] FlightPose presented_pose() const noexcept;
    [[nodiscard]] LifeCycleImage presented_image() const noexcept;
    [[nodiscard]] const PlayerShotResult& shot() const noexcept;
    // Diagnostic copy at consumption; indices precede this pass's row shifts.
    [[nodiscard]] const ForegroundContactSnapshot& last_consumed_contacts() const noexcept;
    // Actual selection after motion/shot effects, before player effects and row shifts.
    [[nodiscard]] const PlayerContactResult& last_player_contact() const noexcept;
    // Latest produced object bands (or a new-world bootstrap), retained across
    // row shifts. Indices identify that generation, not current records.
    [[nodiscard]] const std::vector<WorldObjectBandSchedule>&
    presented_object_bands() const noexcept;
    [[nodiscard]] std::size_t ordinary_hits() const noexcept;
    [[nodiscard]] std::uint32_t score() const noexcept;
    [[nodiscard]] std::uint32_t player_score(std::size_t player) const;
    [[nodiscard]] std::uint32_t displayed_player_score(std::size_t player) const;
    [[nodiscard]] std::size_t selected_option() const noexcept;
    [[nodiscard]] bool two_players() const noexcept;
    [[nodiscard]] std::uint32_t high_score() const noexcept;
    [[nodiscard]] std::uint16_t fuel() const noexcept;
    [[nodiscard]] std::uint16_t bridge_number() const noexcept;
    [[nodiscard]] bool bridge_flash() const noexcept;
    [[nodiscard]] FlightStatus status() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] const LifeCycleState& life_cycle() const noexcept;
    [[nodiscard]] bool invulnerable() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool accepts_session_input() const noexcept;
    [[nodiscard]] const SessionIdleState& idle_state() const noexcept;
    [[nodiscard]] const IdleWatchdogState& watchdog_state() const noexcept;

private:
    void begin_crash(LifeEndAudioCause cause = LifeEndAudioCause::Collision);
    void prepare_round(std::size_t selected_option);
    void begin_attract();
    void advance_audio_tick();
    void bridge_cleared(bool joint_contact);
    void apply_life_transition(const LifeCycleTransition& transition);
    void advance_tick();
    void advance_reset_work_tick();
    void advance_scroll_attempt();
    void sample_controls() noexcept;
    void initialize_players(std::size_t preset_index, RiverCheckpoint checkpoint);
    void publish_presentation() noexcept;
    void reset_contact_presentation() noexcept;
    void seed_object_presentation();

    RiverWorld world_;
    FramePacer pacer_;
    ApproximateActivationSource activation_source_{};
    FlightAudio audio_;
    std::uint8_t audio_timer_sample_{};
    FlightControls controls_{};
    FlightControls keyboard_controls_{};
    JoystickControls joystick_controls_{};
    PlayerSteeringState steering_{};
    FlightPose pose_{FlightPose::Straight};
    PlayerSteeringState presented_steering_{};
    FlightPose presented_pose_{FlightPose::Straight};
    LifeCycleImage presented_image_{LifeCycleImage::Straight};
    PlayerShotResult shot_{};
    ForegroundContactPipeline contact_pipeline_;
    ForegroundContactSnapshot last_consumed_contacts_{};
    PlayerContactResult last_player_contact_{};
    std::vector<WorldObjectBandSchedule> presented_object_bands_;
    std::uint64_t contact_epoch_{};
    std::uint8_t object_animation_phase_{};
    std::uint8_t scroll_fraction_{};
    std::uint64_t pending_ticks_{};
    std::size_t ordinary_hits_{};
    struct PlayerProgress {
        std::uint32_t score{};
        std::uint16_t bridge_number{1};
        RiverCheckpoint checkpoint{};
    };
    std::array<PlayerProgress, 2> players_{};
    std::array<std::uint32_t, 2> attract_display_scores_{};
    std::array<std::uint32_t, 4> high_scores_{};
    std::size_t selected_option_{};
    FuelState fuel_{};
    std::uint8_t bridge_flash_ticks_{};
    LifeCycle life_cycle_;
    SessionIdle session_idle_;
    IdleWatchdog idle_watchdog_;
    std::uint8_t demo_startup_ticks_{};
    bool attract_pending_{};
    std::optional<LifeEndAudioCause> pending_life_end_;
    std::optional<CheckpointResetWorkState> checkpoint_reset_work_;
    FlightStatus status_{FlightStatus::Ready};
};

} // namespace river_raid
