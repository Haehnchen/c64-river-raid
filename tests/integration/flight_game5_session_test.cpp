#include "app/flight_start.hpp"
#include "support/flight_session_steps.hpp"
#include "game/fuel.hpp"
#include "support/game5_session_inputs.hpp"
#include "support/regular_gap_observer.hpp"
#include "support/session_scene_capture.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace river_raid;
using river_raid::test::press;
using river_raid::test::tick;
using namespace std::chrono_literals;

test::SessionSceneCapture scene_capture;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] RiverCheckpoint checkpoint(const FlightPreview& flight) {
    return {flight.world().river_state().section, flight.world().river_state().section_anchor};
}

[[nodiscard]] bool first_life(const FlightPreview& flight) {
    return flight.selected_option() == 4 && !flight.two_players() &&
           flight.status() == FlightStatus::Flying &&
           flight.life_cycle().phase == LifeCyclePhase::Active && flight.lives() == 3;
}

[[nodiscard]] test::RegularGapSnapshot observation(const FlightPreview& flight) {
    return {flight.world().bridge_sprite_state(), flight.world().bridge_presentation_state(),
            flight.bridge_number(), flight.life_cycle().phase, flight.lives(),
            flight.status() == FlightStatus::Flying};
}

[[nodiscard]] bool normal_crossing(const BridgeSpriteState& state) {
    if (!state.crossing) return false;
    const auto image = state.crossing->image;
    return image == BridgeSpriteImage::CrossingRightA ||
           image == BridgeSpriteImage::CrossingRightB ||
           image == BridgeSpriteImage::CrossingLeftA ||
           image == BridgeSpriteImage::CrossingLeftB;
}

[[nodiscard]] bool sprite_at(const std::optional<BridgeSpritePresentation>& sprite,
                             BridgeSpriteImage image, int x, int y) {
    return sprite && sprite->image == image && sprite->vic_x == x && sprite->vic_y == y;
}

