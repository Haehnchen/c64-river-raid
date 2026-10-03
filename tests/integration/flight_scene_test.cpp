#include "app/flight_scene.hpp"
#include "support/flight_start_helpers.hpp"

#include "app/auxiliary_overlay.hpp"
#include "app/bridge_overlay.hpp"
#include "app/flight_hud.hpp"
#include "app/player_overlay.hpp"
#include "app/shot_overlay.hpp"
#include "app/world_scene_setup.hpp"
#include "app/player_contact.hpp"
#include "game_data.hpp"
#include "presentation_palette.hpp"
#include "render/world_object_schedule.hpp"

#include <chrono>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace river_raid;

std::filesystem::path capture_directory;

void capture(std::string_view name, const Frame& frame) {
    if (capture_directory.empty()) return;
    const auto path = capture_directory / (std::string(name) + ".ppm");
    if (std::filesystem::exists(path)) throw std::runtime_error("Capture already exists");
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path, std::ios::binary);
    output << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (const auto pixel : frame.pixels) {
        for (const auto shift : {16, 8, 0}) output.put(static_cast<char>((pixel >> shift) & 255));
    }
    output.close();
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool differs(const Frame& first, const Frame& second) {
    return first.width != second.width || first.height != second.height ||
        first.pixels != second.pixels;
}

bool explosion_image(LifeCycleImage image) {
    return image == LifeCycleImage::ExplosionA || image == LifeCycleImage::ExplosionB;
}

Frame scene_without_player(const FlightPreview& flight, std::size_t selected_option,
                           const PlayerSteeringState& shot_player,
                           const std::vector<WorldObjectBandSchedule>& bands,
                           const PlayerShotResult* shot_override = nullptr,
                           const BridgeSpriteState* bridge_override = nullptr) {
    const auto& active_world = flight.world();
    std::vector<WorldObjectPlacement> placements;
    for (const auto& band : bands) {
        placements.insert(placements.end(), band.placements.begin(), band.placements.end());
    }
    auto image = make_world_scene(std::span(active_world.viewport().rows()).first(157), placements,
                                  selected_option, flight.bridge_flash());
    draw_bridge_sprites(image, bridge_override ? *bridge_override
                                               : active_world.bridge_presentation_state());
    draw_auxiliary_projectile(image, active_world.auxiliary_projectile_state());
    draw_shot_preview(image, shot_player, shot_override ? *shot_override : flight.shot());
    draw_flight_hud(image, flight.score(), flight.lives(), flight.fuel(), flight.bridge_number());
    return image;
}

Frame scene_without_player(const FlightPreview& flight, std::size_t selected_option,
                           const PlayerSteeringState& shot_player,
                           const PlayerShotResult* shot_override = nullptr,
                           const BridgeSpriteState* bridge_override = nullptr) {
    return scene_without_player(flight, selected_option, shot_player,
                                flight.presented_object_bands(), shot_override,
                                bridge_override);
}

void test_normal_scene_is_deterministic_and_composed() {
    FlightPreview flight;
    flight.start();
    river_raid::test::wait_for_launch(flight);

    const auto first = make_flight_scene(flight);
    const auto second = make_flight_scene(flight);
    auto expected = scene_without_player(flight, 0, flight.presented_steering());
    draw_player_preview(expected, flight.presented_steering(), flight.presented_pose(),
                        flight.presented_image());
    require(first.width == 320 && first.height == 200 &&
                first.pixels.size() == 320U * 200U,
            "Flight scene dimensions changed");
    require(first.pixels == second.pixels, "Flight scene was not deterministic");
    require(first.pixels == expected.pixels,
            "Flight scene draw order or player composition changed");
    capture("launch", first);
    const auto expected_hud_pixel = assets::presentation_palette[
        assets::hud_tiles[assets::hud_cells[0]][4U * 8U]];
    require(first.pixels[180U * first.width] == expected_hud_pixel,
            "Flight scene lost the source HUD tile");

    FlightPreview alternate;
    alternate.start(1);
    test::wait_for_launch(alternate);
    const auto alternate_option = make_flight_scene(alternate);
    require(differs(first, alternate_option), "Selected option did not reach flight composition");
}

