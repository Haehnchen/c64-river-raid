#include "app/flight_preview.hpp"

#include "app/demo_timing_setup.hpp"
#include "app/foreground_contacts.hpp"
#include "app/player_contact.hpp"
#include "app/world_setup.hpp"
#include "game/activation_sample.hpp"
#include "game/score.hpp"
#include "game/shot_contact_resolution.hpp"
#include "render/world_object_schedule.hpp"
#include "player_data.hpp"
#include "session_presets.hpp"
#include "demo_clock_data.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace river_raid {
namespace {

constexpr std::uint64_t kCatchUpTicks = 64;
constexpr int kScrollAttemptsPerTick = 2;
constexpr std::uint8_t kUnconditionalScrollSpeed = 0xFE;
// Bounded PAL cold/respawn observations, not a universal reset duration.
constexpr std::uint8_t kResetWorkPresentationTicks = 5;

[[nodiscard]] PlayerSteeringState initial_steering() noexcept {
    namespace data = assets::player;
    return {data::initial_horizontal_position, data::initial_horizontal_fraction,
            data::initial_horizontal_step, data::initial_scroll_speed};
}

} // namespace

FlightPreview::FlightPreview()
    : world_(make_river_world()), pacer_(demo_frame_rate(DemoRegion::Pal)),
      steering_(initial_steering()), presented_steering_(steering_) {
    seed_object_presentation();
}

void FlightPreview::start(std::size_t selected_option) {
    prepare_round(selected_option);
    audio_.reset();
    audio_timer_sample_ = 0;
    audio_.controller().start_transition_effect();
    pacer_.reset();
    pending_ticks_ = 0;
    session_idle_.stop();
    idle_watchdog_.reset();
}

void FlightPreview::prepare_round(std::size_t selected_option) {
    if (selected_option >= 8) throw std::out_of_range("Unknown session option");
    selected_option_ = selected_option;
    const auto preset = river_start_preset(selected_option / 2);
    world_ = make_river_world(preset);
    reset_contact_presentation();
    seed_object_presentation();
    initialize_players(selected_option / 2, {preset.section, preset.anchor});
    ordinary_hits_ = 0;
    life_cycle_.start_round(two_players() ? LifeCyclePlayerMode::TwoPlayer
                                         : LifeCyclePlayerMode::Single);
    pending_life_end_.reset();
    checkpoint_reset_work_.reset();
    fuel_ = {};
    activation_source_.reset();
    steering_ = initial_steering();
    steering_.scroll_speed = kUnconditionalScrollSpeed;
    pose_ = FlightPose::Straight;
    publish_presentation();
    shot_ = {};
    object_animation_phase_ = 0;
    scroll_fraction_ = 0;
    bridge_flash_ticks_ = 0;
    attract_pending_ = false;
    status_ = FlightStatus::Preparing;
}

void FlightPreview::show_options(std::size_t selected_option, bool initial_entry) {
    if (selected_option >= 8) throw std::out_of_range("Unknown session option");
    idle_watchdog_.reset();
    selected_option_ = selected_option;
    const auto preset = river_start_preset(selected_option / 2);
    initialize_players(selected_option / 2, {preset.section, preset.anchor});
    world_.set_checkpoint({preset.section, preset.anchor});
    reset_contact_presentation();
    if (initial_entry) {
        life_cycle_.start_round(two_players() ? LifeCyclePlayerMode::TwoPlayer
                                             : LifeCyclePlayerMode::Single);
        life_cycle_.enter_idle();
    } else {
        life_cycle_.clear_session();
    }
    presented_image_ = life_cycle_.state().image;
    clear_player_shot(shot_);
    pending_life_end_.reset();
    checkpoint_reset_work_.reset();
    object_animation_phase_ = 0xff;
    attract_pending_ = false;
    if (initial_entry) session_idle_.begin_initial_wait();
    else {
        session_idle_.begin_option_wait();
        audio_.controller().start_transition_effect();
    }
    status_ = FlightStatus::OptionWait;
}

