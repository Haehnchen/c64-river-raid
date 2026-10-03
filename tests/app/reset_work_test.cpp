#include "app/flight_start.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

constexpr FlightControls held{false, false, false, true, true};
constexpr FlightControls bank_right{false, false, false, true, false};
constexpr std::array options{StartOption{"1"}};
constexpr std::array interrupt_options{StartOption{"1"}, StartOption{"2"}};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void tick(FlightPreview& flight) {
    for (unsigned slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        const auto audio = flight.take_audio();
        require(count <= 1, "Reset-work slice admitted multiple updates");
        if (count == 1) {
            require(!audio.empty(), "Admitted reset/foreground tick omitted synthesis samples");
            return;
        }
    }
    throw std::runtime_error("Reset-work interval admitted no bounded update");
}

void start_from_title(FlightStart& launch) {
    for (unsigned press = 0; press < 2; ++press) {
        launch.set_button(StartButton::Proceed, true);
        launch.set_button(StartButton::Proceed, false);
    }
}

void press(FlightStart& launch, StartButton button) {
    launch.set_button(button, true);
    launch.set_button(button, false);
}

void finish_reset_work(FlightPreview& flight) {
    require(flight.checkpoint_reset_work() ==
                CheckpointResetWorkState{5, 2} &&
                flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.life_cycle().timer == 0x7f &&
                flight.world().generated_rows() == 0,
            "Reset entry did not defer its first two rows at the source timer boundary");
    const auto life = flight.life_cycle();
    const auto objects = flight.world().object_state();
    const auto river = flight.world().river_state();
    const auto steering = flight.steering();
    const auto shot = flight.shot();
    const auto score = flight.score();
    const auto fuel = flight.fuel();
    const auto hits = flight.ordinary_hits();
    const auto contacts = flight.last_consumed_contacts();
    const auto audio = flight.audio_state();
    const auto phase = flight.animation_phase();
    flight.set_controls(held);

    for (unsigned busy = 0; busy < 5; ++busy) {
        tick(flight);
        require(flight.animation_phase() == static_cast<std::uint8_t>(phase + busy + 1) &&
                    flight.life_cycle() == life && flight.steering() == steering &&
                    flight.shot().state == shot.state && flight.shot().pose == shot.pose &&
                    flight.shot().vertical_position_write == shot.vertical_position_write &&
                    flight.shot().start_event == shot.start_event &&
                    flight.score() == score && flight.fuel() == fuel &&
                    flight.ordinary_hits() == hits && flight.last_consumed_contacts() == contacts &&
                    !flight.last_player_contact().terrain_contact &&
                    !flight.last_player_contact().object_contact &&
                    !flight.last_player_contact().refuel_contact &&
                    !flight.last_player_contact().bridge && flight.audio_state() == audio,
                "Presentation-only reset tick advanced foreground gameplay/controller state");
        require(flight.presented_image() == LifeCycleImage::Hidden &&
                    flight.presented_steering() == steering,
                "Reset-work tick did not publish the hidden checkpoint aircraft");
        if (busy != 4) {
            require(flight.checkpoint_reset_work() ==
                        CheckpointResetWorkState{static_cast<std::uint8_t>(4 - busy), 2} &&
                        flight.world().generated_rows() == 0 &&
                        flight.world().object_state() == objects &&
                        flight.world().river_state() == river,
                    "Reset work changed records/rows before its completion");
        } else {
            require(!flight.checkpoint_reset_work() && flight.world().generated_rows() == 2,
                    "Reset completion did not emit its two deferred rows exactly once");
        }
    }
    tick(flight);
    require(!flight.checkpoint_reset_work() && flight.life_cycle().timer == 0x7e &&
                flight.world().generated_rows() == 4,
            "Ordinary foreground work did not resume after the completed reset interval");
}

unsigned wait_for_input_gate(FlightPreview& flight) {
    unsigned ticks = 0;
    while (ticks < 128 && flight.life_cycle().phase != LifeCyclePhase::AwaitInput) {
        tick(flight);
        ++ticks;
    }
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                !flight.checkpoint_reset_work() && flight.world().generated_rows() == 156 &&
                flight.fuel() == kInitialFuel && flight.shot().state.vertical_position == 0,
            "Normal rebuild did not reach its unconsumed ordinary-input gate");
    return ticks;
}

void test_natural_cold_start_and_held_input() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    launch.set_controls(held);
    start_from_title(launch);
    require(flow.stage() == StartStage::StartRequested &&
                flight.life_cycle().timer == 0x80 && !flight.checkpoint_reset_work(),
            "Ordinary F1/F1 did not seed the hidden round");
    tick(flight);
    require(flight.animation_phase() == 1, "Cold foreground entry advanced the wrong phase");
    finish_reset_work(flight); // Five busy ticks, then one ordinary rebuild tick.
    const auto remaining = wait_for_input_gate(flight);
    require(1 + 5 + 1 + remaining == 84 && flight.animation_phase() == 84 &&
                flight.lives() == 3 && flight.presented_image() == LifeCycleImage::Hidden,
            "Native PAL reset-work profile changed its cold reveal/allocation boundary");
    tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.fuel() == kInitialFuel - kFuelDrainPerTick &&
                flight.pose() == FlightPose::Right && flight.shot().state.vertical_position == 192,
            "Held ordinary input did not resume only after reveal");
}

