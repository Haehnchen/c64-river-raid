#include "app/flight_preview.hpp"
#include "app/flight_start.hpp"
#include "app/world_setup.hpp"
#include "support/flight_start_helpers.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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

void tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        require(count <= 1, "One slice admitted multiple ticks");
        if (count == 1) return;
    }
    throw std::runtime_error("Idle foreground stopped updating");
}

void wait_for_attract(FlightPreview& flight, unsigned limit) {
    const auto rows = flight.world().generated_rows();
    for (unsigned pass = 0; pass < limit; ++pass) {
        tick(flight);
        if (flight.status() == FlightStatus::Attract) return;
        require(flight.world().generated_rows() == rows,
                "Waiting session scrolled before attract entry");
    }
    throw std::runtime_error("Session countdown did not enter attract");
}

void test_option_wait_retains_world_then_rebuilds_selected_demo() {
    FlightPreview flight;
    flight.start();
    test::wait_for_launch(flight);
    const auto old_world = flight.world();
    flight.show_options(6);
    require(flight.status() == FlightStatus::OptionWait && flight.idle_state().countdown == 37 &&
                flight.selected_option() == 6 && flight.bridge_number() == 50 &&
                flight.world().river_state().section == 50 && flight.lives() == 0 &&
                flight.presented_image() == LifeCycleImage::Hidden &&
                flight.life_cycle().image == LifeCycleImage::Hidden,
            "Option selection did not install the selected idle state");
    require(flight.world().object_state() == old_world.object_state() &&
                std::ranges::equal(flight.world().viewport().rows(), old_world.viewport().rows()) &&
                flight.world().river_state().random == old_world.river_state().random &&
                flight.world().river_state().course == old_world.river_state().course,
            "Option selection reset live world state before its timeout");
    wait_for_attract(flight, 300);
    require(flight.world().generated_rows() == 2 && flight.world().river_state().section == 50 &&
                flight.bridge_number() == 50 && flight.life_cycle().image == LifeCycleImage::Hidden &&
                flight.presented_image() == LifeCycleImage::Hidden &&
                flight.idle_state().phase == SessionIdlePhase::Attract,
            "Attract did not start the selected course with two hidden-aircraft rows");
    auto expected_world = make_river_world(river_start_preset(3));
    (void)expected_world.restore_checkpoint();
    expected_world.begin_life();
    (void)expected_world.advance();
    (void)expected_world.advance();
    require(flight.world().object_state() == expected_world.object_state(),
            "Attract updated the rebuilt object history before its first two rows");
    require(flight.audio_state().transition_countdown == 0,
            "Automatic attract incorrectly triggered a function-key transition sound");
    for (int pass = 0; pass < 30; ++pass) tick(flight);
    require(flight.world().generated_rows() == 46 && flight.steering().scroll_speed == 0x80,
            "Attract startup did not settle to its source scroll speed");
    auto previous_rows = flight.world().generated_rows();
    for (int pass = 0; pass < 2100; ++pass) {
        tick(flight);
        require(flight.status() == FlightStatus::Attract &&
                    flight.world().generated_rows() > previous_rows,
                "Attract countdown re-entered or stopped the running demo");
        previous_rows = flight.world().generated_rows();
    }
}