void FlightPreview::begin_attract() {
    attract_display_scores_ = {displayed_player_score(0), displayed_player_score(1)};
    prepare_round(selected_option_);
    apply_life_transition(life_cycle_.advance(false));
    life_cycle_.enter_idle();
    object_animation_phase_ = 0xff;
    demo_startup_ticks_ = assets::demo_clock::initial_startup_ticks;
    status_ = FlightStatus::Attract;
}

void FlightPreview::set_controls(FlightControls controls) noexcept {
    keyboard_controls_ = controls;
    sample_controls();
}

void FlightPreview::set_joystick_controls(JoystickControls controls) noexcept {
    joystick_controls_ = controls;
    sample_controls();
}

void FlightPreview::sample_controls() noexcept {
    controls_ = combine_controls(keyboard_controls_,
                                 joystick_controls_[life_cycle_.state().active_player]);
}

void FlightPreview::note_function_key() noexcept {
    if (status_ != FlightStatus::Blank) idle_watchdog_.note_activity();
}

void FlightPreview::return_to_title() {
    const auto records = high_scores_;
    *this = FlightPreview{};
    high_scores_ = records;
}

const IdleWatchdogState& FlightPreview::watchdog_state() const noexcept {
    return idle_watchdog_.state();
}

std::size_t FlightPreview::advance_elapsed(std::chrono::nanoseconds elapsed) {
    audio_.begin_batch();
    if (status_ == FlightStatus::Ready) return 0;

    auto next_pacer = pacer_;
    const auto due = next_pacer.advance_elapsed(elapsed);
    if (due > std::numeric_limits<std::uint64_t>::max() - pending_ticks_) {
        throw std::overflow_error("Pending flight-preview tick count overflow");
    }
    pacer_ = std::move(next_pacer);
    pending_ticks_ += due;

    const auto budget = std::min(pending_ticks_, kCatchUpTicks);
    std::size_t completed = 0;
    std::uint64_t admitted = 0;
    while (admitted < budget) {
        if (status_ == FlightStatus::Blank) {
            audio_.sustain();
        } else if (checkpoint_reset_work_) {
            // Hold controller output while synthesis continues. Source IRQ SID
            // work during reset is not modeled; this is not audio parity.
            audio_.sustain();
            advance_reset_work_tick();
        } else {
            sample_controls();
            advance_audio_tick();
            advance_tick();
        }
        ++completed;
        --pending_ticks_;
        ++admitted;
    }
    return completed;
}

std::vector<float> FlightPreview::take_audio() noexcept { return audio_.take_samples(); }
const AudioControllerState& FlightPreview::audio_state() const noexcept { return audio_.state(); }

void FlightPreview::advance_audio_tick() {
    // Deterministic substitute until foreground CIA sampling is measured.
    audio_timer_sample_ = static_cast<std::uint8_t>(audio_timer_sample_ + 73U);
    const bool flying = status_ == FlightStatus::Flying && life_cycle_.controls_active();
    const bool sprites_running = status_ != FlightStatus::Ready;
    audio_.advance({steering_.scroll_speed, controls_.up, controls_.down, fuel_.amount,
        sprites_running ? world_.bridge_sprite_state().gap_animation_counter : std::uint8_t{0xff},
        audio_timer_sample_, flying, session_idle_.state().phase == SessionIdlePhase::Attract,
        status_ == FlightStatus::Ready});
}

const RiverWorld& FlightPreview::world() const noexcept { return world_; }
const PlayerSteeringState& FlightPreview::steering() const noexcept { return steering_; }
FlightPose FlightPreview::pose() const noexcept { return pose_; }
std::uint8_t FlightPreview::animation_phase() const noexcept { return object_animation_phase_; }
const std::optional<CheckpointResetWorkState>&
FlightPreview::checkpoint_reset_work() const noexcept { return checkpoint_reset_work_; }
const PlayerSteeringState& FlightPreview::presented_steering() const noexcept {
    return presented_steering_;
}
FlightPose FlightPreview::presented_pose() const noexcept { return presented_pose_; }
LifeCycleImage FlightPreview::presented_image() const noexcept { return presented_image_; }
const PlayerShotResult& FlightPreview::shot() const noexcept { return shot_; }
const ForegroundContactSnapshot& FlightPreview::last_consumed_contacts() const noexcept {
    return last_consumed_contacts_;
}