void complete_regular_gap(FlightStart& launch, FlightPreview& flight) {
    const auto initial = observation(flight);
    require(initial.mechanics.gap_animation_counter == 2 &&
                initial.mechanics.crossing && initial.mechanics.crossing->vic_y == 63 &&
                initial.published.crossing && initial.published.crossing->vic_y == 62,
            "Game 5 completion did not connect to the confirmed entry endpoint");

    constexpr std::array explosion_images{BridgeSpriteImage::GapExplosion0,
        BridgeSpriteImage::GapExplosion1, BridgeSpriteImage::GapExplosion2,
        BridgeSpriteImage::GapExplosion3, BridgeSpriteImage::GapExplosion4};
    constexpr std::array<std::size_t, 6> full_cycle_frames{17, 8, 8, 8, 8, 7};
    constexpr std::array<std::size_t, 6> final_partial_cycle{17, 8, 6, 0, 0, 0};
    constexpr std::array<std::string_view, 5> explosion_capture_labels{
        "game5_gap_explosion_0", "game5_gap_explosion_1", "game5_gap_explosion_2",
        "game5_gap_explosion_3", "game5_gap_explosion_4"};
    std::array<std::array<std::size_t, 6>, 3> frame_updates{};
    std::array<bool, 5> captured_explosions{};
    frame_updates[0][0] = 2; // Publication and its separate surviving update.
    scene_capture.record("game5_gap_flight", flight);
    std::size_t slices = 0, updates = 0, resets = 0, republications = 0;
    std::size_t retired_update = 0;
    for (const auto run : test::game5_regular_gap_completion) {
        require(run.direction == 0 && !run.fire && run.slices > 0,
                "Game 5 completion is not the recorded ordinary neutral continuation");
        launch.set_controls({false, false, false, false, false});
        for (int slice = 0; slice < run.slices; ++slice) {
            for (int half = 0; half < 2; ++half) {
                const auto before = observation(flight);
                const auto admitted = launch.advance_elapsed(10ms);
                (void)flight.take_audio();
                require(admitted <= 1, "Game 5 completion observation admitted multiple updates");
                if (admitted == 0) continue;
                ++updates;
                const auto after = observation(flight);
                require(first_life(flight) && !flight.life_cycle().cause &&
                            flight.bridge_number() == 21 && flight.score() >= 2650 &&
                            flight.steering().horizontal_position == 89 &&
                            flight.world().generated_rows() == 1822 + updates &&
                            flight.fuel() == 32703 - updates * kFuelDrainPerTick,
                        "Game 5 completion lost its connected active first life");
                require(!flight.last_player_contact().contacted() &&
                            !flight.last_player_contact().refuel_contact,
                        "Game 5 completion acquired an unexpected aircraft contact");
                for (const auto& slot : flight.last_consumed_contacts().gap.slots)
                    require(!slot.gap_present || !slot.player_present,
                            "Game 5 completion acquired a sampled gap/player contact");

                if (updates <= 143) {
                    require(after.mechanics.automatic_entry_enabled &&
                                after.published.automatic_entry_enabled &&
                                normal_crossing(after.mechanics) && normal_crossing(after.published) &&
                                after.mechanics.crossing->vic_y == 63 + updates &&
                                after.published.crossing->vic_y == 62 + updates,
                            "Game 5 regular crossing retired before its Y206 boundary");
                    const auto counter = static_cast<std::uint8_t>((2 + updates) % 57);
                    require(after.mechanics.gap_animation_counter == counter &&
                                after.published.gap_animation_counter == counter,
                            "Game 5 active gap counter progression changed");
                    if (counter == 0) {
                        ++resets;
                        require((updates == 55 || updates == 112) &&
                                    before.mechanics.gap_animation_counter == 56 &&
                                    before.mechanics.gap && before.published.gap &&
                                    !after.mechanics.gap && !after.published.gap,
                                "Game 5 active gap failed to hide and reset after its final frame");
                    } else {
                        const auto frame = counter <= 17 ? std::size_t{0}
                            : std::size_t{1} + (counter - 18) / 8;
                        const auto image = frame == 0 ? BridgeSpriteImage::GapFlight
                            : explosion_images[frame - 1];
                        require(after.mechanics.gap && after.published.gap &&
                                    after.mechanics.gap->image == image &&
                                    after.published.gap->image == image,
                                "Game 5 gap flight/explosion publication changed");
                        if (scene_capture.enabled() && frame > 0 &&
                            !captured_explosions[frame - 1]) {
                            scene_capture.record(explosion_capture_labels[frame - 1], flight);
                            captured_explosions[frame - 1] = true;
                        }
                        ++frame_updates[resets][frame];
                        if (counter == 1) {
                            ++republications;
                            require((updates == 56 || updates == 113) &&
                                        before.mechanics.gap_animation_counter == 0 &&
                                        !before.mechanics.gap && !before.published.gap,
                                    "Game 5 active gap did not republish on the following update");
                        }
                    }
                } else {
                    require(!after.mechanics.crossing && !after.published.crossing &&
                                !after.mechanics.gap && !after.published.gap &&
                                !after.mechanics.automatic_entry_enabled &&
                                !after.published.automatic_entry_enabled &&
                                after.mechanics.gap_animation_counter == 0xff &&
                                after.published.gap_animation_counter == 0xff,
                            "Game 5 retired crossing retained active mechanics or publication");
                    if (updates == 144) {
                        retired_update = updates;
                        require(before.mechanics.crossing &&
                                    before.mechanics.crossing->vic_y == 206 &&
                                    before.mechanics.gap_animation_counter == 31,
                                "Game 5 crossing retirement skipped its actual Y206 boundary");
                        scene_capture.record("game5_gap_retirement", flight);
                    } else {
                        require(updates == 145 && retired_update + 1 == updates &&
                                    !before.mechanics.crossing && !before.mechanics.gap,
                                "Game 5 retirement lacked a separate following active update");
                        scene_capture.record("game5_gap_retired", flight);
                    }
                }
            }
            ++slices;
        }
    }
    require(slices == 145 && updates == 145 && resets == 2 && republications == 2 &&
                frame_updates[0] == full_cycle_frames && frame_updates[1] == full_cycle_frames &&
                frame_updates[2] == final_partial_cycle && retired_update == 144 &&
                first_life(flight) && flight.world().generated_rows() == 1967 &&
                flight.bridge_number() == 21 && flight.fuel() == 28063 &&
                flight.score() >= 2650,
            "Full connected Game 5 gap completion or following lifecycle endpoint changed");
    std::cout << "game5_gap_completion route_slices=" << test::game5_regular_gap_route_slices
              << " route_updates=1674 completion_updates=" << updates
              << " resets=" << resets << " republications=" << republications
              << " crossing_retired_at=" << retired_update
              << " rows=" << flight.world().generated_rows()
              << " fuel=" << flight.fuel() << " score=" << flight.score()
              << " bridge=" << flight.bridge_number() << " lives=" << flight.lives()
              << " first_life_survived=true native_coverage_only=true\n";
}

