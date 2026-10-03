#include "game/life_cycle.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace river_raid;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

LifeCycleTransition advance_to_rebuild(LifeCycle& life) {
    LifeCycleTransition transition;
    for (int pass = 1; pass <= 127; ++pass) {
        transition = life.advance(false);
        if (pass != 127) {
            require(life.state().phase == LifeCyclePhase::Exploding,
                    "Explosion phase ended before the source boundary");
            require(!transition.restore_checkpoint && transition.forced_scroll_rows == 0,
                    "Explosion pass requested an early rebuild");
        }
    }
    return transition;
}

void finish_rebuild_and_resume(LifeCycle& life) {
    require(life.state().phase == LifeCyclePhase::Rebuilding,
            "Expected a rebuilding life before reveal");
    for (int pass = 0; pass < 78; ++pass) (void)life.advance(false);
    require(life.state().phase == LifeCyclePhase::AwaitInput,
            "Rebuild did not reveal an aircraft");
    require(life.advance(true).resume_controls && life.controls_active(),
            "Revealed aircraft did not resume controls");
}

void test_initial_round_rebuild() {
    LifeCycle life;
    life.start_round();
    require(life.state().phase == LifeCyclePhase::Exploding &&
                life.state().image == LifeCycleImage::Hidden &&
                life.state().timer == 0x80 &&
                life.state().reserve_aircraft == kInitialReserveAircraft &&
                life.state().inactive_reserve_aircraft == 0 &&
                life.state().active_player == 0 &&
                life.state().player_mode == LifeCyclePlayerMode::Single &&
                !life.state().cause,
            "Round start did not seed the hidden source lifecycle");

    auto transition = life.advance(false);
    require(transition.restore_checkpoint && transition.forced_scroll_rows == 2 &&
                !transition.start_audio && !transition.clear_player_shot &&
                !transition.player_changed,
            "First round-start pass did not enter checkpoint rebuilding");
    require(life.state().phase == LifeCyclePhase::Rebuilding &&
                life.state().timer == 0x7f &&
                life.state().reserve_aircraft == kInitialReserveAircraft,
            "First round-start pass changed its seed prematurely");

    unsigned rows = transition.forced_scroll_rows;
    for (int pass = 0; pass < 77; ++pass) {
        transition = life.advance(false);
        rows += transition.forced_scroll_rows;
    }
    require(rows == 156 && life.state().timer == 0x32,
            "Initial round did not rebuild exactly 156 rows");

    transition = life.advance(false);
    require(transition.reveal_aircraft_and_clear_transients &&
                transition.reserve_decremented &&
                life.state().phase == LifeCyclePhase::AwaitInput &&
                life.state().reserve_aircraft == 3,
            "Initial reveal did not consume one of four total aircraft");
}

void test_collision_entry_and_images() {
    LifeCycle life;
    require(life.controls_active() && life.state().reserve_aircraft == 4 &&
                life.state().image == LifeCycleImage::Straight,
            "Initial life-cycle state differs");

    const auto entry = life.begin_life_end(LifeEndCause::Collision);
    require(entry.start_audio == LifeEndCause::Collision && entry.clear_player_shot,
            "Collision entry omitted its immediate events");
    require(life.state().phase == LifeCyclePhase::Exploding &&
                life.state().timer == 0xfe &&
                life.state().image == LifeCycleImage::ExplosionA &&
                life.state().image_changes_remaining == 4,
            "Collision entry advanced or seeded the wrong explosion state");
    require(life.begin_life_end(LifeEndCause::FuelExhausted) == LifeCycleTransition{},
            "An active life end was restarted");

    unsigned visible_a = 1;
    unsigned visible_b = 0;
    for (int pass = 1; pass <= 127; ++pass) {
        const auto transition = life.advance(false);
        if (life.state().image == LifeCycleImage::ExplosionA) ++visible_a;
        if (life.state().image == LifeCycleImage::ExplosionB) ++visible_b;

        if (pass == 7) {
            require(life.state().image == LifeCycleImage::ExplosionA,
                    "First collision frame lasted fewer than eight publications");
        } else if (pass == 8) {
            require(life.state().image == LifeCycleImage::ExplosionB &&
                        life.state().image_changes_remaining == 3,
                    "First collision image boundary differs");
        } else if (pass == 16) {
            require(life.state().image == LifeCycleImage::ExplosionA &&
                        life.state().image_changes_remaining == 2,
                    "Second collision image boundary differs");
        } else if (pass == 24) {
            require(life.state().image == LifeCycleImage::ExplosionB &&
                        life.state().image_changes_remaining == 1,
                    "Third collision image boundary differs");
        } else if (pass == 32) {
            require(life.state().image == LifeCycleImage::ExplosionA &&
                        life.state().image_changes_remaining == 0,
                    "Fourth collision image boundary differs");
        } else if (pass == 40) {
            require(life.state().image == LifeCycleImage::Hidden,
                    "Collision image did not retire after 40 publications");
        }

        if (pass == 127) {
            require(transition.restore_checkpoint && transition.forced_scroll_rows == 2 &&
                        !transition.player_changed && life.state().active_player == 0 &&
                        life.state().inactive_reserve_aircraft == 0,
                    "Single-player completion changed player or missed rebuilding");
        }
    }
    require(visible_a == 24 && visible_b == 16,
            "Collision did not publish the exact 40-frame image sequence");
    require(life.state().phase == LifeCyclePhase::Rebuilding &&
                life.state().timer == 0x7f &&
                life.state().image == LifeCycleImage::Hidden,
            "Collision did not finish at the rebuild seed");
}