void test_attract_crossing_moves_and_publishes_gap() {
    FlightPreview flight;
    flight.show_options(6, true);
    wait_for_attract(flight, 3);
    bool moved = false;
    bool armed = false;
    bool gap = false;
    bool retired = false;
    bool retained_crossing = false;
    bool retained_gap = false;
    std::uint8_t previous_counter = 0xff;
    for (int pass = 0; pass < 10000 && !retired; ++pass) {
        const auto before = flight.world().bridge_sprite_state().crossing;
        const auto rows = flight.world().generated_rows();
        tick(flight);
        const auto after = flight.world().bridge_sprite_state();
        const auto published = flight.world().bridge_presentation_state();
        const auto row_count = flight.world().generated_rows() - rows;
        require(flight.status() == FlightStatus::Attract &&
                    flight.life_cycle().image == LifeCycleImage::Hidden &&
                    flight.audio_state().output.voices[2].control == AudioVoiceControl::Silent,
                "Bridge attract route left the hidden-aircraft session");
        if (before && after.crossing && published.crossing &&
            after.crossing->vic_y == before->vic_y + row_count) {
            require(published.crossing->vic_y == before->vic_y &&
                        published.crossing->vic_x == after.crossing->vic_x &&
                        published.crossing->image == after.crossing->image,
                    "Attract crossing publication included later row movement");
            retained_crossing |= row_count != 0;
        }
        if (before && after.crossing &&
            after.crossing->vic_y == before->vic_y + flight.world().generated_rows() - rows &&
            (after.crossing->vic_x == before->vic_x + 2 ||
             before->vic_x == after.crossing->vic_x + 2)) moved = true;
        if (previous_counter == 0xff && after.gap_animation_counter <= 1) {
            require(after.crossing && after.crossing->vic_y >= 59 &&
                        (after.gap_animation_counter != 0 || !after.gap),
                    "Demo gap entry did not wait for the crossing's visible row");
            armed = true;
        }
        if (armed && !retired) {
            const auto counter = after.gap_animation_counter;
            if (counter >= 1 && counter <= 17) {
                require(after.gap && after.gap->image == BridgeSpriteImage::GapFlight,
                        "Natural demo gap flight lost its image or counter cadence");
                gap = true;
            } else if (counter >= 18 && counter <= 56) {
                const auto expected = static_cast<BridgeSpriteImage>(
                    static_cast<unsigned>(BridgeSpriteImage::GapExplosion0) +
                    (counter - 18) / 8);
                require(after.gap && after.gap->image == expected,
                        "Natural demo gap explosion changed frame at the wrong counter");
            } else if (counter == 0xff && previous_counter == 56) {
                require(!after.gap && !published.gap,
                        "Inactive demo gap did not publish its final blank image");
                retired = true;
            } else {
                require(counter == 0 && !after.gap,
                        "Natural demo gap skipped or restarted its counter sequence");
            }
            if (after.gap) {
                require(published.gap &&
                            published.gap->vic_y + row_count == after.gap->vic_y &&
                            published.gap->vic_x == after.gap->vic_x &&
                            published.gap->image == after.gap->image,
                        "Attract gap publication included later row movement");
                retained_gap |= row_count != 0;
            }
        }
        if (armed && previous_counter >= 1 && previous_counter < 56) {
            require(after.gap_animation_counter == previous_counter + 1,
                    "Natural demo gap counter skipped an update");
        }
        previous_counter = after.gap_animation_counter;
    }
    require(moved && armed && gap && retired,
            "Natural attract missed crossing motion or a complete regular gap cycle");
    require(retained_crossing && retained_gap,
            "Natural attract did not distinguish published sprites from row state");
}

void test_menu_and_idle_share_the_flight_world() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
        StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"}, StartOption{"8"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    const auto press = [&](StartButton button) {
        launch.set_button(button, true);
        launch.set_button(button, false);
    };
    press(StartButton::Proceed);
    require(flow.stage() == StartStage::Options && flight.idle_state().countdown == 1,
            "Title release did not enter the short initial demo wait");
    wait_for_attract(flight, 3);
    const auto rows = flight.world().generated_rows();
    press(StartButton::NextOption);
    require(flow.selected_option_index() == 1 && flight.selected_option() == 1 &&
                flight.status() == FlightStatus::OptionWait &&
                flight.world().generated_rows() == rows,
            "F3 did not preserve the current world while changing the option");
    wait_for_attract(flight, 300);
    press(StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested && flight.selected_option() == 1 &&
                flight.status() == FlightStatus::Preparing &&
                flight.idle_state().phase == SessionIdlePhase::Inactive,
            "F1 did not leave attract through the normal selected-player startup");
    test::wait_for_launch(flight);
    require(flight.world().generated_rows() == 156 && flight.lives() == 3,
            "Attract bypassed the playable aircraft rebuild");
    press(StartButton::NextOption);
    require(flow.stage() == StartStage::Options && flight.selected_option() == 2 &&
                flight.status() == FlightStatus::OptionWait && flight.bridge_number() == 5,
            "F3 from launch wait did not return to selected-course presentation");
}

