#include "app/flight_preview.hpp"
#include "app/bridge_overlay.hpp"
#include "support/flight_start_helpers.hpp"
#include "fixtures/joint_bridge_witness.hpp"
#include "game/shot_contact.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void advance_one_tick(FlightPreview& flight) {
    for (int slice = 0; slice < 4; ++slice) {
        const auto count = flight.advance_elapsed(10ms);
        require(count <= 1, "Ten-millisecond slice admitted multiple foreground ticks");
        if (count == 1) return;
    }
    throw std::runtime_error("Foreground pass was not admitted");
}

struct InputRun { int direction; int frames; };

// Recorded 20-ms input slices, not C64 frames; 0 = centered, 1 = right, 2 = left.
// The default route holds fire; the aircraft approach stops it at row 900.
constexpr InputRun first_bridge_route[]{
    {0,19},{2,1},{0,13},{1,8},{0,7},{2,1},{0,5},{2,10},{0,49},{1,11},
    {0,9},{1,6},{0,4},{2,11},{0,6},{1,6},{0,3},{2,1},{0,9},{1,6},
    {0,17},{2,1},{0,18},{1,14},{0,22},{2,1},{0,2},{2,1},{0,1},{2,1},
    {0,8},{1,8},{0,5},{2,12},{0,8},{1,6},{0,5},{1,13},{0,36},{2,1},
    {0,12},{1,5},{0,24},{2,1},{0,1},{2,1},{0,3},{2,1},{0,5},{2,1},
    {0,4},{2,1},{0,6},{1,10},{0,2},{1,6},{0,7},{1,6},{0,6},{1,1},
    {0,8},{2,1},{0,6},{1,8},{0,27},{2,1},{0,1},{2,1},{0,3},{2,1},
    {0,1},{2,1},{0,1},{2,1},{0,8},{1,8},{0,3},{2,1},{0,2},{2,1},
    {0,2},{2,1},{0,40},{1,14},{0,30},{2,1},{0,1},{2,1},{0,3},{2,1},
    {0,7},{1,8},{0,6},{2,11},{0,13},{1,13},{0,10},{2,1},{0,7},{1,8},
    {0,5},{2,11},{0,14},{1,10},{0,3},{2,1},{0,3},{2,8},{0,13},{1,13},
    {0,8},{2,1},{0,1},{2,1},{0,7},{2,10},{0,31},{1,5},{0,12},
};

// The retained contact generation needs an earlier left dodge of the expanded
// enemy, then two more right slices before the next terrain bend.
constexpr InputRun aircraft_bridge_approach[]{
    {0,174},{2,4},{1,1},{2,2},{0,1},{1,1},{2,2},{1,11},{0,12},{1,6},{0,13},
    {2,1},{0,1},{2,1},{0,1},{2,1},{0,1},{2,1},{0,1},{2,1},{0,1},
    {2,1},{0,1},{1,1},{2,3},{1,1},{2,1},{1,1},{2,1},{0,23},
};

void drive_first_bridge_route(FlightPreview& flight, bool shoot_bridge) {
    flight.start();
    river_raid::test::wait_for_launch(flight);
    require(flight.life_cycle().phase == LifeCyclePhase::AwaitInput &&
                flight.world().generated_rows() == 156 && flight.lives() == 3,
            "New game did not finish the source launch rebuild");
    require(flight.bridge_number() == 1, "New game did not start at bridge one");
    for (const auto run : first_bridge_route) {
        for (int frame = 0; frame < run.frames; ++frame) {
            flight.set_controls({false, false, run.direction == 2, run.direction == 1,
                                 shoot_bridge || flight.world().generated_rows() < 900});
            flight.advance_elapsed(20ms);
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "Recorded route lost a life before destroying bridge one");
            if (shoot_bridge && flight.bridge_number() == 2) return;
            if (!shoot_bridge && flight.world().generated_rows() >= 900) return;
        }
    }
}

void reach_first_bridge(FlightPreview& flight) {
    drive_first_bridge_route(flight, true);
    require(flight.bridge_number() == 2 && flight.score() == 1000,
            "Normal bridge shot did not update bridge number and score once");
    require(flight.world().generated_rows() == 1033,
            "Retained bridge contact did not publish on the next input row");
    require(flight.bridge_flash(), "Bridge destruction did not start the terrain flash");
    require(flight.audio_state().explosion_countdown == 31,
            "Bridge explosion was consumed before the audio pass");
    const auto sprites = flight.world().bridge_presentation_state();
    require(sprites.crossing.has_value(), "Normal bridge route did not generate its crossing sprite");
    Frame sprite_frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0x12345678)};
    draw_bridge_sprites(sprite_frame, sprites);
    bool visible = false;
    for (const auto pixel : sprite_frame.pixels) visible |= pixel != 0x12345678;
    require(visible, "Generated crossing sprite produced no visible pixels");
}