void test_fuel_image_duration() {
    LifeCycle life;
    const auto entry = life.begin_life_end(LifeEndCause::FuelExhausted);
    require(entry.start_audio == LifeEndCause::FuelExhausted &&
                life.state().image_changes_remaining == 0,
            "Fuel entry used the collision animation count");

    unsigned visible = 1;
    for (int pass = 1; pass <= 127; ++pass) {
        (void)life.advance(false);
        if (life.state().image == LifeCycleImage::ExplosionA) ++visible;
        require(life.state().image != LifeCycleImage::ExplosionB,
                "Fuel death presented the alternating collision image");
        if (pass == 7) {
            require(life.state().image == LifeCycleImage::ExplosionA,
                    "Fuel image lasted fewer than eight publications");
        } else if (pass == 8) {
            require(life.state().image == LifeCycleImage::Hidden,
                    "Fuel image did not retire at its first boundary");
        }
    }
    require(visible == 8, "Fuel death did not publish exactly eight visible images");
}

void test_rebuild_reveal_and_input_wait() {
    LifeCycle life;
    (void)life.begin_life_end(LifeEndCause::Collision);
    auto transition = advance_to_rebuild(life);
    unsigned rows = transition.forced_scroll_rows;

    for (int pass = 0; pass < 77; ++pass) {
        transition = life.advance(false);
        require(transition.forced_scroll_rows == 2 && !transition.restore_checkpoint,
                "Rebuild pass did not request two source rows");
        rows += transition.forced_scroll_rows;
    }
    require(rows == 156 && life.state().timer == 0x32,
            "Rebuild did not stop after 156 rows");

    transition = life.advance(false);
    require(transition.reveal_aircraft_and_clear_transients &&
                transition.reserve_decremented && transition.forced_scroll_rows == 0,
            "Reveal boundary omitted its transient clears or reserve decrement");
    require(life.state().phase == LifeCyclePhase::AwaitInput &&
                life.state().timer == 0x31 &&
                life.state().image == LifeCycleImage::Straight &&
                life.state().reserve_aircraft == 3,
            "Reveal did not enter the source input wait");

    const auto held = life.state();
    for (int pass = 0; pass < 200; ++pass) {
        require(life.advance(false) == LifeCycleTransition{} && life.state() == held,
                "Neutral input advanced the stable input wait");
    }
    transition = life.advance(true);
    require(transition.resume_controls && life.controls_active() &&
                life.state().timer == 0 && !life.state().cause,
            "Held input did not resume controls on the admitted pass");
}