void run_session() {
    constexpr std::array options{StartOption{"1"}, StartOption{"2"}, StartOption{"3"},
        StartOption{"4"}, StartOption{"5"}, StartOption{"6"}, StartOption{"7"},
        StartOption{"8"}};
    StartFlow flow(options);
    FlightPreview flight;
    FlightStart launch(flow, flight);
    press(launch, StartButton::Proceed);
    for (int option = 0; option < 4; ++option) press(launch, StartButton::NextOption);
    press(launch, StartButton::Proceed);
    require(flow.stage() == StartStage::StartRequested && flow.selected_option_index() == 4 &&
                flight.selected_option() == 4 && !flight.two_players() &&
                flight.status() == FlightStatus::Preparing && flight.bridge_number() == 20 &&
                flight.world().river_state().section == 20,
            "Normal menu did not select Game 5");

    std::size_t waiting_slices = 0, waiting_updates = 0;
    for (int update = 0; update < 84; ++update) {
        std::size_t admitted = 0;
        for (int slice = 0; slice < 4 && admitted == 0; ++slice) {
            admitted = launch.advance_elapsed(10ms);
            (void)flight.take_audio();
            ++waiting_slices;
            require(admitted <= 1, "Game 5 launch observation admitted multiple updates");
        }
        require(admitted == 1, "Game 5 launch did not admit one bounded update");
        waiting_updates += admitted;
    }
    test::RegularGapObserver observer;
    require(waiting_slices == 168 && waiting_updates == 84 &&
                flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::AwaitInput && flight.lives() == 3 &&
                flight.world().generated_rows() == 156 && flight.bridge_number() == 20 &&
                flight.score() == 0 && !observer.armed() && !observer.confirmed() &&
                !flight.world().bridge_sprite_state().automatic_entry_enabled,
            "Game 5 launch gate changed or already supplied the regular-gap goal");

    std::size_t slices = 0, updates = 0, bridge_changes = 0;
    std::size_t arms = 0, entries = 0, publications = 0, confirmations = 0;
    std::size_t publication_update = 0, confirmation_update = 0;
    for (const auto run : test::game5_regular_gap_route) {
        require(run.direction >= 0 && run.direction <= 2 && run.slices > 0,
                "Invalid ordinary Game 5 route input");
        launch.set_controls({false, false, run.direction == 2, run.direction == 1, run.fire});
        for (int slice = 0; slice < run.slices; ++slice) {
            // Each 20-ms held-input slice is observed in two ordinary 10-ms halves.
            for (int half = 0; half < 2; ++half) {
                const auto before = observation(flight);
                const bool was_armed = observer.armed();
                const bool was_pending = observer.awaiting_survival();
                const bool was_confirmed = observer.confirmed();
                const auto admitted = launch.advance_elapsed(10ms);
                (void)flight.take_audio();
                require(admitted <= 1, "Game 5 route observation admitted multiple updates");
                updates += admitted;
                const auto after = observation(flight);
                (void)observer.observe(before, after, admitted);
                require(flight.selected_option() == 4 && !flight.two_players() &&
                            flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                            (updates == 0 || first_life(flight)),
                        "Fixed Game 5 route lost its first life");
                if (before.bridge != after.bridge) {
                    ++bridge_changes;
                    require(!was_armed && !observer.armed() && before.bridge == 20 &&
                                after.bridge == 21,
                            "Game 5 bridge progression changed or corrupted regular origin");
                }
                if (!was_armed && observer.armed()) {
                    ++arms;
                    require(admitted == 1 && !before.mechanics.crossing &&
                                before.bridge == after.bridge && after.bridge == 21 &&
                                flight.world().generated_rows() == 1792 &&
                                after.mechanics.automatic_entry_enabled &&
                                normal_crossing(after.mechanics) &&
                                after.mechanics.gap_animation_counter == 0xff,
                            "Game 5 regular crossing origin changed");
                    scene_capture.record("game5_crossing_origin", flight);
                }
                if (was_armed && observer.armed()) {
                    require(before.bridge == 21 && after.bridge == 21 &&
                                after.mechanics.automatic_entry_enabled &&
                                normal_crossing(after.mechanics),
                            "Game 5 regular instance changed before automatic entry");
                    if (admitted == 1 && before.mechanics.gap_animation_counter == 0xff &&
                        after.mechanics.gap_animation_counter <= 1) {
                        ++entries;
                        scene_capture.record("game5_gap_entry", flight);
                    }
                }
                if (!was_pending && observer.awaiting_survival()) {
                    ++publications;
                    publication_update = updates;
                    require(admitted == 1 && !observer.confirmed(),
                            "Game 5 entry publication incorrectly credited survival");
                }
                if (!was_confirmed && observer.confirmed()) {
                    ++confirmations;
                    confirmation_update = updates;
                    require(was_pending && admitted == 1 &&
                                confirmation_update == publication_update + 1,
                            "Game 5 entry lacked a separate following surviving update");
                }
            }
            ++slices;
        }
    }
    require(slices == test::game5_regular_gap_route_slices && updates == 1674 &&
                bridge_changes == 1 && arms == 1 && entries == 1 && publications == 1 &&
                confirmations == 1 && confirmation_update == updates && observer.confirmed() &&
                first_life(flight) && !flight.life_cycle().cause &&
                flight.world().generated_rows() == 1822 &&
                flight.steering().horizontal_position == 89 && flight.bridge_number() == 21 &&
                flight.fuel() == 32703 && flight.score() == 2650,
            "Full fixed Game 5 route endpoint or regular-gap history changed");

    const auto& witness = observer.witness();
    require(witness && witness->bridge == 21 && witness->flying && witness->lives == 3 &&
                witness->life_phase == LifeCyclePhase::Active &&
                witness->mechanics.automatic_entry_enabled &&
                witness->published.automatic_entry_enabled &&
                witness->mechanics.gap_animation_counter == 1 &&
                witness->published.gap_animation_counter == 1 &&
                sprite_at(witness->mechanics.crossing, BridgeSpriteImage::CrossingRightA, 56, 62) &&
                sprite_at(witness->published.crossing, BridgeSpriteImage::CrossingRightA, 56, 61) &&
                sprite_at(witness->mechanics.gap, BridgeSpriteImage::GapFlight, 70, 58) &&
                sprite_at(witness->published.gap, BridgeSpriteImage::GapFlight, 70, 57),
            "Game 5 automatic regular-gap witness changed");

    // This copy checks queued death only; it cannot supply route goal credit.
    auto neutral_probe = flight;
    neutral_probe.set_controls({});
    std::size_t probe_updates = 0;
    for (int half = 0; half < 2; ++half) {
        const auto admitted = neutral_probe.advance_elapsed(10ms);
        (void)neutral_probe.take_audio();
        require(admitted <= 1 && first_life(neutral_probe) && !neutral_probe.life_cycle().cause,
                "Game 5 automatic entry queued a fatal next-update contact");
        probe_updates += admitted;
    }
    require(probe_updates >= 1 && observer.confirmed(),
            "Game 5 neutral guard did not check a following update");
    complete_regular_gap(launch, flight);

    const auto connected_score = flight.score();
    require(connected_score >= 2650 && flight.bridge_number() == 21 &&
                flight.life_cycle().phase == LifeCyclePhase::Active &&
                flight.world().river_state().section == 21,
            "Game 5 gap completion lost its connected section and score");
    for (int reserves = 3; reserves >= 0; --reserves) {
        launch.set_controls({false, false, false, true, false});
        for (int update = 0; update < 2000 &&
                                  flight.status() == FlightStatus::Flying; ++update)
            tick(launch, flight);
        require(flight.status() == FlightStatus::Crashed &&
                    flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                    flight.life_cycle().cause == LifeEndCause::Collision &&
                    flight.lives() == reserves && flight.bridge_number() == 21 &&
                    flight.score() == connected_score,
                "Natural Game 5 collision changed connected progress");
        const auto selected_checkpoint = flight.world().selected_checkpoint();
        launch.set_controls({});
        for (int update = 0; update < 127; ++update) tick(launch, flight);
        if (reserves == 0) break;
        require(flight.life_cycle().phase == LifeCyclePhase::Rebuilding &&
                    flight.fuel() == kInitialFuel &&
                    flight.last_consumed_contacts() == ForegroundContactSnapshot{} &&
                    flight.shot().pose == PlayerShotPose::Hidden &&
                    checkpoint(flight) == selected_checkpoint,
                "Game 5 checkpoint restore retained stale state or wrong anchor");
        for (int update = 0; update < 83; ++update) tick(launch, flight);
        require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                    flight.status() == FlightStatus::Flying &&
                    flight.lives() == reserves - 1 && flight.bridge_number() == 21 &&
                    flight.score() == connected_score &&
                    flight.world().generated_rows() == 156 &&
                    checkpoint(flight) == selected_checkpoint,
                "Rebuilt Game 5 life lost checkpoint, score, or reserves");
    }
    require(flight.status() == FlightStatus::GameOver &&
                flight.life_cycle().phase == LifeCyclePhase::Terminal &&
                flight.lives() == 0 && flight.bridge_number() == 21 &&
                flight.score() == connected_score && flight.high_score() == connected_score,
            "Game 5 did not reach terminal state with connected progress");
    scene_capture.record("game5_game_over", flight);
    press(launch, StartButton::Proceed);
    require(flight.status() == FlightStatus::Preparing &&
                flight.selected_option() == 4 && !flight.two_players() &&
                flight.bridge_number() == 20 && flight.world().river_state().section == 20 &&
                flight.score() == 0 && flight.world().generated_rows() == 0 &&
                flight.high_score() == connected_score,
            "F1 restart lost Game 5 preset or high score");
    for (int update = 0; update < 84; ++update) tick(launch, flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.lives() == 3 && flight.bridge_number() == 20 &&
                flight.score() == 0 && flight.world().generated_rows() == 156 &&
                flight.high_score() == connected_score,
            "Restart did not rebuild Game 5 initial checkpoint");
    scene_capture.record("game5_restart_ready", flight);
}
} // namespace

int main(int argc, char** argv) {
    try {
        scene_capture.configure(argc, argv);
        run_session();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