void crash_naturally(FlightPreview& flight) {
    flight.set_controls(bank_right);
    for (unsigned update = 0; update < 2000; ++update) {
        tick(flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.life_cycle().cause == LifeEndCause::Collision &&
                        flight.life_cycle().timer == 0xfe && !flight.checkpoint_reset_work(),
                    "Ordinary bank contact did not enter its natural collision lifecycle");
            return;
        }
        require(flight.status() == FlightStatus::Flying,
                "Bank-contact route left ordinary active flight");
    }
    throw std::runtime_error("Held ordinary bank input did not cause a bounded natural collision");
}

void reach_pending_respawn(FlightPreview& flight, FlightStart& launch) {
    start_from_title(launch);
    (void)wait_for_input_gate(flight);
    crash_naturally(flight);
    flight.set_controls(held);
    for (unsigned exploding = 0; exploding < 127; ++exploding) tick(flight);
    require(flight.status() == FlightStatus::Crashed &&
                flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.world().generated_rows() == 0,
            "Natural collision did not expose the interruptible deferred-row interval");
}

void test_f1_restarts_and_discards_pending_rows() {
    StartFlow flow(interrupt_options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    reach_pending_respawn(flight, launch);

    // The accepted F1 restart replaces this pending checkpoint reset with a
    // fresh round; only the fresh round's own reset may publish its two rows.
    press(launch, StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested && flight.status() == FlightStatus::Preparing &&
                !flight.checkpoint_reset_work() && flight.world().generated_rows() == 0 &&
                flight.lives() == kInitialReserveAircraft && !flight.two_players(),
            "F1 restart retained the interrupted life reset or its deferred rows");
    tick(flight);
    require(flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.world().generated_rows() == 0,
            "Fresh F1 session did not start an independent reset-work interval");
    for (unsigned work = 0; work < 4; ++work) {
        tick(flight);
        require(flight.checkpoint_reset_work() ==
                    CheckpointResetWorkState{static_cast<std::uint8_t>(4 - work), 2} &&
                    flight.world().generated_rows() == 0,
                "Interrupted F1 reset leaked rows before the fresh interval completed");
    }
    tick(flight);
    require(!flight.checkpoint_reset_work() && flight.world().generated_rows() == 2,
            "Fresh F1 reset did not publish exactly its own deferred rows");
}

void test_f3_returns_to_options_and_discards_pending_rows() {
    StartFlow flow(interrupt_options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    reach_pending_respawn(flight, launch);

    press(launch, StartButton::NextOption);
    require(flow.stage() == StartStage::Options && flow.selected_option_index() == 1 &&
                flight.status() == FlightStatus::OptionWait && flight.selected_option() == 1 &&
                flight.two_players() && !flight.checkpoint_reset_work() &&
                flight.world().generated_rows() == 0,
            "F3 menu return did not cancel pending reset work without publishing deferred rows");
    for (unsigned menu_tick = 0; menu_tick < 8; ++menu_tick) {
        tick(flight);
        require(flow.stage() == StartStage::Options && flight.status() == FlightStatus::OptionWait &&
                    !flight.checkpoint_reset_work() && flight.world().generated_rows() == 0,
                "Canceled checkpoint rows leaked into the option-wait session");
    }
}

void test_natural_respawns_and_terminal_bypass() {
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    start_from_title(launch);
    (void)wait_for_input_gate(flight);
    for (int reserves = 3; reserves >= 0; --reserves) {
        crash_naturally(flight);
        require(flight.lives() == reserves, "Natural collision changed reserves before reveal");
        flight.set_controls(held);
        const auto terminal_rows = flight.world().generated_rows();
        for (unsigned exploding = 0; exploding < 127; ++exploding) {
            tick(flight);
            if (exploding != 126)
                require(!flight.checkpoint_reset_work(), "Reset interval began before explosion completion");
        }
        if (reserves == 0) {
            require(flight.status() == FlightStatus::GameOver &&
                        flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                        !flight.checkpoint_reset_work() &&
                        flight.world().generated_rows() == terminal_rows,
                    "Zero-reserve terminal path incorrectly entered reset/forced rows");
            flight.set_controls({});
            tick(flight);
            require(!flight.checkpoint_reset_work() &&
                        flight.world().generated_rows() == terminal_rows,
                    "Terminal publication started deferred checkpoint work");
            break;
        }
        require(flight.status() == FlightStatus::Crashed, "Respawn reset lost its collision status");
        finish_reset_work(flight);
        (void)wait_for_input_gate(flight);
        require(flight.lives() == reserves - 1, "Respawn reveal omitted reserve allocation");
    }
}
} // namespace

int main() {
    try {
        test_natural_cold_start_and_held_input();
        test_natural_respawns_and_terminal_bypass();
        test_f1_restarts_and_discards_pending_rows();
        test_f3_returns_to_options_and_discards_pending_rows();
        std::cout << "Normal F1 reset, three natural collision respawns, F1/F3 interruption of pending "
                     "reset work, held-input blocking and terminal bypass; five-tick PAL approximation, "
                     "not reset/IRQ/audio parity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