void advance_one_tick(FlightPreview& flight) {
    using namespace std::chrono_literals;
    for (int slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        require(count <= 1, "Ten-millisecond slice admitted multiple foreground ticks");
        if (count == 1) return;
    }
    throw std::runtime_error("Foreground pass was not admitted");
}

void test_scene_renders_published_bridge_sprites_in_attract() {
    FlightPreview flight;
    flight.show_options(6, true);
    for (int pass = 0; pass < 4 && flight.status() != FlightStatus::Attract; ++pass) {
        advance_one_tick(flight);
    }
    require(flight.status() == FlightStatus::Attract,
            "Bridge publication scene fixture did not enter attract");

    bool saw_visible_publication_difference = false;
    for (int pass = 0; pass < 10'000; ++pass) {
        advance_one_tick(flight);
        require(flight.status() == FlightStatus::Attract &&
                    flight.life_cycle().image == LifeCycleImage::Hidden,
                "Bridge publication scene fixture left the hidden-aircraft attract path");

        const auto mechanical = flight.world().bridge_sprite_state();
        const auto published = flight.world().bridge_presentation_state();
        if (mechanical.crossing == published.crossing && mechanical.gap == published.gap) continue;

        const auto actual = make_flight_scene(flight);
        auto expected = scene_without_player(
            flight, flight.selected_option(), flight.presented_steering(),
            flight.presented_object_bands(), nullptr, &published);
        draw_player_preview(expected, flight.presented_steering(), flight.presented_pose(),
                            flight.presented_image());
        require(actual.pixels == expected.pixels,
                "Attract scene did not render the last published bridge placement");

        auto current_world = scene_without_player(
            flight, flight.selected_option(), flight.presented_steering(),
            flight.presented_object_bands(), nullptr, &mechanical);
        draw_player_preview(current_world, flight.presented_steering(), flight.presented_pose(),
                            flight.presented_image());
        if (actual.pixels != current_world.pixels) {
            saw_visible_publication_difference = true;
            break;
        }
    }
    require(saw_visible_publication_difference,
            "Attract route did not expose a visible current/published bridge difference");
}

void test_ordinary_fire_publishes_new_shot_y_in_scene() {
    FlightPreview flight;
    flight.start();
    test::wait_for_launch(flight);
    require(flight.shot().pose == PlayerShotPose::Hidden,
            "Launch retained a projectile before ordinary fire");

    const auto pre_fire_steering = flight.steering();
    const auto pre_fire_pose = flight.pose();
    flight.set_controls({false, false, true, false, true});
    advance_one_tick(flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active,
            "First held-fire pass did not admit active flight");
    require(flight.shot().pose == PlayerShotPose::Visible &&
                flight.shot().state.vertical_position == 192,
            "First held-fire pass did not publish shot Y=192");
    require(flight.presented_steering() == pre_fire_steering &&
                flight.presented_pose() == pre_fire_pose &&
                flight.steering().horizontal_position !=
                    flight.presented_steering().horizontal_position,
            "First shot did not retain pre-control aircraft X and pose");
    const auto first_shot = flight.shot();
    const auto first_x = flight.presented_steering().horizontal_position;
    const auto next_published_steering = flight.steering();
    const auto first_scene = make_flight_scene(flight);
    const auto shot_pixel_x = 2 * static_cast<int>(first_x) - 12;
    for (int y = 142; y < 147; ++y) {
        for (int x = shot_pixel_x; x < shot_pixel_x + 2; ++x) {
            require(first_scene.pixels[static_cast<std::size_t>(y) * first_scene.width + x] ==
                        assets::presentation_palette[1],
                    "First scene did not draw the Y=192 projectile at published X");
        }
    }

    advance_one_tick(flight);
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active,
            "Second held-fire pass left active flight");
    require(flight.shot().pose == PlayerShotPose::Visible &&
                flight.shot().state.vertical_position == 188,
            "Second held-fire pass did not advance the shot upward by four");
    require(flight.presented_steering() == next_published_steering &&
                flight.presented_steering().horizontal_position != first_x,
            "Second shot did not use the newly published pre-control X");

    const auto actual = make_flight_scene(flight);
    const auto second_shot_pixel_x =
        2 * static_cast<int>(flight.presented_steering().horizontal_position) - 12;
    for (int y = 138; y < 143; ++y) {
        for (int x = second_shot_pixel_x; x < second_shot_pixel_x + 2; ++x) {
            require(actual.pixels[static_cast<std::size_t>(y) * actual.width + x] ==
                        assets::presentation_palette[1],
                    "Second scene did not draw the Y=188 projectile at published X");
        }
    }
    auto expected = scene_without_player(flight, 0, flight.presented_steering());
    draw_player_preview(expected, flight.presented_steering(), flight.presented_pose(),
                        flight.presented_image());
    require(actual.pixels == expected.pixels,
            "Second scene did not compose the updated projectile state");

    auto stale_y = scene_without_player(flight, 0, flight.presented_steering(), &first_shot);
    draw_player_preview(stale_y, flight.presented_steering(), flight.presented_pose(),
                        flight.presented_image());
    require(differs(actual, stale_y),
            "Second scene was indistinguishable from rendering the previous shot Y");
}