void test_final_life_and_reserve_cap() {
    LifeCycle final_life{0};
    (void)final_life.begin_life_end(LifeEndCause::Collision);
    const auto transition = advance_to_rebuild(final_life);
    require(transition.terminal && !transition.restore_checkpoint &&
                transition.forced_scroll_rows == 0 && transition.clear_player_shot,
            "Final life incorrectly entered checkpoint rebuilding");
    require(final_life.state().phase == LifeCyclePhase::Terminal &&
                final_life.state().image == LifeCycleImage::Hidden &&
                final_life.state().timer == 0xff,
            "Final life did not enter the terminal state");
    require(final_life.advance(true) == LifeCycleTransition{},
            "Terminal state responded to controls");

    LifeCycle reserves{8};
    require(reserves.add_reserve_aircraft() &&
                reserves.state().reserve_aircraft == kMaximumReserveAircraft,
            "Extra life did not increment the reserve count");
    require(!reserves.add_reserve_aircraft() &&
                reserves.state().reserve_aircraft == kMaximumReserveAircraft,
            "Extra life exceeded the source reserve cap");
    reserves.exhaust_reserves();
    require(reserves.state().reserve_aircraft == 0 && reserves.controls_active(),
            "Reserve exhaustion changed the active lifecycle prematurely");
    reserves.reset();
    require(reserves.state().reserve_aircraft == 8 && reserves.controls_active(),
            "Reset did not restore the configured session seed");

    bool rejected = false;
    try {
        LifeCycle invalid{static_cast<std::uint8_t>(kMaximumReserveAircraft + 1U)};
        (void)invalid;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Invalid reserve count was accepted");
}

void test_two_player_initial_boundary_and_alternating_deaths() {
    LifeCycle life;
    life.start_round(LifeCyclePlayerMode::TwoPlayer);
    require(life.state().player_mode == LifeCyclePlayerMode::TwoPlayer &&
                life.state().active_player == 1 &&
                life.state().reserve_aircraft == 4 &&
                life.state().inactive_reserve_aircraft == 4,
            "Two-player startup did not seed both aircraft allocations on player 2");

    auto transition = life.advance(false);
    require(transition.player_changed && transition.restore_checkpoint &&
                life.state().active_player == 0 &&
                life.state().reserve_aircraft == 4 &&
                life.state().inactive_reserve_aircraft == 4,
            "Initial hidden boundary did not switch to player 1 before rebuild");
    finish_rebuild_and_resume(life);
    require(life.state().reserve_aircraft == 3 &&
                life.state().inactive_reserve_aircraft == 4,
            "First reveal consumed the wrong player's aircraft");

    (void)life.begin_life_end(LifeEndCause::Collision);
    transition = advance_to_rebuild(life);
    require(transition.player_changed && transition.restore_checkpoint &&
                life.state().active_player == 1 &&
                life.state().reserve_aircraft == 4 &&
                life.state().inactive_reserve_aircraft == 3,
            "First death did not activate player 2's stored reserves");
    finish_rebuild_and_resume(life);
    require(life.state().reserve_aircraft == 3 &&
                life.state().inactive_reserve_aircraft == 3,
            "Player 2 reveal consumed another player's reserve");

    (void)life.begin_life_end(LifeEndCause::FuelExhausted);
    transition = advance_to_rebuild(life);
    require(transition.player_changed && transition.restore_checkpoint &&
                life.state().active_player == 0 &&
                life.state().reserve_aircraft == 3 &&
                life.state().inactive_reserve_aircraft == 3,
            "Second death did not return to player 1");
}

void test_two_player_survivor_and_terminal_boundary() {
    LifeCycle life;
    life.start_round(LifeCyclePlayerMode::TwoPlayer);
    (void)life.advance(false);
    finish_rebuild_and_resume(life);
    life.exhaust_reserves();
    require(life.state().active_player == 0 &&
                life.state().reserve_aircraft == 0 &&
                life.state().inactive_reserve_aircraft == 4,
            "Reserve exhaustion affected the inactive player");

    (void)life.begin_life_end(LifeEndCause::Collision);
    auto transition = advance_to_rebuild(life);
    require(transition.player_changed && !transition.terminal &&
                life.state().active_player == 1 &&
                life.state().reserve_aircraft == 4 &&
                life.state().inactive_reserve_aircraft == 0,
            "Player 1 exhaustion ended a round while player 2 still had aircraft");
    finish_rebuild_and_resume(life);

    (void)life.begin_life_end(LifeEndCause::Collision);
    transition = advance_to_rebuild(life);
    require(!transition.player_changed && !transition.terminal &&
                transition.restore_checkpoint && life.state().active_player == 1 &&
                life.state().reserve_aircraft == 3 &&
                life.state().inactive_reserve_aircraft == 0,
            "Surviving player did not continue when the other player was out");
    finish_rebuild_and_resume(life);
    life.exhaust_reserves();

    (void)life.begin_life_end(LifeEndCause::Collision);
    transition = advance_to_rebuild(life);
    require(transition.player_changed && transition.terminal &&
                !transition.restore_checkpoint && life.state().active_player == 0 &&
                life.state().reserve_aircraft == 0 &&
                life.state().inactive_reserve_aircraft == 0 &&
                life.state().phase == LifeCyclePhase::Terminal,
            "Round did not normalize player 1 after both players exhausted aircraft");
}

void test_two_player_extra_reserves_belong_to_active_player() {
    LifeCycle life{8};
    life.start_round(LifeCyclePlayerMode::TwoPlayer);
    (void)life.advance(false);
    finish_rebuild_and_resume(life);
    require(life.state().reserve_aircraft == 7 &&
                life.state().inactive_reserve_aircraft == 8,
            "Two-player reveal did not keep independent reserve counts");
    require(life.add_reserve_aircraft() && life.add_reserve_aircraft() &&
                !life.add_reserve_aircraft() &&
                life.state().reserve_aircraft == kMaximumReserveAircraft &&
                life.state().inactive_reserve_aircraft == 8,
            "Extra aircraft crossed player ownership or exceeded the cap");

    (void)life.begin_life_end(LifeEndCause::Collision);
    const auto transition = advance_to_rebuild(life);
    require(transition.player_changed && life.state().active_player == 1 &&
                life.state().reserve_aircraft == 8 &&
                life.state().inactive_reserve_aircraft == kMaximumReserveAircraft,
            "Player switch duplicated or lost stored reserves");
    require(life.add_reserve_aircraft() && !life.add_reserve_aircraft() &&
                life.state().reserve_aircraft == kMaximumReserveAircraft &&
                life.state().inactive_reserve_aircraft == kMaximumReserveAircraft,
            "Extra aircraft did not apply only to the newly active player");
}

void test_idle_preserves_allocation_and_clear_session_discards_it() {
    LifeCycle life;
    life.start_round(LifeCyclePlayerMode::TwoPlayer);
    (void)life.advance(false);
    finish_rebuild_and_resume(life);
    (void)life.begin_life_end(LifeEndCause::Collision);
    const auto active = life.state().active_player;
    const auto reserve = life.state().reserve_aircraft;
    const auto inactive = life.state().inactive_reserve_aircraft;

    life.enter_idle();
    require(life.state().phase == LifeCyclePhase::Terminal &&
                life.state().image == LifeCycleImage::Hidden &&
                life.state().timer == 0xff &&
                life.state().image_changes_remaining == 0 &&
                !life.state().cause && !life.controls_active() &&
                life.state().player_mode == LifeCyclePlayerMode::TwoPlayer &&
                life.state().active_player == active &&
                life.state().reserve_aircraft == reserve &&
                life.state().inactive_reserve_aircraft == inactive &&
                life.advance(true) == LifeCycleTransition{},
            "Idle entry changed a player's allocation or retained life-end state");

    life.clear_session();
    require(life.state().phase == LifeCyclePhase::Terminal &&
                life.state().image == LifeCycleImage::Hidden &&
                life.state().timer == 0xff &&
                life.state().player_mode == LifeCyclePlayerMode::Single &&
                life.state().active_player == 0 &&
                life.state().reserve_aircraft == 0 &&
                life.state().inactive_reserve_aircraft == 0 &&
                !life.state().cause && !life.controls_active(),
            "Option clear retained a player's aircraft allocation");
    life.start_round(LifeCyclePlayerMode::TwoPlayer);
    require(life.state().phase == LifeCyclePhase::Exploding &&
                life.state().active_player == 1 &&
                life.state().reserve_aircraft == kInitialReserveAircraft &&
                life.state().inactive_reserve_aircraft == kInitialReserveAircraft,
            "New two-player round did not reseed after idle option selection");
}

} // namespace

int main() {
    try {
        test_initial_round_rebuild();
        test_collision_entry_and_images();
        test_fuel_image_duration();
        test_rebuild_reveal_and_input_wait();
        test_final_life_and_reserve_cap();
        test_two_player_initial_boundary_and_alternating_deaths();
        test_two_player_survivor_and_terminal_boundary();
        test_two_player_extra_reserves_belong_to_active_player();
        test_idle_preserves_allocation_and_clear_session_discards_it();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