void test_terminal_countdown_keeps_records_and_enters_attract() {
    FlightPreview flight;
    flight.start();
    flight.set_controls({false, false, false, false, true});
    for (int pass = 0; pass < 10000 && flight.running(); ++pass) tick(flight);
    require(flight.status() == FlightStatus::GameOver && flight.high_score() != 0 &&
                flight.idle_state().countdown == 255,
            "Natural round did not enter the terminal attract wait with its record");
    const auto record = flight.high_score();
    const auto last_score = flight.displayed_player_score(0);
    wait_for_attract(flight, 2050);
    require(flight.high_score() == record && flight.life_cycle().image == LifeCycleImage::Hidden,
            "Attract cleared the retained record or revealed the player");
    require(flight.player_score(0) == 0 && flight.displayed_player_score(0) == last_score,
            "Attract did not retain HUD digits independently of reset logical scores");
    flight.start();
    require(flight.high_score() == record && flight.score() == 0 && flight.lives() == 4,
            "Restart after attract lost its record or playable allocation");
}

void test_attract_entry_does_not_reset_the_elapsed_budget() {
    FlightPreview flight;
    flight.show_options(0, true);
    const auto admitted = flight.advance_elapsed(1s);
    require(admitted > 2 && admitted <= 64 && flight.status() == FlightStatus::Attract,
            "Batched idle update did not enter attract");
    require(flight.advance_elapsed(0ns) == 0, "Attract entry corrupted pending elapsed ticks");
}

void test_generated_auxiliary_projectile_uses_attract_gate_and_audio() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
        StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"},
        StartOption{"8"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    const auto press = [&](StartButton button) {
        launch.set_button(button, true);
        launch.set_button(button, false);
    };

    press(StartButton::Proceed);
    wait_for_attract(flight, 3);
    for (int option = 0; option < 6; ++option) press(StartButton::NextOption);
    require(flow.selected_option_index() == 6 && flight.status() == FlightStatus::OptionWait,
            "Repeated F3 input did not select the late-course attract option");
    wait_for_attract(flight, 300);
    require(flight.status() == FlightStatus::Attract &&
                flight.world().river_state().section >= 13 &&
                flight.life_cycle().image == LifeCycleImage::Hidden,
            "Selected option did not enter the hidden-aircraft attract path");

    bool launched = false;
    for (unsigned pass = 0; pass < 30'000; ++pass) {
        tick(flight);
        require(flight.status() == FlightStatus::Attract &&
                    flight.presented_image() == LifeCycleImage::Hidden &&
                    flight.life_cycle().image == LifeCycleImage::Hidden,
                "Attract projectile test left its normal hidden-aircraft session");
        if (flight.audio_state().auxiliary_sprite_countdown != 0) {
            launched = true;
            break;
        }
        (void)flight.take_audio();
    }
    require(launched && flight.audio_state().auxiliary_sprite_countdown == 8,
            "Generated attract projectile did not queue its launch sound");
    const auto projectile = flight.world().auxiliary_projectile_state();
    require(projectile.presentation && projectile.presentation->vic_x != 0,
            "Attract launch did not publish the generated projectile");

    (void)flight.take_audio();
    tick(flight);
    require(flight.audio_state().auxiliary_sprite_countdown == 7 &&
                flight.audio_state().output.voices[2].control == AudioVoiceControl::Silent &&
                flight.audio_state().output.voices[2].sustain == 8,
            "Attract launch did not consume the silent demo voice-three countdown");
    require(flight.world().auxiliary_projectile_state().horizontal_coordinate !=
                projectile.horizontal_coordinate &&
                flight.life_cycle().image == LifeCycleImage::Hidden,
            "Attract projectile stopped moving or revealed the inactive aircraft");
    const auto pcm = flight.take_audio();
    require(!pcm.empty(), "Attract stopped the shared audio sample clock");
}
} // namespace

int main() {
    try {
        test_option_wait_retains_world_then_rebuilds_selected_demo();
        test_attract_crossing_moves_and_publishes_gap();
        test_menu_and_idle_share_the_flight_world();
        test_terminal_countdown_keeps_records_and_enters_attract();
        test_attract_entry_does_not_reset_the_elapsed_budget();
        test_generated_auxiliary_projectile_uses_attract_gate_and_audio();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
