#include "app/flight_preview.hpp"
#include "app/player_contact.hpp"
#include "support/flight_start_helpers.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        require(count <= 1, "Ten-millisecond slice admitted multiple passes");
        if (count == 1) return;
    }
    throw std::runtime_error("Foreground pass was not admitted");
}

void test_audio_precedes_new_events() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);
    flight.set_controls({false, false, false, false, true});
    tick(flight);
    require(flight.shot().start_event && flight.audio_state().shot_countdown == 32,
            "New shot event was consumed by audio in its launch pass");
    require(flight.audio_state().output.voices[1].control == AudioVoiceControl::Silent,
            "Shot voice used an event raised after its audio call");
    tick(flight);
    require(flight.audio_state().shot_countdown == 31 &&
                flight.audio_state().output.voices[1].control == AudioVoiceControl::TriangleGate &&
                flight.audio_state().output.voices[1].frequency == 0x2000,
            "Pending shot event did not reach the next eligible audio call");
}

void test_first_shot_contact_is_published_on_the_next_pass() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);
    require(flight.last_consumed_contacts() == ForegroundContactSnapshot{},
            "A new round began with stale foreground contacts");

    flight.set_controls({false, false, false, false, true});
    tick(flight);
    require(flight.shot().pose == PlayerShotPose::Visible,
            "The first ordinary fire input did not present a shot");
    require(!flight.last_consumed_contacts().shot_presented,
            "The newly presented first shot was consumed in its production pass");

    flight.set_controls({});
    tick(flight);
    require(flight.last_consumed_contacts().shot_presented,
            "The first shot was not published to the following foreground pass");
}

void test_options_and_round_restart_clear_contact_generations() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);
    flight.set_controls({false, false, false, false, true});
    tick(flight);
    flight.set_controls({});
    tick(flight);
    require(flight.last_consumed_contacts().shot_presented,
            "Reset test did not first establish a naturally produced shot generation");

    flight.show_options(0);
    require(flight.status() == FlightStatus::OptionWait &&
                flight.last_consumed_contacts() == ForegroundContactSnapshot{},
            "Entering options retained the previous round's consumed generation");
    tick(flight);
    require(!flight.last_consumed_contacts().shot_presented,
            "An old projectile generation survived an ordinary options pass");

    flight.start();
    require(flight.last_consumed_contacts() == ForegroundContactSnapshot{},
            "Starting a new round retained the previous session's contact generation");
}

void test_aircraft_selection_priority() {
    WorldObjectState objects{};
    objects.records[0].animated_kind = 9;
    objects.records[1].animated_kind = 13;
    PlayerContactSnapshot contacts{};
    auto& first = contacts.slots[1];
    first.object_contact = true;
    first.primary_present = true;
    first.primary_object = ShotObjectIndex{0};
    first.alternate_present = true;
    first.alternate_object = ShotObjectIndex{1};
    auto& second = contacts.slots[0];
    second.object_contact = true;
    second.primary_present = true;
    second.primary_object = ShotObjectIndex{1};
    contacts.terrain_contacts[0] = true;

    auto selected = select_player_contact(contacts, objects);
    require(selected.object_contact && selected.object_contact->record_index == 0 &&
                selected.object_contact->slot == WorldObjectHitSlot::Primary &&
                !selected.terrain_contact && !selected.bridge && !selected.refuel_contact,
            "Aircraft selector lost sample/role priority or returned multiple effects");

    first.primary_present = false;
    selected = select_player_contact(contacts, objects);
    require(selected.object_contact && selected.object_contact->record_index == 1 &&
                selected.object_contact->slot == WorldObjectHitSlot::Alternate,
            "Alternate mapping was not retained by the aircraft selector");

    first.primary_present = true;
    objects.records[0].animated_kind = 2;
    selected = select_player_contact(contacts, objects);
    require(!selected.object_contact && selected.terrain_contact,
            "Aircraft selector used stale kind or continued after an ignored-kind diversion");

    objects.records[0].animated_kind = 15;
    selected = select_player_contact(contacts, objects);
    require(selected.refuel_contact && !selected.contacted(),
            "Refuel did not return before remaining damage contacts");

    objects.records[0].animated_kind = 14;
    first.bridge_sprite_present = true;
    selected = select_player_contact(contacts, objects);
    require(selected.bridge && selected.bridge->joint_contact &&
                !selected.object_contact && !selected.terrain_contact && !selected.refuel_contact,
            "Bridge contact was not an exclusive aircraft transaction");

    first.auxiliary_projectile_present = true;
    selected = select_player_contact(contacts, objects);
    require(selected.terrain_contact && !selected.bridge && !selected.object_contact,
            "Auxiliary participation did not divert ordinary aircraft scanning");

    first.auxiliary_projectile_present = false;
    first.primary_object = ShotObjectIndex{12};
    selected = select_player_contact(contacts, objects);
    require(selected.object_contact && selected.object_contact->record_index == 1 &&
                selected.object_contact->slot == WorldObjectHitSlot::Primary,
            "Invalid mapping did not continue to the next lower sample");

    first.primary_object = ShotObjectIndex{0};
    for (unsigned kind = 0; kind <= 255; ++kind) {
        objects.records[0].animated_kind = static_cast<std::uint8_t>(kind);
        selected = select_player_contact(contacts, objects);
        require(selected.object_contact.has_value() == (kind >= 7 && kind < 14) &&
                    selected.bridge.has_value() == (kind == 14) &&
                    selected.refuel_contact == (kind == 15) &&
                    selected.terrain_contact == (kind < 7 || kind > 15),
                "Current-kind classification differs across the byte domain");
    }
}

