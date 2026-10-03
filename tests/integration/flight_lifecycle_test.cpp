#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "app/player_contact.hpp"
#include "support/flight_start_helpers.hpp"
#include "game/life_cycle.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void advance_one_tick(FlightPreview& preview) {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto completed = preview.advance_elapsed(10ms);
        require(completed <= 1, "A ten-millisecond slice completed more than one tick");
        if (completed == 1) return;
    }
    throw std::runtime_error("A bounded PAL interval did not complete one tick");
}

constexpr FlightControls neutral{};
constexpr FlightControls bank_right{false, false, false, true, false};

void start_from_title(FlightStart& launch) {
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
}

void test_first_reveal_allocation_and_clearing() {
    constexpr std::array options{StartOption{"1"}};
    StartFlow flow(options);
    FlightPreview preview;
    FlightStart launch(flow, preview);
    launch.set_controls(neutral);
    start_from_title(launch);
    for (int update = 0; update < 128 && preview.life_cycle().timer != 0x32; ++update)
        advance_one_tick(preview);
    require(preview.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                preview.life_cycle().timer == 0x32 && preview.lives() == 4 &&
                preview.life_cycle().image == LifeCycleImage::Hidden &&
                preview.presented_image() == LifeCycleImage::Hidden,
            "Normal first start did not reach the hidden pre-reveal boundary");

    const auto require_cleared = [&] {
        require(preview.shot().state.vertical_position == 0 &&
                    preview.shot().pose == PlayerShotPose::Hidden,
                "First reveal retained a player shot");
        const auto auxiliary = preview.world().auxiliary_projectile_state();
        require(auxiliary.vertical_position == 0 &&
                    (!auxiliary.presentation || auxiliary.presentation->vic_y == 0),
                "First reveal retained auxiliary stored or published Y");
        for (const auto state : {preview.world().bridge_sprite_state(),
                                 preview.world().bridge_presentation_state()}) {
            require((!state.crossing || state.crossing->vic_y == 0) &&
                        (!state.gap || state.gap->vic_y == 0),
                    "First reveal retained crossing or gap Y");
        }
    };
    advance_one_tick(preview);
    require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                preview.life_cycle().timer == 0x31 && preview.lives() == 3 &&
                preview.life_cycle().image == LifeCycleImage::Straight &&
                preview.presented_image() == LifeCycleImage::Hidden &&
                preview.shot().vertical_position_write == 0,
            "First reveal did not allocate once and clear before image publication");
    require_cleared();

    // Native hidden initializers need not reproduce the original attract leftovers.
    // This checks the normal-start endpoint and waiting stability, not live hits.
    const auto rows = preview.world().generated_rows();
    for (int update = 0; update < 8; ++update) {
        advance_one_tick(preview);
        require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    preview.life_cycle().timer == 0x31 && preview.lives() == 3 &&
                    preview.presented_image() == LifeCycleImage::Straight &&
                    preview.world().generated_rows() == rows,
                "Neutral first-launch wait repeated allocation or advanced rebuilding");
        require_cleared();
    }
}

void force_crash(FlightPreview& preview) {
    preview.set_controls(bank_right);
    for (std::size_t tick = 0; tick < 2000; ++tick) {
        advance_one_tick(preview);
        if (preview.status() == FlightStatus::Crashed ||
            preview.status() == FlightStatus::GameOver) {
            return;
        }
        require(preview.status() == FlightStatus::Flying,
                "Natural bank path left the active flight unexpectedly");
    }
    throw std::runtime_error("Held right input did not produce a natural bank contact");
}

bool explosion_image(LifeCycleImage image) {
    return image == LifeCycleImage::ExplosionA || image == LifeCycleImage::ExplosionB;
}