const PlayerContactResult& FlightPreview::last_player_contact() const noexcept {
    return last_player_contact_;
}
const std::vector<WorldObjectBandSchedule>&
FlightPreview::presented_object_bands() const noexcept {
    return presented_object_bands_;
}

void FlightPreview::initialize_players(std::size_t preset_index, RiverCheckpoint checkpoint) {
    const auto bridge = assets::session_presets::presets[preset_index].bridge_number;
    for (auto& player : players_) player = {0, bridge, checkpoint};
}

void FlightPreview::publish_presentation() noexcept {
    presented_steering_ = steering_;
    presented_pose_ = pose_;
    presented_image_ = life_cycle_.state().image;
}

void FlightPreview::reset_contact_presentation() noexcept {
    contact_pipeline_.reset();
    last_consumed_contacts_ = {};
    last_player_contact_ = {};
    ++contact_epoch_;
}
void FlightPreview::seed_object_presentation() {
    // A replaced world has no assigned raster bands yet. Seed only its own
    // current objects until the first pass supplies phased placements.
    presented_object_bands_ = schedule_world_object_snapshot(
        world_.river_state().phase, world_.object_state());
}
std::size_t FlightPreview::ordinary_hits() const noexcept { return ordinary_hits_; }
std::uint32_t FlightPreview::score() const noexcept {
    return players_[life_cycle_.state().active_player].score;
}
std::uint32_t FlightPreview::player_score(std::size_t player) const {
    return players_.at(player).score;
}
std::uint32_t FlightPreview::displayed_player_score(std::size_t player) const {
    return status_ == FlightStatus::Attract ? attract_display_scores_.at(player)
                                           : player_score(player);
}
std::size_t FlightPreview::selected_option() const noexcept { return selected_option_; }
bool FlightPreview::two_players() const noexcept { return (selected_option_ & 1U) != 0; }
std::uint32_t FlightPreview::high_score() const noexcept {
    return high_scores_[selected_option_ / 2];
}
std::uint16_t FlightPreview::fuel() const noexcept { return fuel_.amount; }
std::uint16_t FlightPreview::bridge_number() const noexcept {
    return players_[life_cycle_.state().active_player].bridge_number;
}
bool FlightPreview::bridge_flash() const noexcept {
    return status_ != FlightStatus::GameOver && (bridge_flash_ticks_ & 1U) != 0;
}
FlightStatus FlightPreview::status() const noexcept { return status_; }
int FlightPreview::lives() const noexcept { return life_cycle_.state().reserve_aircraft; }
const LifeCycleState& FlightPreview::life_cycle() const noexcept { return life_cycle_.state(); }
bool FlightPreview::invulnerable() const noexcept {
    return status_ == FlightStatus::Flying && life_cycle_.state().phase == LifeCyclePhase::AwaitInput;
}
bool FlightPreview::running() const noexcept {
    return status_ == FlightStatus::Flying || status_ == FlightStatus::Crashed ||
           status_ == FlightStatus::Preparing;
}

bool FlightPreview::accepts_session_input() const noexcept {
    return status_ != FlightStatus::Ready && status_ != FlightStatus::Blank &&
           !life_cycle_.controls_active();
}

const SessionIdleState& FlightPreview::idle_state() const noexcept { return session_idle_.state(); }

void FlightPreview::bridge_cleared(bool joint_contact) {
    bridge_flash_ticks_ = 23;
    auto& bridge = players_[life_cycle_.state().active_player].bridge_number;
    bridge = static_cast<std::uint16_t>((bridge + 1) % 10000);
    world_.mark_bridge_destroyed();
    world_.apply_bridge_sprite_destruction({joint_contact, bridge});
}

void FlightPreview::begin_crash(LifeEndAudioCause cause) {
    apply_life_transition(life_cycle_.begin_life_end(cause == LifeEndAudioCause::Collision
        ? LifeEndCause::Collision : LifeEndCause::FuelExhausted));
}