void test_enemy_contact_queues_life_end() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);
    flight.set_controls({false, false, false, false, true});
    tick(flight);
    flight.set_controls({false, false, false, true, false});
    tick(flight);
    require(flight.presented_pose() != flight.pose(),
            "Natural contact route did not establish a current/published pose difference");
    flight.set_controls({});
    bool previous_pass_consumed_enemy = false;
    std::size_t rows_after_contact = 0;
    for (int pass = 0; pass < 1000; ++pass) {
        const auto pre_tick_objects = flight.world().object_state();
        const auto before_rows = flight.world().generated_rows();
        tick(flight);
        if (flight.status() == FlightStatus::Crashed) {
            require(previous_pass_consumed_enemy && flight.lives() == 3 &&
                        flight.score() == 0 && flight.audio_state().explosion_countdown == 30 &&
                        flight.audio_state().voice1_effect == Voice1AudioEffect::Collision &&
                        flight.audio_state().voice1_countdown == 24 &&
                        flight.world().generated_rows() == rows_after_contact,
                    "Fatal contact did not enter the ordinary life-end pass after consumption");
            return;
        }

        const auto selected = select_player_contact(
            flight.last_consumed_contacts().player, pre_tick_objects);
        const bool enemy_contact = selected.object_contact.has_value();
        if (enemy_contact) {
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                        flight.world().generated_rows() > before_rows && flight.score() == 0 &&
                        flight.audio_state().explosion_countdown == 31 &&
                        flight.audio_state().voice1_effect != Voice1AudioEffect::Collision,
                    "Consumed aircraft contact skipped its row pass or life-end audio ordering");
            rows_after_contact = flight.world().generated_rows();
        }
        previous_pass_consumed_enemy = enemy_contact;
    }
    throw std::runtime_error("Natural input did not consume an enemy contact before life end");
}

void test_natural_contact_uses_published_position() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);

    const FlightControls right{false, false, false, true, false};
    for (int pass = 0; pass < 6; ++pass) {
        flight.set_controls(right);
        tick(flight);
        require(flight.status() == FlightStatus::Flying,
                "Warm-up steering ended before a contact boundary could be sampled");
    }

    for (int pass = 0; pass < 1200; ++pass) {
        const auto presented = flight.presented_steering();
        const auto current = flight.steering();
        const auto pre_tick_objects = flight.world().object_state();
        const auto fresh_contact = sample_player_contact(flight.world(), current, flight.pose());
        const auto before_rows = flight.world().generated_rows();
        flight.set_controls(right);
        tick(flight);
        require(flight.status() == FlightStatus::Flying,
                "Natural route ended before an asymmetric contact witness");

        const auto consumed_contact = select_player_contact(
            flight.last_consumed_contacts().player, pre_tick_objects);
        if (presented.horizontal_position != current.horizontal_position &&
            consumed_contact.contacted() != fresh_contact.contacted()) {
            const bool expected_fatal = consumed_contact.contacted();
            if (expected_fatal) {
                require(flight.world().generated_rows() > before_rows && flight.score() == 0 &&
                            flight.status() == FlightStatus::Flying,
                        "Published fatal sample skipped its producing pass");
            }
            flight.set_controls({});
            tick(flight);
            require((flight.status() == FlightStatus::Crashed) == expected_fatal,
                    "Natural outcome followed fresh controls rather than the consumed generation");
            if (expected_fatal) {
                require(flight.audio_state().explosion_countdown == 30 &&
                            flight.audio_state().voice1_effect == Voice1AudioEffect::Collision &&
                            flight.audio_state().voice1_countdown == 24,
                        "Life-end audio did not start on the following pass");
            }
            return;
        }
    }
    throw std::runtime_error("Bounded steering route found no consumed/fresh contact disagreement");
}
} // namespace

int main() {
    try {
        test_audio_precedes_new_events();
        test_first_shot_contact_is_published_on_the_next_pass();
        test_options_and_round_restart_clear_contact_generations();
        test_aircraft_selection_priority();
        test_enemy_contact_queues_life_end();
        test_natural_contact_uses_published_position();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