// Drive one collision through its complete non-terminal lifecycle. Controls are
// neutral during the rebuild so AwaitInput remains observable.
void finish_nonterminal_lifecycle(FlightPreview& preview) {
    preview.set_controls(neutral);
    require(preview.status() == FlightStatus::Crashed &&
                preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                preview.life_cycle().image == LifeCycleImage::ExplosionA &&
                preview.presented_image() == LifeCycleImage::Straight &&
                preview.life_cycle().timer == 0xfe &&
                preview.life_cycle().image_changes_remaining == 4 &&
                preview.audio_state().voice1_effect == Voice1AudioEffect::Collision &&
                preview.audio_state().voice1_countdown == 24,
            "Collision did not enter private explosion A while retaining the prior image");

    const auto crash_steering = preview.steering();
    const auto crash_fuel = preview.fuel();
    advance_one_tick(preview);
    require(preview.status() == FlightStatus::Crashed &&
                preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                preview.life_cycle().image == LifeCycleImage::ExplosionA &&
                preview.presented_image() == LifeCycleImage::ExplosionA &&
                preview.life_cycle().timer == 0xfd &&
                preview.steering() == crash_steering && preview.fuel() == crash_fuel,
            "Explosion image did not catch up on the next pass or controls advanced during it");

    unsigned ticks_after_entry = 1;
    unsigned visible_explosion_publications =
        explosion_image(preview.presented_image()) ? 1U : 0U;
    while (preview.life_cycle().phase == LifeCyclePhase::Exploding) {
        advance_one_tick(preview);
        ++ticks_after_entry;
        if (explosion_image(preview.presented_image())) ++visible_explosion_publications;
    }
    require(ticks_after_entry == 127,
            "Explosion did not reach rebuild after 127 subsequent ticks");
    require(visible_explosion_publications == 40,
            "Life-end explosion did not publish exactly 40 visible images");
    require(preview.status() == FlightStatus::Crashed &&
                preview.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                preview.life_cycle().image == LifeCycleImage::Hidden,
            "Explosion did not enter the rebuilding phase");
    require(preview.last_consumed_contacts() == ForegroundContactSnapshot{},
            "Checkpoint restoration consumed contacts from the previous world");
    require(preview.fuel() == kInitialFuel,
            "Partial checkpoint restore did not refill fuel at the rebuild boundary");

    unsigned rebuild_publications = 1; // The Exploding -> Rebuilding foreground pass.
    unsigned reset_work_ticks = 0;
    unsigned lifecycle_ticks = ticks_after_entry;
    while (preview.life_cycle().phase != LifeCyclePhase::AwaitInput) {
        const auto before = preview.life_cycle().phase;
        const auto before_timer = preview.life_cycle().timer;
        const bool busy = preview.checkpoint_reset_work().has_value();
        const auto before_rows = preview.world().generated_rows();
        advance_one_tick(preview);
        ++lifecycle_ticks;
        if (busy) {
            ++reset_work_ticks;
            require(preview.life_cycle().timer == before_timer &&
                        (reset_work_ticks == 5 ||
                         preview.world().generated_rows() == before_rows),
                    "Reset-work interval admitted a rebuilding pass or early rows");
        }
        if (before == LifeCyclePhase::Rebuilding &&
            preview.life_cycle().phase == LifeCyclePhase::Rebuilding &&
            preview.life_cycle().timer != before_timer) {
            ++rebuild_publications;
        }
        require(preview.status() == FlightStatus::Crashed ||
                    preview.status() == FlightStatus::Flying,
                "Rebuild entered an unexpected flight status");
        require(preview.fuel() == kInitialFuel,
                "Rebuild changed the refilled tank before resume");
    }
    require(reset_work_ticks == 5 && rebuild_publications == 78 &&
                preview.world().generated_rows() == 156,
            "Rebuild did not complete five busy ticks and 156 rows over 78 foreground passes");
    require(lifecycle_ticks == 210 && preview.status() == FlightStatus::Flying &&
                preview.life_cycle().image == LifeCycleImage::Straight &&
                preview.invulnerable(),
            "Aircraft was not revealed on lifecycle tick 210");
}

void resume_with_held_input(FlightPreview& preview) {
    require(preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                preview.status() == FlightStatus::Flying && preview.invulnerable(),
            "Non-terminal crash did not wait for input");
    const auto before_fuel = preview.fuel();
    preview.set_controls(bank_right);
    advance_one_tick(preview);
    require(preview.life_cycle().phase == LifeCyclePhase::Active &&
                preview.status() == FlightStatus::Flying && !preview.invulnerable() &&
                preview.fuel() == before_fuel - kFuelDrainPerTick,
            "Held input did not resume active flight on the same pass");
}