void test_scene_uses_published_aircraft_tuple() {
    FlightPreview flight;
    require(flight.presented_steering() == flight.steering() &&
                flight.presented_pose() == flight.pose() &&
                flight.presented_object_bands() ==
                    schedule_world_object_snapshot(flight.world().river_state().phase,
                                                   flight.world().object_state()),
            "Constructed flight retained a different presentation tuple");
    flight.start();
    require(flight.presented_steering() == flight.steering() &&
                flight.presented_pose() == flight.pose() &&
                flight.presented_object_bands() ==
                    schedule_world_object_snapshot(flight.world().river_state().phase,
                                                   flight.world().object_state()),
            "New round retained a previous presentation tuple or object scene");
    river_raid::test::wait_for_launch(flight);

    bool saw_pose_difference = false;
    const auto advance_with_presentation_check = [&](FlightControls controls,
                                                     FlightPose expected_pose) {
        flight.set_controls(controls);
        const auto pre_tick_steering = flight.steering();
        const auto pre_tick_pose = flight.pose();
        advance_one_tick(flight);
        require(flight.status() == FlightStatus::Flying &&
                    !flight.life_cycle().cause.has_value(),
                "Bounded ordinary-input reversal left active flight unexpectedly");
        require(flight.presented_steering() == pre_tick_steering &&
                    flight.presented_pose() == pre_tick_pose,
                "Admitted tick did not publish its pre-tick steering tuple");
        require(flight.pose() == expected_pose,
                "Current pose did not follow the admitted ordinary direction");
        saw_pose_difference |= flight.presented_pose() != flight.pose();
    };

    // Exercise a real direction reversal without touching gameplay state:
    // each admitted tick exposes its prior controls for rendering, then applies
    // the currently held direction. Fire remains held to retain the shot check.
    for (unsigned tick = 0; tick < 16; ++tick) {
        advance_with_presentation_check({false, false, true, false, true}, FlightPose::Left);
    }
    for (unsigned tick = 0; tick < 20; ++tick) {
        advance_with_presentation_check({false, false, false, true, true}, FlightPose::Right);
        if (flight.presented_steering().horizontal_position !=
            flight.steering().horizontal_position) break;
    }
    require(saw_pose_difference,
            "Ordinary steering did not create a current/published pose difference");
    require(flight.presented_steering().horizontal_position !=
                flight.steering().horizontal_position,
            "Bounded held steering did not create a current/published X difference");
    require(flight.shot().pose == PlayerShotPose::Visible,
            "Ordinary fire did not leave a visible projectile for the placement check");

    const auto actual = make_flight_scene(flight);
    auto expected = scene_without_player(flight, 0, flight.presented_steering());
    draw_player_preview(expected, flight.presented_steering(), flight.presented_pose(),
                        flight.presented_image());
    require(actual.pixels == expected.pixels,
            "Aircraft scene did not render the retained presentation tuple");

    auto current_controls = scene_without_player(flight, 0, flight.steering());
    draw_player_preview(current_controls, flight.steering(), flight.pose(),
                        flight.presented_image());
    require(actual.pixels != current_controls.pixels,
            "Aircraft placement did not distinguish current X from published X");

    auto current_shot = scene_without_player(flight, 0, flight.steering());
    draw_player_preview(current_shot, flight.presented_steering(), flight.presented_pose(),
                        flight.presented_image());
    require(actual.pixels != current_shot.pixels,
            "Shot placement did not use the aircraft's published horizontal position");

    for (unsigned tick = 0; tick < 2; ++tick) {
        advance_with_presentation_check({false, false, false, false, true},
                                        FlightPose::Straight);
    }

    const auto previous_round_bands = flight.presented_object_bands();
    flight.show_options(flight.selected_option(), false);
    require(flight.presented_object_bands() == previous_round_bands,
            "Options entry discarded placements for the retained live world");
    flight.start();
    require(flight.presented_steering() == flight.steering() &&
                flight.presented_pose() == flight.pose() &&
                flight.presented_object_bands() ==
                    schedule_world_object_snapshot(flight.world().river_state().phase,
                                                   flight.world().object_state()),
            "Restart retained the prior round's presentation tuple or object scene");
    require(previous_round_bands != flight.presented_object_bands(),
            "Restart fixture did not distinguish the old object scene from its new world");
}

