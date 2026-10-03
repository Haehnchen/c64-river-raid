#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "support/flight_start_helpers.hpp"
#include "game/fuel.hpp"
#include "support/long_session_inputs.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

FlightControls controls_for(const test::SessionInputRun& run, bool down = false) {
    return {false, down, run.direction == 2, run.direction == 1, run.fire};
}

std::size_t advance_slice(FlightStart& launch, FlightPreview& flight,
                          FlightControls controls) {
    launch.set_controls(controls);
    const auto admitted = launch.advance_elapsed(20ms);
    require(admitted <= 2, "Route slice admitted more than two updates");
    (void)flight.take_audio();
    return admitted;
}

void advance_one_update(FlightStart& launch, FlightPreview& flight) {
    launch.set_controls({});
    for (int half = 0; half < 4; ++half) {
        const auto admitted = launch.advance_elapsed(10ms);
        require(admitted <= 1, "Ten-millisecond life-cycle slice admitted multiple updates");
        (void)flight.take_audio();
        if (admitted == 1) return;
    }
    throw std::runtime_error("Life-cycle update did not arrive within four half-slices");
}

void test_fuel_death_preserves_joint_crossing() {
    FlightPreview flight;
    constexpr std::array options{StartOption{"1"}};
    StartFlow flow(options);
    FlightStart launch(flow, flight);
    const auto press_f1 = [&launch] {
        launch.set_button(StartButton::Proceed, true);
        launch.set_button(StartButton::Proceed, false);
    };
    press_f1();
    press_f1();
    require(flow.stage() == StartStage::StartRequested &&
                flight.status() == FlightStatus::Preparing &&
                flight.selected_option() == 0 && !flight.two_players(),
            "F1 did not prepare the normal Game 1 session");
    test::wait_for_launch(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.status() == FlightStatus::Flying &&
                flight.world().generated_rows() == 156 &&
                flight.bridge_number() == 1 && flight.lives() == 3,
            "Normal launch did not reach its first input gate");

    int whole_slices = 0, whole_updates = 0;
    bool first_bridge_hit = false;
    for (const auto run : test::first_bridge_session_prefix) {
        for (int slice = 0; slice < run.slices; ++slice) {
            whole_updates += static_cast<int>(advance_slice(launch, flight, controls_for(run)));
            ++whole_slices;
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "First bridge route lost its first life");
            if (flight.bridge_number() == 2) {
                first_bridge_hit = true;
                break;
            }
        }
        if (first_bridge_hit) break;
    }
    require(first_bridge_hit && whole_slices == 883 && whole_updates == 885 &&
                flight.bridge_number() == 2 && flight.score() == 1000 &&
                flight.world().generated_rows() == 1033 && flight.fuel() == 39455,
            "First bridge route changed before the Down input window");

    // The original horizontal/fire route is held; Down begins on whole-route slice 2005.
    int second_slices = 0, second_updates = 0;
    std::size_t bridge_hit_admitted = 0;
    std::uint32_t score_before_bridge_hit = 0;
    for (const auto run : test::second_bridge_session_suffix) {
        for (int slice = 0; slice < run.slices; ++slice) {
            const bool down = second_slices >= 1121;
            score_before_bridge_hit = flight.score();
            bridge_hit_admitted = advance_slice(launch, flight, controls_for(run, down));
            second_updates += static_cast<int>(bridge_hit_admitted);
            ++second_slices;
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "Second bridge route lost its first life");
            require(flight.bridge_number() == (second_slices == 1129 ? 3 : 2),
                    "Second bridge clear moved outside its observed update");
        }
    }
    bool joint_shot_contact = false;
    const auto& hit = flight.last_consumed_contacts();
    for (const auto& slot : hit.shot)
        joint_shot_contact |= hit.shot_presented && slot.object_contact &&
            slot.bridge_sprite_present && (slot.primary_present || slot.secondary_present);
    const auto bridge_private = flight.world().bridge_sprite_state().crossing;
    const auto bridge_published = flight.world().bridge_presentation_state().crossing;
    require(whole_slices + second_slices == 2012 &&
                whole_updates + second_updates == 2017 &&
                second_slices == 1129 && second_updates == 1132 &&
                bridge_hit_admitted == 1 && joint_shot_contact &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.world().generated_rows() == 2164 && flight.fuel() == 3231 &&
                score_before_bridge_hit == 1440 &&
                flight.score() - score_before_bridge_hit == 750 &&
                flight.score() == 2190 && flight.bridge_number() == 3 &&
                flight.bridge_flash() &&
                flight.shot().pose == PlayerShotPose::Hidden &&
                flight.shot().state.vertical_position == 0 &&
                flight.audio_state().explosion_countdown == 31 &&
                bridge_private && bridge_private->image == BridgeSpriteImage::JointExplosionA &&
                bridge_private->vic_x == 180 && bridge_private->vic_y == 149 &&
                bridge_published && bridge_published->image == BridgeSpriteImage::CrossingLeftA &&
                bridge_published->vic_x == 180 && bridge_published->vic_y == 148,
            "Ordinary Down route did not produce the observed bridge joint hit");

    int fuel_slices = 0, fuel_updates = 0;
    for (const auto run : test::fuel_exhaustion_session_suffix) {
        for (int slice = 0; slice < run.slices; ++slice) {
            const auto before_private = flight.world().bridge_sprite_state().crossing;
            const auto before_published = flight.world().bridge_presentation_state().crossing;
            const auto before_shot = flight.shot();
            const auto before_fuel = flight.fuel();
            const auto admitted = advance_slice(launch, flight, controls_for(run, true));
            fuel_updates += static_cast<int>(admitted);
            ++fuel_slices;
            require(admitted == 1, "Fuel suffix did not expose one update per observed slice");
            if (fuel_slices < 101) {
                require(flight.status() == FlightStatus::Flying &&
                            flight.life_cycle().phase == LifeCyclePhase::Active &&
                            flight.lives() == 3 && flight.bridge_number() == 3,
                        "Fuel route ended before its natural dry-tank update");
                continue;
            }
            const auto after_private = flight.world().bridge_sprite_state().crossing;
            const auto after_published = flight.world().bridge_presentation_state().crossing;
            require(fuel_slices == 101 && fuel_updates == 101 &&
                        before_fuel == 31 && flight.fuel() == 31 &&
                        flight.status() == FlightStatus::Crashed &&
                        flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.life_cycle().cause == LifeEndCause::FuelExhausted &&
                        flight.life_cycle().timer == 0xfe &&
                        flight.life_cycle().image == LifeCycleImage::ExplosionA &&
                        flight.presented_image() == LifeCycleImage::Straight &&
                        flight.bridge_number() == 3 && flight.score() == 2270 &&
                        flight.lives() == 3 && flight.world().generated_rows() == 2219 &&
                        !flight.last_player_contact().contacted() &&
                        flight.audio_state().voice1_effect == Voice1AudioEffect::FuelExhausted &&
                        before_shot.pose == PlayerShotPose::Visible &&
                        before_shot.state.vertical_position == 176 &&
                        flight.shot().pose == PlayerShotPose::Hidden &&
                        flight.shot().state.vertical_position == 0 &&
                        flight.shot().vertical_position_write == 0,
                    "Natural fuel death changed its one-update lifecycle or audio endpoint");
            require(before_private && after_private &&
                        before_private->image == BridgeSpriteImage::JointExplosionA &&
                        after_private->image == BridgeSpriteImage::JointExplosionA &&
                        before_private->vic_x == 180 && after_private->vic_x == 180 &&
                        before_private->vic_y == 204 && after_private->vic_y == 204 &&
                        before_published && after_published &&
                        before_published->image == BridgeSpriteImage::JointExplosionA &&
                        after_published->image == BridgeSpriteImage::JointExplosionA &&
                        before_published->vic_x == 180 && after_published->vic_x == 180 &&
                        before_published->vic_y == 203 && after_published->vic_y == 204,
                    "Fuel death did not preserve the private and published joint crossing");
        }
    }
    require(fuel_slices == 101 && fuel_updates == 101,
            "Fuel suffix did not finish at the observed natural death update");
    const auto checkpoint = flight.world().selected_checkpoint();

    advance_one_update(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                flight.life_cycle().timer == 0xfd &&
                flight.presented_image() == LifeCycleImage::ExplosionA &&
                flight.world().generated_rows() == 2219 &&
                flight.world().bridge_sprite_state().crossing &&
                flight.world().bridge_presentation_state().crossing &&
                flight.world().bridge_sprite_state().crossing->image ==
                    BridgeSpriteImage::JointExplosionA &&
                flight.world().bridge_presentation_state().crossing->image ==
                    BridgeSpriteImage::JointExplosionA,
            "First post-death update lost the preserved joint or delayed aircraft image");
    for (int update = 1; update < 127; ++update) advance_one_update(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.status() == FlightStatus::Crashed &&
                flight.fuel() == kInitialFuel && flight.lives() == 3 &&
                flight.score() == 2270 && flight.bridge_number() == 3 &&
                flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                flight.shot().pose == PlayerShotPose::Hidden &&
                flight.shot().state.vertical_position == 0 &&
                RiverCheckpoint{flight.world().river_state().section,
                                flight.world().river_state().section_anchor} == checkpoint,
            "127-update reset lost the selected checkpoint or session progress");
    for (int update = 0; update < 83; ++update) advance_one_update(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.status() == FlightStatus::Flying && flight.invulnerable() &&
                flight.fuel() == kInitialFuel && flight.lives() == 2 &&
                flight.score() == 2270 && flight.bridge_number() == 3 &&
                flight.world().generated_rows() == 156 &&
                RiverCheckpoint{flight.world().river_state().section,
                                flight.world().river_state().section_anchor} == checkpoint,
            "83-update reveal did not retain checkpoint, score and remaining reserves");
    std::cout << "fuel_joint_session bridge_slices=2012 bridge_updates=2017"
                 " dry_suffix_slices=101 dry_suffix_updates=101 rows=2219"
                 " private_and_published_joint_preserved=1 reset_updates=127"
                 " reveal_updates=83 native_coverage_only=true\n";
}

} // namespace

int main() {
    try {
        test_fuel_death_preserves_joint_crossing();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