void test_reserve_aircraft_and_terminal_explosion() {
    FlightPreview preview;
    require(preview.status() == FlightStatus::Ready &&
                preview.life_cycle().phase == LifeCyclePhase::Active &&
                preview.lives() == kInitialReserveAircraft &&
                preview.world().generated_rows() == 0 && !preview.running(),
            "Preview did not begin with an empty four-aircraft reserve");

    constexpr std::array options{StartOption{"1"}};
    StartFlow flow(options);
    FlightStart launch(flow, preview);
    launch.set_controls(bank_right);
    start_from_title(launch);
    require(preview.status() == FlightStatus::Preparing && preview.running() &&
                preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                preview.life_cycle().image == LifeCycleImage::Hidden &&
                preview.life_cycle().timer == 0x80 &&
                preview.lives() == kInitialReserveAircraft &&
                preview.world().generated_rows() == 0,
            "Starting a flight did not enter the source-backed launch preparation");
    test::wait_for_launch(preview);
    require(preview.status() == FlightStatus::Flying &&
                preview.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                preview.life_cycle().image == LifeCycleImage::Straight && preview.lives() == 3 &&
                preview.world().generated_rows() == 156 && preview.invulnerable() &&
                preview.pose() == FlightPose::Straight,
            "Launch preparation did not reveal the aircraft after reset work");
    advance_one_tick(preview);
    require(preview.life_cycle().phase == LifeCyclePhase::Active &&
                preview.status() == FlightStatus::Flying && !preview.invulnerable() &&
                preview.pose() == FlightPose::Right,
            "Held controls resumed launch on the wrong pass");

    for (int reserve = preview.lives(); reserve > 0; --reserve) {
        force_crash(preview);
        require(preview.status() == FlightStatus::Crashed &&
                    preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                    preview.lives() == reserve,
                "Collision decremented reserve aircraft at explosion entry");
        finish_nonterminal_lifecycle(preview);
        require(preview.lives() == reserve - 1,
                "Reserve aircraft did not decrement when the aircraft was revealed");
        resume_with_held_input(preview);
    }

    force_crash(preview);
    require(preview.status() == FlightStatus::Crashed &&
                preview.life_cycle().phase == LifeCyclePhase::Exploding && preview.lives() == 0,
            "Final collision did not enter its terminal explosion with zero reserves");
    preview.set_controls(neutral);
    unsigned final_ticks = 0;
    while (preview.status() == FlightStatus::Crashed) {
        advance_one_tick(preview);
        ++final_ticks;
    }
    require(final_ticks == 127 && preview.status() == FlightStatus::GameOver &&
                preview.life_cycle().phase == LifeCyclePhase::Terminal && preview.lives() == 0 &&
                !preview.running(),
            "Final death did not remain visible for 127 ticks before Game Over");

    // Exercise F1/start wiring, including controls held at restart.
    launch.set_controls(bank_right);
    launch.set_button(StartButton::Proceed, true);
    launch.set_button(StartButton::Proceed, false);
    require(preview.status() == FlightStatus::Preparing && preview.running() &&
                preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                preview.life_cycle().image == LifeCycleImage::Hidden &&
                preview.lives() == kInitialReserveAircraft && !preview.invulnerable() &&
                preview.world().generated_rows() == 0,
            "F1/start did not reset terminal lifecycle state");
    test::wait_for_launch(preview);
    advance_one_tick(preview);
    require(preview.life_cycle().phase == LifeCyclePhase::Active &&
                preview.pose() == FlightPose::Right,
            "Held steering controls were discarded by F1/start");
}