void expire_bridge_flash(FlightPreview& flight) {
    flight.set_controls({});
    for (int tick = 1; tick <= 23; ++tick) {
        std::size_t advanced = 0;
        for (int wait = 0; wait < 4 && advanced == 0; ++wait) {
            advanced = flight.advance_elapsed(10ms);
        }
        require(advanced == 1 && flight.bridge_flash() == ((23 - tick) % 2 != 0),
                "Bridge flash did not alternate and expire after 23 updates");
        if (tick == 1) {
            require(flight.audio_state().explosion_countdown == 30 &&
                        flight.audio_state().output.voices[1].control == AudioVoiceControl::NoiseGate,
                    "Pending bridge explosion did not reach the following audio pass");
        }
    }
}

void respawn_after_bridge(FlightPreview& flight, bool expect_joint_retirement = false) {
    const auto score = flight.score();
    flight.set_controls({false, false, false, true, false});
    for (int frame = 0; frame < 200 && flight.status() == FlightStatus::Flying; ++frame) {
        flight.advance_elapsed(20ms);
    }
    require(flight.status() == FlightStatus::Crashed && flight.lives() == 3,
            "Post-bridge crash consumed its reserve before reveal");
    const auto crash_sprites = flight.world().bridge_sprite_state();
    const auto crash_presentation = flight.world().bridge_presentation_state();
    if (expect_joint_retirement) {
        require(!crash_sprites.crossing && crash_presentation.crossing &&
                    (crash_presentation.crossing->image == BridgeSpriteImage::JointExplosionA ||
                     crash_presentation.crossing->image == BridgeSpriteImage::JointExplosionB) &&
                    flight.life_cycle().phase == LifeCyclePhase::Exploding &&
                    flight.life_cycle().cause == LifeEndCause::Collision,
                "Queued joint life-end did not privately retire after publishing its final image");
    } else {
        require(crash_sprites.crossing.has_value(), "Crash fixture lost its crossing sprite");
        require(crash_presentation.crossing.has_value(),
                "Crash fixture lost its published crossing sprite");
    }
    const auto crash_rows = flight.world().generated_rows();
    if (expect_joint_retirement) {
        advance_one_tick(flight);
        require(flight.status() == FlightStatus::Crashed &&
                    !flight.world().bridge_sprite_state().crossing &&
                    !flight.world().bridge_presentation_state().crossing &&
                    flight.world().generated_rows() == crash_rows,
                "Retired joint image remained published past the next crossing stage");
    }
    bool animated = false;
    flight.set_controls({});
    for (int frame = 0; frame < 500 && flight.status() == FlightStatus::Crashed; ++frame) {
        flight.advance_elapsed(10ms);
        if (flight.life_cycle().phase == LifeCyclePhase::Exploding) {
            const auto current = flight.world().bridge_sprite_state();
            const auto current_presentation = flight.world().bridge_presentation_state();
            if (expect_joint_retirement) {
                require(!current.crossing && !current_presentation.crossing &&
                            flight.world().generated_rows() == crash_rows,
                        "Retired joint crossing returned during crash animation");
            } else {
                require(current.crossing &&
                        current.crossing->vic_x == crash_sprites.crossing->vic_x &&
                        current.crossing->vic_y == crash_sprites.crossing->vic_y &&
                        current_presentation.crossing &&
                        current_presentation.crossing->vic_x == crash_presentation.crossing->vic_x &&
                        current_presentation.crossing->vic_y == crash_presentation.crossing->vic_y &&
                        flight.world().generated_rows() == crash_rows,
                        "Crash animation moved the crossing or generated terrain");
                animated |= current.crossing->image != crash_sprites.crossing->image;
            }
        }
    }
    if (!expect_joint_retirement) {
        require(animated, "Crash pause froze the crossing animation");
    }
    require(flight.status() == FlightStatus::Flying && flight.bridge_number() == 2 &&
                flight.score() == score && flight.fuel() == kInitialFuel &&
                flight.life_cycle().phase == LifeCyclePhase::AwaitInput && flight.lives() == 2 &&
                flight.world().generated_rows() == 156,
            "Checkpoint respawn lost bridge, score, or fuel state");
    require(flight.world().river_state().section == 2,
            "Cleared bridge respawn skipped or repeated a course section");
    const auto revealed = flight.world().bridge_sprite_state();
    const auto revealed_presentation = flight.world().bridge_presentation_state();
    require((!revealed.crossing || revealed.crossing->vic_y == 0) &&
                (!revealed.gap || revealed.gap->vic_y == 0) &&
                (!revealed_presentation.crossing ||
                 revealed_presentation.crossing->vic_y == 0) &&
                (!revealed_presentation.gap || revealed_presentation.gap->vic_y == 0),
            "Respawn retained destroyed bridge layers");
}