void test_retained_object_scene_survives_natural_history_shift() {
    using namespace std::chrono_literals;

    FlightPreview flight;
    flight.start();
    flight.set_controls({});
    river_raid::test::wait_for_launch(flight);
    flight.set_controls({false, false, false, false, true});
    advance_one_tick(flight);
    flight.set_controls({});
    require(flight.status() == FlightStatus::Flying &&
                flight.life_cycle().phase == LifeCyclePhase::Active,
            "Neutral route did not resume from its ordinary fire pulse");

    auto generating_objects = flight.world().object_state();
    auto generating_rows = flight.world().generated_rows();
    for (std::size_t pass = 0; pass < 1000; ++pass) {
        const auto objects_before = flight.world().object_state();
        const auto rows_before = flight.world().generated_rows();
        const bool shifted_before_contact =
            rows_before == generating_rows + 1 &&
            generating_objects.active_begin < kWorldObjectCapacity &&
            generating_objects.records.back().vertical_position == 211 &&
            objects_before.active_begin == generating_objects.active_begin + 1 &&
            objects_before.records[11].vertical_position ==
                generating_objects.records[10].vertical_position + 1 &&
            objects_before.records[11].base_kind ==
                generating_objects.records[10].base_kind;

        bool shift_frame_uses_retained_bands = false;
        bool shift_frame_differs_from_current_snapshot = false;
        if (shifted_before_contact) {
            const auto retained_bands = flight.presented_object_bands();
            const auto current_bands = schedule_world_object_snapshot(
                flight.world().river_state().phase, objects_before);
            const auto actual = make_flight_scene(flight);
            auto expected = scene_without_player(
                flight, 0, flight.presented_steering(), retained_bands);
            draw_player_preview(expected, flight.presented_steering(),
                                flight.presented_pose(), flight.presented_image());
            shift_frame_uses_retained_bands = actual.pixels == expected.pixels;
            auto legacy = scene_without_player(
                flight, 0, flight.presented_steering(), current_bands);
            draw_player_preview(legacy, flight.presented_steering(),
                                flight.presented_pose(), flight.presented_image());
            shift_frame_differs_from_current_snapshot = actual.pixels != legacy.pixels;
        }

        advance_one_tick(flight);
        const auto selected = select_player_contact(
            flight.last_consumed_contacts().player, objects_before);
        if (selected.object_contact) {
            require(shifted_before_contact &&
                        selected.object_contact->record_index == 11 &&
                        shift_frame_uses_retained_bands &&
                        shift_frame_differs_from_current_snapshot &&
                        flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                        flight.world().generated_rows() > rows_before,
                    "Deferred enemy contact did not match its retained history-shift scene");
            return;
        }
        if (!selected.object_contact) {
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "Neutral scene route crashed before its history-shift contact");
            generating_objects = objects_before;
            generating_rows = rows_before;
            continue;
        }
    }
    throw std::runtime_error("Neutral scene route did not reach the bounded history-shift witness");
}

