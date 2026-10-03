#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "fixtures/refuel_session_cases.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

void advance_one_tick(FlightPreview& flight) {
    for (unsigned slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        (void)flight.take_audio();
        if (count > 1) throw std::runtime_error("Refuel replay admitted multiple updates");
        if (count == 1) return;
    }
    throw std::runtime_error("Refuel replay did not admit a bounded update");
}
} // namespace

int main() {
    try {
        FlightPreview flight;
        flight.start();
        test::wait_for_launch(flight);
        (void)flight.take_audio();
        static_assert(test_data::refuel_session_cases.size() == 154);
        static_assert(test_data::refuel_contact_witnesses.size() == 19);
        std::size_t index = 0, refuels = 0;
        for (const auto& expected : test_data::refuel_session_cases) {
            const auto require = [&](bool condition, const char* field) {
                if (!condition) throw std::runtime_error(
                    "Natural refuel pass " + std::to_string(index) + ": " + field);
            };
            require(expected.index == index && expected.inferred_input == (index == 0),
                    "input sequence or seed provenance changed");
            // Only recorded controls drive gameplay; the initial seed input is inferred.
            flight.set_controls({false, false, expected.direction == 2,
                                 expected.direction == 1, expected.fire});
            const auto fuel_before = flight.fuel();
            const auto objects_before = flight.world().object_state();
            advance_one_tick(flight);
            require(flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active,
                    "left natural active flight");
            require(flight.steering() == expected.steering && flight.pose() == expected.pose,
                    "post-control steering or pose");
            require(flight.fuel() == expected.fuel && flight.score() == expected.score &&
                        flight.lives() == expected.lives,
                    "post-consumer fuel, score or reserves");
            const auto& contact = flight.last_player_contact();
            require(contact.refuel_contact == expected.refueled && !contact.contacted(),
                    "actual player-consumer outcome");
            if (expected.refueled) {
                const auto& witness = test_data::refuel_contact_witnesses.at(refuels);
                require(witness.active_case_index == index &&
                            consume_fuel({fuel_before}).state.amount == witness.fuel_before &&
                            flight.fuel() == witness.fuel_after,
                        "drain-before-refuel branch boundary");
                const auto& slot = flight.last_consumed_contacts().player.slots.at(
                    witness.contact_slot);
                require(slot.object_contact && slot.primary_present &&
                            !slot.auxiliary_projectile_present && slot.primary_object &&
                            slot.primary_object->value == witness.record_index &&
                            objects_before.records[witness.record_index].animated_kind == 15,
                        "published live-depot participant or mapping");
                ++refuels;
                require(flight.audio_state().voice1_effect ==
                            (expected.saturated ? Voice1AudioEffect::RefuelTankSaturated
                                                : Voice1AudioEffect::RefuelFuelAdded),
                        "refuel sound selection");
            }
            ++index;
        }
        if (refuels != test_data::refuel_contact_witnesses.size())
            throw std::runtime_error("Refuel comparison did not consume every positive witness");
        std::cout << index << " active passes and " << refuels
                  << " natural refuel consumers match; first input seed-inferred\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