void test_neutral_flight_enemy_collision() {
    FlightPreview preview;
    preview.start();
    preview.set_controls(neutral);
    test::wait_for_launch(preview);
    preview.set_controls({false, false, false, false, true});
    advance_one_tick(preview);
    (void)preview.take_audio();
    preview.set_controls(neutral);
    require(preview.status() == FlightStatus::Flying &&
                preview.life_cycle().phase == LifeCyclePhase::Active && preview.lives() == 3,
            "Initial fire pulse did not resume the prepared flight");

    auto generating_objects = preview.world().object_state();
    for (int pass = 0; pass < 1000 && preview.status() == FlightStatus::Flying; ++pass) {
        const auto objects_before = preview.world().object_state();
        const auto rows_before = preview.world().generated_rows();
        // Predict only the next ordinary neutral production pass on a copy.
        // The real flight remains the single source of outcome/state; this
        // lookahead selects a one-pass fire pulse without injecting state.
        auto predictor = preview;
        predictor.set_controls(neutral);
        advance_one_tick(predictor);
        const auto predicted_contact = select_player_contact(
            predictor.last_consumed_contacts().player, objects_before);
        const bool fire_on_contact_pass = predictor.status() == FlightStatus::Flying &&
                                          predicted_contact.object_contact.has_value();
        preview.set_controls(fire_on_contact_pass
                                 ? FlightControls{false, false, false, false, true}
                                 : neutral);
        advance_one_tick(preview);
        (void)preview.take_audio();
        // The retained generation is consumed before rows and hit mutation;
        // the resulting rendered world is not that generation's geometry.
        const auto contact = select_player_contact(
            preview.last_consumed_contacts().player, objects_before);
        if (!contact.object_contact) {
            require(preview.status() == FlightStatus::Flying && preview.lives() == 3,
                    "Neutral route crashed before its consumed enemy contact");
            generating_objects = objects_before;
            continue;
        }
        require(fire_on_contact_pass && predicted_contact.object_contact &&
                    predicted_contact.object_contact->record_index ==
                        contact.object_contact->record_index,
                "Copied ordinary-tick lookahead missed the natural fatal contact");
        // This natural generation crosses a history shift: the former tail
        // expires and record 10 becomes 11 before the stored contact is consumed.
        require(generating_objects.active_begin < kWorldObjectCapacity &&
                    generating_objects.records.back().vertical_position == 211 &&
                    objects_before.records[11].vertical_position ==
                        generating_objects.records[10].vertical_position + 1 &&
                    objects_before.records[11].base_kind ==
                        generating_objects.records[10].base_kind,
                "Enemy contact route did not cross its expected object-history shift");
        require(contact.object_contact->record_index == 11 &&
                    preview.status() == FlightStatus::Flying && preview.lives() == 3 &&
                    preview.world().generated_rows() > rows_before && preview.score() == 0 &&
                    preview.shot().pose == PlayerShotPose::Visible &&
                    preview.shot().state.vertical_position == 0xc0 &&
                    preview.shot().start_event.has_value(),
                "Consumed aircraft contact did not complete its current row loop: record=" +
                    std::to_string(contact.object_contact->record_index) +
                    " status=" + std::to_string(static_cast<int>(preview.status())) +
                    " lives=" + std::to_string(preview.lives()) +
                    " rows=" + std::to_string(rows_before) + "->" +
                    std::to_string(preview.world().generated_rows()) +
                    " score=" + std::to_string(preview.score()));
        const auto contact_rows = preview.world().generated_rows();
        preview.set_controls({false, false, false, false, true});
        advance_one_tick(preview);
        (void)preview.take_audio();
        require(preview.status() == FlightStatus::Crashed &&
                    preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                    preview.life_cycle().image == LifeCycleImage::ExplosionA &&
                    preview.presented_image() == LifeCycleImage::Straight &&
                    preview.life_cycle().timer == 0xfe &&
                    preview.lives() == 3 && preview.world().generated_rows() == contact_rows &&
                    preview.shot().pose == PlayerShotPose::Hidden &&
                    preview.shot().state.vertical_position == 0 &&
                    preview.shot().vertical_position_write == 0 &&
                    preview.audio_state().voice1_effect == Voice1AudioEffect::Collision &&
                    preview.audio_state().voice1_countdown == 24,
                "Pending aircraft life-end was not consumed before the next row loop");

        const auto crash_steering = preview.steering();
        const auto crash_fuel = preview.fuel();
        preview.set_controls(neutral);
        advance_one_tick(preview);
        require(preview.status() == FlightStatus::Crashed && preview.lives() == 3 &&
                    preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                    preview.life_cycle().image == LifeCycleImage::ExplosionA &&
                    preview.presented_image() == LifeCycleImage::ExplosionA &&
                    preview.life_cycle().timer == 0xfd &&
                    preview.steering() == crash_steering && preview.fuel() == crash_fuel,
                "Immediate crash controls or next-pass presentation gate changed");
        require(preview.last_consumed_contacts().player != PlayerContactSnapshot{},
                "Crash-entry pass did not consume the retained straight-image contact generation");
        advance_one_tick(preview);
        require(preview.status() == FlightStatus::Crashed && preview.lives() == 3 &&
                    preview.life_cycle().phase == LifeCyclePhase::Exploding &&
                    preview.presented_image() == LifeCycleImage::ExplosionA &&
                    preview.last_consumed_contacts().player == PlayerContactSnapshot{},
                "Explosion-image generation retained straight-aircraft contact data");
        return;
    }
    throw std::runtime_error("Neutral input did not reach a consumed enemy contact");
}

} // namespace

int main() {
    try {
        test_first_reveal_allocation_and_clearing();
        test_reserve_aircraft_and_terminal_explosion();
        test_neutral_flight_enemy_collision();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
