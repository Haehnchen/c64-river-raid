#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "fixtures/reversal_session_cases.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

void advance_one_tick(FlightPreview& flight) {
    for (unsigned slice = 0; slice < 4; ++slice) {
        const auto admitted = flight.advance_elapsed(10ms);
        if (admitted > 1) throw std::runtime_error("Replay admitted multiple foreground passes");
        (void)flight.take_audio();
        if (admitted == 1) return;
    }
    throw std::runtime_error("Replay failed to admit a foreground pass");
}

} // namespace

int main() {
    try {
        FlightPreview flight;
        flight.start();
        test::wait_for_launch(flight);
        (void)flight.take_audio();
        if (flight.last_player_contact().contacted() || flight.last_player_contact().refuel_contact)
            throw std::runtime_error("Fresh flight retained a player contact selection");
        static_assert(test_data::reversal_session_cases.size() == 360);
        static_assert(test_data::reversal_session_resume_index == 258);
        static_assert(test_data::reversal_collision.active_case_index + 1 ==
                      test_data::reversal_session_resume_index);
        std::size_t index = 0;
        unsigned rebuild_ticks = 0;
        for (const auto& original : test_data::reversal_session_cases) {
            const auto require = [&](bool condition, const char* field) {
                if (!condition) throw std::runtime_error(
                    "Natural reversal pass " + std::to_string(index) + ": " + field);
            };
            require(original.index == index, "nonsequential expected observation");
            require(original.inferred_input == (index == 0), "input provenance changed");
            require(original.direction == (index < 4 ? 2 : index < 7 ? 1 : 0) &&
                        original.fire, "recorded input sequence changed");
            if (index == test_data::reversal_session_resume_index) {
                const auto score = flight.score();
                const auto fuel = flight.fuel();
                const auto lives = flight.lives();
                require(flight.shot().pose == PlayerShotPose::Visible,
                        "collision witness lacks a visible incoming shot");
                // No next source control read occurs until after the natural respawn.
                // Held controls remain in place; no state or contact is injected.
                advance_one_tick(flight);
                require(!flight.last_player_contact().contacted() &&
                            !flight.last_player_contact().refuel_contact,
                        "queued crash retained the preceding active player selection");
                require(flight.status() == FlightStatus::Crashed &&
                            flight.life_cycle().cause == LifeEndCause::Collision &&
                            flight.life_cycle().timer == 0xfe &&
                            flight.life_cycle().image == LifeCycleImage::ExplosionA &&
                            flight.presented_image() == LifeCycleImage::Straight &&
                            flight.shot().pose == PlayerShotPose::Hidden &&
                            flight.shot().state.vertical_position == 0 &&
                            flight.fuel() == fuel && flight.score() == score &&
                            flight.lives() == lives,
                        "natural collision boundary differs");
                // Native reset work is a bounded presentation-tick approximation;
                // this does not establish source elapsed-time or raster parity.
                for (; rebuild_ticks < 512 &&
                       flight.life_cycle().phase != LifeCyclePhase::AwaitInput; ++rebuild_ticks) {
                    advance_one_tick(flight);
                }
                require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                            flight.life_cycle().image == LifeCycleImage::Straight &&
                            flight.life_cycle().timer == 0x31 &&
                            flight.presented_image() == LifeCycleImage::Hidden &&
                            flight.fuel() == kInitialFuel && flight.score() == score &&
                            flight.lives() == lives - 1 &&
                            flight.shot().pose == PlayerShotPose::Hidden &&
                            !flight.last_player_contact().contacted() &&
                            !flight.last_player_contact().refuel_contact,
                        "natural checkpoint reveal differs");
            }
            // Only recorded controls drive gameplay; expected states are never loaded.
            flight.set_controls({false, false, original.direction == 2,
                                 original.direction == 1, original.fire});
            const auto before_objects = flight.world().object_state();
            const auto before_phase = flight.animation_phase();
            advance_one_tick(flight);
            const auto& expected_contact = test_data::reversal_collision;
            if (index == expected_contact.active_case_index) {
                const auto& selected = flight.last_player_contact();
                require(selected.object_contact.has_value() && !selected.terrain_contact &&
                            !selected.refuel_contact && !selected.bridge,
                        "fatal pass did not select only an ordinary object");
                const auto& enemy = *selected.object_contact;
                require(enemy.record_index == expected_contact.record_index &&
                            enemy.slot == expected_contact.object_slot &&
                            enemy.animated_kind == expected_contact.consumed_kind,
                        "fatal consumer record, sprite slot or post-motion kind differs");
                // The native-only pre-motion witness changes with reset-work
                // phase. The authenticated source expectation is consumed kind 9.
                const auto pre_motion_kind = static_cast<std::uint8_t>(
                    expected_contact.consumed_kind ^ ((before_phase & 1U) == 0 ? 1U : 0U));
                require(before_objects.records[enemy.record_index].animated_kind ==
                            pre_motion_kind,
                        "native pre-motion animation witness differs at current phase");
                const auto& contacts = flight.last_consumed_contacts().player;
                const auto& slot = contacts.slots.at(expected_contact.contact_slot);
                require(slot.object_contact && slot.alternate_present && !slot.primary_present &&
                            !slot.auxiliary_projectile_present && !slot.bridge_sprite_present &&
                            slot.alternate_object && slot.alternate_object->value == enemy.record_index &&
                            !contacts.terrain_contacts[0] && !contacts.terrain_contacts[1] &&
                            !contacts.player_fully_offscreen,
                        "fatal consumed participant snapshot differs");
                const auto& after = flight.world().object_state();
                require(after.active_begin == before_objects.active_begin,
                        "fatal witness needs explicit row-history index rebasing");
                const auto& destroyed = after.records[enemy.record_index];
                require(destroyed.animated_kind == expected_contact.post_effect_kind &&
                            destroyed.animation_count == expected_contact.post_effect_animation_count,
                        "ordinary player collision did not destroy the selected object");
            }
            require(flight.status() == FlightStatus::Flying &&
                        flight.life_cycle().phase == LifeCyclePhase::Active,
                    "left active flight before the captured collision boundary");
            require(flight.steering() == original.steering, "post-control steering");
            require(flight.pose() == original.pose, "post-control pose");
            require(flight.fuel() == original.fuel, "post-control fuel");
            // Score and reserves come from the following lower sample, after consumers.
            require(flight.score() == original.score, "post-consumer score");
            require(flight.lives() == original.lives, "post-consumer reserves");
            ++index;
        }
        std::cout << index << " natural active passes across collision/respawn match "
                     "steering, pose, fuel, score and reserves; fatal object consumer matches; "
                  << rebuild_ticks << " native rebuild passes (phase alignment only); "
                     "first input inferred from captured seed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
