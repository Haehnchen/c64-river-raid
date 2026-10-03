#include "app/flight_preview.hpp"
#include "app/player_contact.hpp"
#include "support/flight_start_helpers.hpp"
#include "game/life_cycle.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void advance_tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto ticks = flight.advance_elapsed(10ms);
        require(ticks <= 1, "Fuel test advanced multiple ticks");
        if (ticks == 1) return;
    }
    throw std::runtime_error("Flight tick did not advance");
}

void test_depot_pass() {
    FlightPreview flight;
    require(flight.fuel() == kInitialFuel, "Ready tank was not full");
    require(flight.advance_elapsed(1s) == 0 && flight.fuel() == kInitialFuel,
            "Ready tank drained");
    flight.start();
    flight.set_controls({});
    test::wait_for_launch(flight);
    bool contact = false, drained = false, refilled = false;
    bool refuel_audio_pending = false;
    bool pending_saturated = false;
    bool heard_refuel_audio = false;
    for (int tick = 0; tick < 160 && flight.status() == FlightStatus::Flying; ++tick) {
        flight.set_controls({false, true, tick >= 40 && flight.steering().horizontal_position > 78,
                             false, false});
        const auto before = flight.fuel();
        const auto before_effect = flight.audio_state().voice1_effect;
        const auto objects_before = flight.world().object_state();
        advance_tick(flight);
        // Depot kinds are stable across motion. Interpret the consumed masks
        // against pre-row indices, not a fresh scan of the already scrolled scene.
        const auto depot_contact = select_player_contact(
            flight.last_consumed_contacts().player, objects_before).refuel_contact;
        if (refuel_audio_pending) {
            const auto& voice = flight.audio_state().output.voices[0];
            require(voice.control == AudioVoiceControl::TriangleGate &&
                        voice.frequency == (pending_saturated ? 0x3f00 : 0x2000),
                    "Depot audio did not reach the following audio pass");
            refuel_audio_pending = false;
            heard_refuel_audio = true;
        }
        drained |= flight.fuel() < before;
        refilled |= flight.fuel() > before;
        contact |= depot_contact;
        const auto expected = std::min<unsigned>(kFullFuel,
            before - kFuelDrainPerTick + (depot_contact ? kRefuelPerContact : 0));
        require(flight.fuel() == expected, "Flight fuel differs from one drain/refill per tick");
        if (depot_contact && flight.status() == FlightStatus::Flying) {
            const bool saturated = before - kFuelDrainPerTick > kFullFuel - kRefuelPerContact;
            const auto expected_effect =
                        (saturated ? Voice1AudioEffect::RefuelTankSaturated
                                   : Voice1AudioEffect::RefuelFuelAdded);
            require(flight.audio_state().voice1_effect == expected_effect,
                    "Depot event was not retained after the foreground pass");
            if (before_effect != expected_effect) {
                require(flight.audio_state().voice1_countdown == 8,
                        "New depot event did not retain its source countdown");
                refuel_audio_pending = true;
                pending_saturated = saturated;
            }
        }
    }
    require(contact, "Scripted normal flight missed the depot");
    require(heard_refuel_audio, "Scripted depot pass did not verify delayed refuel audio");
    require(drained && refilled, "Normal depot pass did not consume and replenish fuel");

    flight.set_controls({false, false, false, true, false});
    for (int tick = 0; tick < 1000 && flight.status() == FlightStatus::Flying; ++tick) {
        advance_tick(flight);
    }
    require(flight.status() == FlightStatus::Crashed &&
                flight.life_cycle().phase == LifeCyclePhase::Exploding,
            "Bank contact did not start the life-end explosion");
    flight.set_controls({});
    const auto crashed_fuel = flight.fuel();
    for (int tick = 0; tick < 127; ++tick) {
        advance_tick(flight);
        if (tick != 126) {
            require(flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                        flight.fuel() == crashed_fuel,
                    "Explosion changed fuel before partial restore");
        }
    }
    require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                flight.status() == FlightStatus::Crashed && flight.fuel() == kInitialFuel,
            "Partial restore did not refill the tank at the 127-tick boundary");
    require(flight.checkpoint_reset_work() == CheckpointResetWorkState{5, 2} &&
                flight.world().generated_rows() == 0,
            "Checkpoint reset did not defer fuel-safe rebuild rows");
    for (int publication = 0; publication < 5; ++publication) {
        advance_tick(flight);
        if (publication < 4) {
            require(flight.checkpoint_reset_work() ==
                        CheckpointResetWorkState{static_cast<std::uint8_t>(4 - publication), 2} &&
                        flight.world().generated_rows() == 0 &&
                        flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                        flight.fuel() == kInitialFuel,
                    "Reset presentation changed fuel or emitted rows before completion");
        }
    }
    require(!flight.checkpoint_reset_work() && flight.world().generated_rows() == 2 &&
                flight.fuel() == kInitialFuel,
            "Reset completion did not publish deferred rows with a full tank");
    for (int publication = 0; publication < 77; ++publication) {
        advance_tick(flight);
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.fuel() == kInitialFuel,
                "Rebuild changed fuel before aircraft reveal");
    }
    advance_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.status() == FlightStatus::Flying && flight.invulnerable() &&
                flight.fuel() == kInitialFuel,
            "Aircraft reveal did not preserve the full tank while awaiting input");
    for (int tick = 0; tick < 8; ++tick) {
        advance_tick(flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.fuel() == kInitialFuel,
                "AwaitInput consumed fuel without controls");
    }
    flight.set_controls({false, false, false, true, false});
    advance_tick(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::Active &&
                !flight.invulnerable() && flight.fuel() == kInitialFuel - kFuelDrainPerTick,
            "Resumed flight did not consume fuel on its first active pass");
    flight.start();
    require(flight.status() == FlightStatus::Preparing && flight.fuel() == kInitialFuel &&
                flight.lives() == kInitialReserveAircraft && flight.world().generated_rows() == 0,
            "New game did not reset its preparing lifecycle");
}
} // namespace

int main() {
    try {
        test_depot_pass();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