void test_natural_terminal_states_remain_renderable() {
    FlightPreview flight;
    flight.start();
    // Held steering is ordinary input and also lets the rebuilt life leave
    // its source AwaitInput phase without a test-only lifecycle shortcut.
    flight.set_controls({false, false, false, true, false});
    bool saw_crashed = false;
    bool saw_terminal = false;
    bool captured_crash = false;
    std::array<bool, 2> captured_rebuild{}, captured_reveal{};
    bool saw_entry_lag = false;
    bool saw_animation_lag = false;
    bool saw_hide_lag = false;
    bool saw_reveal_lag = false;
    std::array<bool, 4> caught_up{};
    bool catch_up_pending = false;
    LifeCycleImage catch_up_image = LifeCycleImage::Hidden;
    std::size_t catch_up_kind = 0;
    for (unsigned tick = 0; tick < 12'000 && !saw_terminal; ++tick) {
        const auto before = flight.life_cycle();
        advance_one_tick(flight);
        const auto state = flight.life_cycle();
        const auto presented = flight.presented_image();
        const bool phase_changed = state.phase != before.phase;
        const bool image_changed = state.image != before.image;
        bool caught_up_this_tick = false;
        if (catch_up_pending) {
            require(presented == catch_up_image,
                    "A delayed lifecycle image did not catch up on the next admitted pass");
            caught_up[catch_up_kind] = true;
            catch_up_pending = false;
            caught_up_this_tick = true;
        }
        if (image_changed && presented != state.image) {
            if (before.phase != LifeCyclePhase::Exploding &&
                state.phase == LifeCyclePhase::Exploding &&
                state.image == LifeCycleImage::ExplosionA) {
                require(presented == before.image,
                        "Collision entry did not retain the previously programmed image");
                saw_entry_lag = true;
                catch_up_kind = 0;
            } else if (before.phase == LifeCyclePhase::Exploding &&
                       state.phase == LifeCyclePhase::Exploding &&
                       explosion_image(before.image) && explosion_image(state.image)) {
                require(presented == before.image,
                        "Explosion-frame transition did not retain the prior frame");
                saw_animation_lag = true;
                catch_up_kind = 1;
            } else if (before.phase == LifeCyclePhase::Exploding &&
                       state.phase == LifeCyclePhase::Exploding &&
                       explosion_image(before.image) && state.image == LifeCycleImage::Hidden) {
                require(presented == before.image,
                        "Explosion hide transition did not retain the prior visible frame");
                saw_hide_lag = true;
                catch_up_kind = 2;
            } else if (before.phase == LifeCyclePhase::Rebuilding &&
                       state.phase == LifeCyclePhase::AwaitInput &&
                       before.image == LifeCycleImage::Hidden &&
                       state.image == LifeCycleImage::Straight) {
                require(presented == LifeCycleImage::Hidden,
                        "Reveal did not retain the hidden image for its publication pass");
                saw_reveal_lag = true;
                catch_up_kind = 3;
            } else {
                throw std::runtime_error("Unexpected private/presented lifecycle image split");
            }
            catch_up_pending = true;
            catch_up_image = state.image;
        }

        const bool image_mismatch = presented != state.image;
        const auto reveal_index = (state.cause.has_value() || before.cause.has_value())
                                      ? 1U : 0U;
        const bool reveal_window = state.phase == LifeCyclePhase::AwaitInput ||
                                   before.phase == LifeCyclePhase::AwaitInput;
        const bool reveal_scene_needed = reveal_window &&
                                         presented == LifeCycleImage::Straight &&
                                         !captured_reveal[reveal_index];
        if (!phase_changed && !image_mismatch && !caught_up_this_tick &&
            !reveal_scene_needed) continue;

        const auto image = make_flight_scene(flight);
        require(image.width == 320 && image.height == 200,
                "Lifecycle flight scene dimensions changed");
        auto expected = scene_without_player(flight, 0, flight.presented_steering());
        draw_player_preview(expected, flight.presented_steering(), flight.presented_pose(),
                            presented);
        require(image.pixels == expected.pixels,
                "Lifecycle scene did not compose the presented aircraft image");
        if (reveal_scene_needed) {
            capture(reveal_index == 0 ? "startup_reveal" : "respawn_reveal", image);
            captured_reveal[reveal_index] = true;
        }
        if (image_mismatch) {
            auto private_image = scene_without_player(flight, 0,
                                                      flight.presented_steering());
            draw_player_preview(private_image, flight.presented_steering(),
                                flight.presented_pose(), state.image);
            require(differs(image, private_image),
                    "Scene did not distinguish the private-image counterfactual");
        }
        if (state.phase == LifeCyclePhase::Exploding && state.cause) {
            if (!captured_crash) {
                capture("crash", image);
                captured_crash = true;
            }
            saw_crashed = true;
            require(explosion_image(state.image) || state.image == LifeCycleImage::Hidden,
                    "Private crash lifecycle did not select an explosion image");
            if (presented != LifeCycleImage::Hidden) {
                require(image.pixels != scene_without_player(
                            flight, 0, flight.presented_steering()).pixels,
                        "Published crash scene did not visibly draw the aircraft");
            }
        }
        if (state.phase == LifeCyclePhase::Rebuilding) {
            const auto index = reveal_index;
            if (!captured_rebuild[index]) {
                capture(index == 0 ? "startup_rebuild" : "respawn_rebuild", image);
                captured_rebuild[index] = true;
            }
            require(state.image == LifeCycleImage::Hidden,
                    "Rebuilding scene did not hide the aircraft");
            require(image.pixels == scene_without_player(flight, 0, flight.presented_steering()).pixels,
                    "Rebuilding scene retained stale aircraft pixels");
            require(flight.presented_steering().horizontal_position ==
                        flight.steering().horizontal_position &&
                        flight.presented_pose() == flight.pose(),
                    "Checkpoint rebuild retained a prior presentation position or pose");
        }
        if (state.phase == LifeCyclePhase::AwaitInput) {
            require(state.image == LifeCycleImage::Straight,
                    "Reveal lifecycle did not select the normal aircraft image");
            if (presented == LifeCycleImage::Hidden) {
                require(image.pixels == scene_without_player(
                            flight, 0, flight.presented_steering()).pixels,
                        "Reveal publication pass did not retain the hidden image");
            } else {
                require(image.pixels != scene_without_player(
                            flight, 0, flight.presented_steering()).pixels,
                        "Caught-up reveal scene remained without the aircraft");
            }
        }
        if (state.phase == LifeCyclePhase::Terminal) {
            capture("terminal", image);
            saw_terminal = true;
            require(state.image == LifeCycleImage::Hidden,
                    "Terminal scene did not hide the aircraft");
            require(image.pixels == scene_without_player(flight, 0, flight.presented_steering()).pixels,
                    "Terminal scene retained hidden-aircraft pixels");
        }
    }
    require(saw_crashed, "Natural flight route did not expose a crash scene");
    require(saw_terminal, "Natural flight route did not expose the game-over scene");
    require(saw_entry_lag && saw_animation_lag && saw_hide_lag && saw_reveal_lag &&
                caught_up[0] && caught_up[1] && caught_up[2] && caught_up[3],
            "Natural route missed one-pass collision, animation, hide, or reveal publication");
    require(captured_rebuild[0] && captured_rebuild[1] &&
                captured_reveal[0] && captured_reveal[1],
            "Natural route missed initial or respawn rebuild/reveal scenes");
    const auto game_over = make_flight_scene(flight);
    require(game_over.width == 320 && game_over.height == 200,
            "Game-over scene dimensions changed");
    require(game_over.pixels == scene_without_player(flight, 0, flight.presented_steering()).pixels,
            "Game-over scene did not hide the aircraft");
    require(game_over.pixels == make_flight_scene(flight).pixels,
            "Game-over scene was not deterministic");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--capture-dir") {
            capture_directory = argv[2];
            if (capture_directory.empty()) throw std::runtime_error("Empty capture directory");
            std::filesystem::create_directories(capture_directory);
        } else if (argc != 1) {
            throw std::runtime_error("Usage: flight_scene_test [--capture-dir DIRECTORY]");
        }
        test_normal_scene_is_deterministic_and_composed();
        test_scene_renders_published_bridge_sprites_in_attract();
        test_ordinary_fire_publishes_new_shot_y_in_scene();
        test_scene_uses_published_aircraft_tuple();
        test_retained_object_scene_survives_natural_history_shift();
        test_natural_terminal_states_remain_renderable();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