void collide_with_first_bridge(FlightPreview& flight) {
    drive_first_bridge_route(flight, false);
    for (const auto run : aircraft_bridge_approach) {
        flight.set_controls({false, false, run.direction == 2, run.direction == 1, false});
        for (int frame = 0; frame < run.frames; ++frame) flight.advance_elapsed(20ms);
    }
    require(flight.status() == FlightStatus::Flying && flight.lives() == 3 &&
                flight.bridge_number() == 2 && flight.score() == 440 &&
                flight.world().generated_rows() == 1170,
            "Aircraft bridge contact failed to progress without awarding points");
    flight.set_controls({});
    flight.advance_elapsed(20ms);
    require(flight.status() == FlightStatus::Crashed && flight.lives() == 3 &&
                flight.bridge_number() == 2 && flight.score() == 440 &&
                flight.world().generated_rows() == 1170,
            "Aircraft bridge contact was not deferred into the life-end pass");
    respawn_after_bridge(flight, true);
}

void shoot_joint_first_bridge(FlightPreview& flight) {
    constexpr auto expected = fixtures::joint_bridge::expected;
    drive_first_bridge_route(flight, false);
    int frame = 0;
    for (const auto run : aircraft_bridge_approach) {
        for (int index = 0; index < run.frames; ++index, ++frame) {
            const auto previous_score = flight.score();
            const auto previous_bridge = flight.bridge_number();
            const auto objects_before = flight.world().object_state();
            const auto previously_published_crossing =
                flight.world().bridge_presentation_state().crossing;
            flight.set_controls({false, false, run.direction == 2, run.direction == 1,
                                 frame == 236});
            flight.advance_elapsed(20ms);
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "Joint bridge input route lost an aircraft");
            if (flight.bridge_number() == 1) continue;
            require(frame == 243 && flight.world().generated_rows() == 1144 &&
                        flight.bridge_number() == 2 && flight.score() == 1190 &&
                        flight.score() - previous_score == expected.score_award &&
                        flight.bridge_number() - previous_bridge == expected.bridge_increment,
                    "Natural joint bridge shot lost its single 750-point award");
            const auto crossing = flight.world().bridge_sprite_state().crossing;
            const auto published_crossing = flight.world().bridge_presentation_state().crossing;
            const auto& consumed = flight.last_consumed_contacts();
            ShotContactSnapshot contact{};
            contact.slots = consumed.shot;
            for (std::size_t slot = 0; slot < contact.object_kinds.size(); ++slot)
                contact.object_kinds[slot] = objects_before.records[slot].animated_kind;
            const auto selected = select_shot_contact(contact);
            const bool joint_sample = consumed.shot_presented &&
                selected.outcome == ShotContactOutcome::Bridge && selected.slot && selected.object &&
                consumed.shot[selected.slot->value].bridge_sprite_present &&
                contact.object_kinds[selected.object->index.value] == 14;
            require(joint_sample == expected.same_sample_joint_contact,
                    "Joint bridge participants did not share the selected contact slot");
            const bool published_normal = published_crossing &&
                (published_crossing->image == BridgeSpriteImage::CrossingRightA ||
                 published_crossing->image == BridgeSpriteImage::CrossingRightB ||
                 published_crossing->image == BridgeSpriteImage::CrossingLeftA ||
                 published_crossing->image == BridgeSpriteImage::CrossingLeftB);
            const bool shot_cleared = flight.shot().pose == PlayerShotPose::Hidden &&
                flight.shot().state.vertical_position == 0 && flight.shot().vertical_position_write == 0;
            require(crossing && crossing->image == expected.private_joint_image_after_hit &&
                        previously_published_crossing &&
                        published_normal == expected.published_normal_during_hit &&
                        shot_cleared == expected.projectile_cleared && flight.bridge_flash() &&
                        flight.audio_state().explosion_countdown == 31,
                    "Joint bridge shot lost its deferred image, published contact, projectile, flash, or audio effect");
            advance_one_tick(flight);
            const auto next_crossing = flight.world().bridge_presentation_state().crossing;
            const bool next_joint = next_crossing &&
                (next_crossing->image == BridgeSpriteImage::JointExplosionA ||
                 next_crossing->image == BridgeSpriteImage::JointExplosionB);
            require(next_joint == expected.next_published_joint_family,
                    "Next crossing stage did not publish the joint explosion image");
            const bool life_end_started = flight.status() != FlightStatus::Flying ||
                flight.life_cycle().phase != LifeCyclePhase::Active || flight.life_cycle().cause;
            require(life_end_started == expected.life_end_queued && flight.lives() == 3,
                    "Joint projectile hit queued a life end before the next update");
            return;
        }
    }
    throw std::runtime_error("Natural joint bridge route did not destroy its bridge");
}

} // namespace

int main() {
    try {
        FlightPreview flight;
        reach_first_bridge(flight);
        respawn_after_bridge(flight);
        reach_first_bridge(flight);
        collide_with_first_bridge(flight);
        reach_first_bridge(flight);
        expire_bridge_flash(flight);
        shoot_joint_first_bridge(flight);
        respawn_after_bridge(flight, true);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