void FlightPreview::apply_life_transition(const LifeCycleTransition& transition) {
    if (transition.start_audio) {
        audio_.controller().start_life_end(*transition.start_audio == LifeEndCause::Collision
            ? LifeEndAudioCause::Collision : LifeEndAudioCause::FuelExhausted);
    }
    if (transition.clear_player_shot) clear_player_shot(shot_);
    if (transition.player_changed && transition.restore_checkpoint) {
        players_[life_cycle_.state().active_player ^ 1U].checkpoint = world_.commit_checkpoint();
    }
    if (transition.restore_checkpoint) {
        const auto checkpoint = transition.player_changed
            ? world_.restore_checkpoint(players_[life_cycle_.state().active_player].checkpoint)
            : world_.restore_checkpoint();
        reset_contact_presentation();
        fuel_ = {};
        steering_ = initial_steering();
        steering_.horizontal_position = checkpoint.player_horizontal_position;
        steering_.scroll_speed = kUnconditionalScrollSpeed;
        pose_ = FlightPose::Straight;
        publish_presentation();
        scroll_fraction_ = 0;
        world_.begin_life();
        seed_object_presentation();
    }
    if (transition.reveal_aircraft_and_clear_transients) {
        presented_steering_ = steering_;
        presented_pose_ = pose_;
        audio_.controller().reset_engine_ramp();
        world_.hide_life_transients();
        clear_player_shot(shot_);
    }
    if (transition.resume_controls) {
        pending_life_end_.reset();
        steering_.scroll_speed = 0x40;
    }
    if (transition.terminal) {
        checkpoint_reset_work_.reset();
        auto& record = high_scores_[selected_option_ / 2];
        record = std::max({record, players_[0].score, players_[1].score});
        session_idle_.begin_terminal();
        object_animation_phase_ = 0xff;
    }
    if (life_cycle_.state().image != LifeCycleImage::Straight) {
        presented_steering_ = steering_;
        presented_pose_ = pose_;
    }
    switch (life_cycle_.state().phase) {
    case LifeCyclePhase::Active:
    case LifeCyclePhase::AwaitInput: status_ = FlightStatus::Flying; break;
    case LifeCyclePhase::Exploding:
    case LifeCyclePhase::Rebuilding:
        status_ = life_cycle_.state().cause ? FlightStatus::Crashed : FlightStatus::Preparing;
        break;
    case LifeCyclePhase::Terminal:
        if (status_ != FlightStatus::OptionWait && status_ != FlightStatus::Attract)
            status_ = FlightStatus::GameOver;
        break;
    }
}

void FlightPreview::advance_reset_work_tick() {
    // Only supported frame-side publication continues. The checkpoint clear
    // remains atomic; progressive buffer writes and IRQ contacts are unmodeled.
    publish_presentation();
    if (bridge_flash_ticks_ != 0) --bridge_flash_ticks_;
    ++object_animation_phase_;
    if (object_animation_phase_ == 0 && idle_watchdog_.notify_phase_wrap()) {
        status_ = FlightStatus::Blank;
    }
    if (--checkpoint_reset_work_->presentation_ticks_remaining == 0) {
        const auto rows = checkpoint_reset_work_->deferred_scroll_rows;
        checkpoint_reset_work_.reset();
        for (std::uint8_t row = 0; row < rows; ++row)
            (void)world_.advance(contact_pipeline_.sampling_buffer());
    }
}

void FlightPreview::advance_tick() {
    if (keyboard_controls_ != FlightControls{} || joystick_controls_[0] != FlightControls{} ||
        joystick_controls_[1] != FlightControls{}) idle_watchdog_.note_activity();
    const bool was_attract = status_ == FlightStatus::Attract;
    const bool enter_attract = std::exchange(attract_pending_, false);
    const bool queue_attract = !enter_attract && session_idle_.advance(object_animation_phase_);
    if (was_attract && demo_startup_ticks_ != 0) {
        if (demo_startup_ticks_ == 1)
            steering_.scroll_speed = assets::demo_clock::steady_scroll_speed;
        --demo_startup_ticks_;
    }
    if (bridge_flash_ticks_ != 0) --bridge_flash_ticks_;
    auto contacts = contact_pipeline_.publish();
    last_consumed_contacts_ = contacts;
    auto top_objects = world_.object_state();
    auto contact_phase = world_.river_state().phase;
    auto contact_epoch = contact_epoch_;
    std::optional<std::uint8_t> activation_candidate;
    if (is_activation_phase(object_animation_phase_)) {
        activation_candidate = activation_candidate_from_sample(activation_source_.next());
    }
    const bool object_motion_enabled = world_.river_state().section >= 2 &&
        (life_cycle_.controls_active() || status_ == FlightStatus::Attract);
    world_.advance_motion({object_animation_phase_, object_motion_enabled,
                           activation_candidate});
    world_.advance_bridge_crossing(object_animation_phase_, life_cycle_.controls_active(),
                                   status_ == FlightStatus::Attract);

    if (enter_attract) {
        begin_attract();
        contacts = {};
        top_objects = world_.object_state();
        contact_phase = world_.river_state().phase;
        contact_epoch = contact_epoch_;
    } else if (queue_attract) {
        attract_pending_ = true;
        object_animation_phase_ = 0xff;
    }

    // Prior contacts are already published. Retain the image programmed by
    // the preceding pass before lifecycle changes this pass's private image.
    // This models sampled publication boundaries, not exact raster timing.
    publish_presentation();

    const auto transition = life_cycle_.advance(controls_.up || controls_.down || controls_.left ||
                                                controls_.right || controls_.fire);
    apply_life_transition(transition);
    if (contact_epoch != contact_epoch_) {
        // A restored world cannot consume mappings from the previous life.
        contacts = {};
        top_objects = world_.object_state();
        contact_phase = world_.river_state().phase;
        contact_epoch = contact_epoch_;
    }
    last_consumed_contacts_ = contacts;
    if (transition.restore_checkpoint) {
        // The reset follows this pass's audio/motion. First forced rows wait
        // until work completes; no extra foreground calls fill the interval.
        checkpoint_reset_work_ = CheckpointResetWorkState{
            kResetWorkPresentationTicks, transition.forced_scroll_rows};
        ++object_animation_phase_;
        if (object_animation_phase_ == 0 && idle_watchdog_.notify_phase_wrap()) {
            status_ = FlightStatus::Blank;
        }
        return;
    }
    if (life_cycle_.controls_active() && pending_life_end_) {
        const auto cause = *pending_life_end_;
        pending_life_end_.reset();
        world_.retire_bridge_crossing_for_pending_life_end();
        begin_crash(cause);
    }
    if (life_cycle_.controls_active()) {
        const auto consumption = consume_fuel(fuel_);
        fuel_ = consumption.state;
        if (consumption.exhausted) begin_crash(LifeEndAudioCause::FuelExhausted);
    }
    const bool active = life_cycle_.controls_active();
    if (active) {
        const auto result = steer_player(steering_, controls_);
        steering_ = result.state;
        pose_ = result.pose;
        shot_ = update_player_shot(shot_.state, controls_.fire);
        if (shot_.start_event) audio_.controller().start_shot();
    }

    std::optional<std::uint8_t> score_kind;
    if (contacts.shot_presented) {
        const auto resolution = resolve_shot_contacts(
            world_, shot_, contacts.shot, false);
        if (resolution.object_effect) {
            audio_.controller().start_explosion();
            if (resolution.selection.outcome == ShotContactOutcome::Bridge) {
                bridge_cleared(resolution.bridge_sprite_contact);
            }
            if (resolution.selection.outcome == ShotContactOutcome::Ordinary) ++ordinary_hits_;
            score_kind = resolution.object_effect->score_kind;
        }
    }

    PlayerContactResult player_contact;
    if (life_cycle_.state().image == LifeCycleImage::Straight) {
        player_contact = select_player_contact(contacts.player, world_.object_state());
        player_contact.terrain_contact |= contacts.player.player_fully_offscreen;
    }
    last_player_contact_ = player_contact;
    if (player_contact.refuel_contact) {
        const bool saturates = fuel_.amount > kFullFuel - kRefuelPerContact;
        fuel_ = refuel_fuel(fuel_);
        audio_.controller().start_refuel(saturates ? RefuelAudioKind::TankSaturated
                                                 : RefuelAudioKind::FuelAdded);
    }

    bool gap_consumer_reached = !player_contact.contacted() && !player_contact.refuel_contact;
    if (player_contact.bridge) {
        const auto effect = world_.apply_projectile_hit(
            player_contact.bridge->record_index, player_contact.bridge->slot, true);
        if (effect) {
            audio_.controller().start_explosion();
            bridge_cleared(player_contact.bridge->joint_contact);
            if (const auto write = bridge_score_kind_write(
                    player_contact.bridge->joint_contact, true))
                score_kind = write;
            clear_player_shot(shot_);
            const auto after_bridge = select_player_contact(contacts.player, world_.object_state());
            gap_consumer_reached = !after_bridge.contacted() && !after_bridge.refuel_contact &&
                                   !contacts.player.player_fully_offscreen;
        }
    }
    if (player_contact.object_contact) {
        const auto effect = world_.apply_projectile_hit(
            player_contact.object_contact->record_index,
            player_contact.object_contact->slot, true);
        if (effect) audio_.controller().start_explosion();
    }

    GapContactSelection gap_contact;
    if (gap_consumer_reached) {
        gap_contact = select_gap_contact(world_.bridge_sprite_state().gap_animation_counter,
                                         contacts.gap, world_.object_state());
        if (gap_contact.object) {
            (void)world_.apply_gap_contact_hit(gap_contact.object->record_index,
                                              gap_contact.object->slot);
        }
    }

    world_.advance_bridge_gap(object_animation_phase_, active);
    const auto auxiliary = world_.advance_auxiliary_projectile(
        active, status_ == FlightStatus::Attract,
        audio_.state().auxiliary_sprite_countdown == 0,
        contacts.auxiliary.terrain, contacts.auxiliary.player);
    if (auxiliary.audio_started) audio_.controller().start_auxiliary_sprite_effect();
    ++object_animation_phase_;

    if (score_kind) {
        auto& score = players_[life_cycle_.state().active_player].score;
        const auto award = award_object_score(score, *score_kind);
        score = award.total;
        if (award.extra_life && life_cycle_.add_reserve_aircraft()) {
            audio_.controller().award_extra_life();
        }
        if (award.exhausted) {
            life_cycle_.exhaust_reserves();
            pending_life_end_ = LifeEndAudioCause::Collision;
        }
    }

    // Retain this pass's geometric contacts for the next foreground consumer.
    // The top band uses pre-motion records; later bands use post-effect records.
    // This is pass ordering, not a raster-exact sprite/latch model.
    presented_object_bands_ = contact_pipeline_.produce(
        world_, top_objects, contact_phase, presented_steering_,
        presented_pose_, shot_, presented_image_);

    if (active || status_ == FlightStatus::Attract) {
        for (int attempt = 0; attempt < kScrollAttemptsPerTick; ++attempt) advance_scroll_attempt();
    } else {
        for (std::uint8_t row = 0; row < transition.forced_scroll_rows; ++row)
            (void)world_.advance(contact_pipeline_.sampling_buffer());
    }

    const bool fatal_contact = player_contact.contacted() ||
                               gap_contact.fatal_player_contact || auxiliary.fatal_player_contact;
    if (active && fatal_contact) {
        pending_life_end_ = LifeEndAudioCause::Collision;
    }
    // The queued attract handoff publishes its wrap on entry, not twice.
    if (!queue_attract && object_animation_phase_ == 0 && idle_watchdog_.notify_phase_wrap()) {
        status_ = FlightStatus::Blank;
    }
}

void FlightPreview::advance_scroll_attempt() {
    bool scroll = steering_.scroll_speed >= kUnconditionalScrollSpeed;
    if (!scroll) {
        const auto sum = static_cast<unsigned>(scroll_fraction_) + steering_.scroll_speed;
        scroll_fraction_ = static_cast<std::uint8_t>(sum);
        scroll = sum > 0xFFU;
    }
    if (!scroll) return;

    (void)world_.advance(contact_pipeline_.sampling_buffer());
}

} // namespace river_raid
