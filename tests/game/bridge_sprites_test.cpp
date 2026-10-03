#include "game/bridge_sprites.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using namespace river_raid;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

RiverGeneratorState regular_river() {
    RiverGeneratorState river{};
    river.phase = 0;
    river.section = 8;
    river.course = 32;
    river.random = {0, 0, 0};
    river.scripted_course = false;
    river.channel_mode = RiverChannelMode::Single;
    river.target_width_units = 7;
    river.previous_target_width_units = 7;
    river.section_interval = 5;
    return river;
}

WorldObjectState bridge_object_source() {
    WorldObjectState objects{};
    objects.generator_cursor = 3;
    objects.records[3].animated_kind = 8;
    return objects;
}

void test_scripted_spawn_and_scroll() {
    auto river = regular_river();
    river.scripted_course = true;
    river.random.low = 0x20;
    const auto objects = bridge_object_source();

    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Scripted right crossing did not spawn");
    const auto first = sprites.state();
    require(first.crossing == BridgeSpritePresentation{
                                  BridgeSpriteImage::CrossingRightA, 72, 0x21, 0},
            "Scripted right crossing placement differs");
    require(first.gap_animation_counter == 0xff && !first.gap,
            "Scripted spawn armed a gap animation");
    require(!first.automatic_entry_enabled &&
                !sprites.presentation_state().automatic_entry_enabled,
            "Scripted crossing advertised automatic entry");

    river.phase = 1;
    require(!sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Active crossing spawned a duplicate");
    require(sprites.state().crossing->vic_y == 0x22,
            "Generated row did not scroll the crossing once");

    sprites.advance_tick({0, true, false});
    require(sprites.state().crossing == BridgeSpritePresentation{
                                             BridgeSpriteImage::CrossingRightB, 74, 0x22, 0},
            "Scripted crossing did not move and animate on its four-tick cadence");

    sprites.reset();
    river.phase = 0;
    river.random.low = 0x50;
    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Scripted left crossing did not spawn");
    require(sprites.state().crossing == BridgeSpritePresentation{
                                             BridgeSpriteImage::CrossingLeftA, 228, 0x21, 0},
            "Scripted left crossing placement differs");
}

void test_regular_spawn_guards_and_placement() {
    auto river = regular_river();
    auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {100, 140, 0, 0}}),
            "Eligible regular crossing did not spawn");
    require(sprites.state().automatic_entry_enabled &&
                sprites.presentation_state().automatic_entry_enabled &&
                !sprites.presentation_state().crossing,
            "Regular entry rule did not expose live mechanics before publication");
    require(sprites.state().crossing == BridgeSpritePresentation{
                                             BridgeSpriteImage::CrossingRightA, 48, 0x21, 0},
            "Regular even-section placement differs");

    const auto rejected = [&](RiverGeneratorState rejected_river,
                              WorldObjectState rejected_objects,
                              std::string_view message) {
        BridgeSpriteSystem candidate;
        require(!candidate.advance_after_row(
                    {rejected_river, rejected_objects, {100, 140, 0, 0}}) &&
                    !candidate.state().crossing,
                message);
    };

    auto changed = river;
    changed.section_interval = 4;
    rejected(changed, objects, "Interval below five spawned a crossing");
    changed = river;
    changed.section_interval = 14;
    rejected(changed, objects, "Interval fourteen spawned a crossing");
    changed = river;
    changed.random.high = 0x40;
    rejected(changed, objects, "High random selector bits spawned a crossing");
    changed = river;
    changed.target_width_units = 4;
    rejected(changed, objects, "Width below five spawned a crossing");
    changed = river;
    changed.channel_mode = RiverChannelMode::SplitOpening;
    rejected(changed, objects, "Split channel spawned a regular crossing");
    objects.records[3].animated_kind = 7;
    rejected(river, objects, "Object kind below eight spawned a crossing");
    objects = bridge_object_source();
    objects.generator_cursor = kWorldObjectCapacity;
    rejected(river, objects, "Missing generator object spawned a crossing");
    objects.records.front().base_kind = 8;
    BridgeSpriteSystem retained_kind;
    require(retained_kind.advance_after_row({river, objects, {100, 140, 0, 0}}),
            "One-past producer cursor lost the oldest retained base kind");

    changed = river;
    changed.section = 7;
    changed.random.high = 0x18;
    changed.bank_inset_step = 2;
    BridgeSpriteSystem odd_left;
    require(odd_left.advance_after_row({changed, bridge_object_source(), {100, 100, 0, 0}}),
            "Odd-section inactive-feature branch did not spawn");
    require(odd_left.state().crossing == BridgeSpritePresentation{
                                              BridgeSpriteImage::CrossingLeftA, 280, 0x21, 0},
            "Odd-section inactive-feature branch differs");

    changed.bank_inset_step = -2;
    changed.feature.active = true;
    changed.feature.side = RiverBankSide::Right;
    BridgeSpriteSystem odd_right;
    require(odd_right.advance_after_row({changed, bridge_object_source(), {100, 100, 0, 0}}),
            "Odd-section right-feature branch did not spawn");
    require(odd_right.state().crossing == BridgeSpritePresentation{
                                               BridgeSpriteImage::CrossingRightA, 72, 0x21, 0},
            "Odd-section right-feature placement differs");

    changed.random.high = 0x08;
    changed.bank_inset_step = 2;
    changed.feature.side = RiverBankSide::Left;
    BridgeSpriteSystem odd_low_left;
    require(odd_low_left.advance_after_row(
                {changed, bridge_object_source(), {100, 100, 0, 0}}),
            "Odd-section low left-feature branch did not spawn");
    require(odd_low_left.state().crossing == BridgeSpritePresentation{
                                                  BridgeSpriteImage::CrossingLeftA, 280, 0x21, 0},
            "Odd-section low left-feature placement differs");

    changed = river;
    changed.bank_inset_step = 2;
    changed.feature.active = true;
    changed.feature.side = RiverBankSide::Left;
    BridgeSpriteSystem even_low_left;
    require(even_low_left.advance_after_row(
                {changed, bridge_object_source(), {100, 100, 0, 0}}),
            "Even-section low left-feature branch did not spawn");
    require(even_low_left.state().crossing == BridgeSpritePresentation{
                                                   BridgeSpriteImage::CrossingLeftA, 304, 0x21, 0},
            "Even-section low left-feature placement differs");

    // Placement signature sampled at native Game 5 row 1376. Other fields
    // remain this unit fixture's eligible values, not a natural-session replay.
    changed = river;
    changed.section = 21;
    changed.course = 8;
    changed.random.high = 43;
    changed.feature.active = true;
    changed.feature.side = RiverBankSide::Right;
    BridgeSpriteSystem outside;
    require(!outside.advance_after_row({changed, bridge_object_source(), {30, 100, 0, 0}}) &&
                !outside.state().crossing && outside.state().target_horizontal_coordinate == 16,
            "Horizontal coordinate 15 bypassed the lower visibility bound");
    changed.course = 9;
    BridgeSpriteSystem at_edge;
    require(at_edge.advance_after_row({changed, bridge_object_source(), {30, 100, 0, 0}}) &&
                at_edge.state().crossing->vic_x == 32 && at_edge.state().automatic_entry_enabled,
            "Horizontal coordinate 16 was incorrectly excluded from regular spawning");
}

void test_gap_trigger_and_animation_boundaries() {
    auto river = regular_river();
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Regular crossing did not spawn for gap test");

    river.phase = 1;
    for (int row = 0; row < 26; ++row) {
        (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    }
    require(sprites.state().crossing->vic_y == 0x3b,
            "Gap entry row was not reached exactly");
    sprites.advance_tick({0, true, false});
    require(sprites.state().gap_animation_counter == 0 && !sprites.state().gap,
            "Entry trigger did not wait for the visible-row boundary");

    (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    sprites.advance_tick({1, true, false});
    require(sprites.state().gap == BridgeSpritePresentation{
                                      BridgeSpriteImage::GapFlight, 62, 0x38, 1} &&
                sprites.state().gap_animation_counter == 1,
            "Gap flight did not start four rows above and seven units to the right");

    sprites.advance_tick({2, true, false});
    require(sprites.state().gap == BridgeSpritePresentation{
                                      BridgeSpriteImage::GapFlight, 70, 0x37, 1},
            "Gap trajectory did not apply its first vertical and horizontal step");

    for (int tick = 0; tick < 15; ++tick) sprites.advance_tick({3, true, false});
    require(sprites.state().gap_animation_counter == 17 &&
                sprites.state().gap->image == BridgeSpriteImage::GapFlight,
            "Gap flight duration differs at the explosion boundary");
    sprites.advance_tick({3, true, false});
    require(sprites.state().gap_animation_counter == 18 &&
                sprites.state().gap->image == BridgeSpriteImage::GapExplosion0,
            "First gap explosion frame began at the wrong counter");
    for (int tick = 0; tick < 7; ++tick) sprites.advance_tick({3, true, false});
    require(sprites.state().gap_animation_counter == 25 &&
                sprites.state().gap->image == BridgeSpriteImage::GapExplosion0,
            "First gap explosion frame did not last eight updates");
    sprites.advance_tick({3, true, false});
    require(sprites.state().gap->image == BridgeSpriteImage::GapExplosion1,
            "Second gap explosion frame began late");
}

void test_demo_crossing_and_active_only_gap_completion() {
    auto river = regular_river();
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Demo crossing fixture did not spawn");
    const auto initial_x = sprites.state().crossing->vic_x;
    auto inactive = sprites;
    inactive.advance_crossing({0, false, false, false});
    sprites.advance_crossing({0, false, false, true});
    require(inactive.state().crossing->vic_x == initial_x &&
                sprites.state().crossing->vic_x == initial_x + 2,
            "Crossing did not distinguish demo from inactive non-demo motion");
    auto progressed = sprites;
    progressed.advance_crossing({4, false, true, true});
    require(progressed.state().crossing->vic_x == sprites.state().crossing->vic_x,
            "Demo bypassed the bridge-progression motion gate");

    river.phase = 1;
    for (int row = 0; row < 26; ++row)
        (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    inactive = sprites;
    inactive.advance_crossing({4, false, false, false});
    sprites.advance_crossing({4, false, false, true});
    require(inactive.state().gap_animation_counter == 0xff &&
                sprites.state().gap_animation_counter == 0,
            "Demo crossing failed to arm the regular gap at its entry row");
    (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    sprites.advance_gap({1, false, false, true});
    require(sprites.state().gap && sprites.state().gap_animation_counter == 1,
            "Demo gap did not publish after the visible-row boundary");
    auto active = sprites;
    for (int tick = 0; tick < 55; ++tick) {
        sprites.advance_gap({1, false, false, true});
        active.advance_gap({1, true, false, false});
    }
    require(sprites.state().gap_animation_counter == 56 &&
                active.state().gap_animation_counter == 56,
            "Gap completion fixture missed its final published frame");
    sprites.advance_gap({1, false, false, true});
    active.advance_gap({1, true, false, false});
    require(!sprites.state().gap && sprites.state().gap_animation_counter == 0xff &&
                !active.state().gap && active.state().gap_animation_counter == 0,
            "Demo incorrectly reused the active-aircraft gap completion rule");
}

void test_destruction_gate_and_joint_effect() {
    auto river = regular_river();
    const auto objects = bridge_object_source();

    BridgeSpriteSystem near;
    require(near.advance_after_row({river, objects, {78, 120, 0, 0}}),
            "Near crossing did not spawn");
    require(near.bridge_destroyed({false, 4}) == BridgeSpriteDestructionResult{} &&
                near.state().gap_animation_counter == 0xff,
            "Displayed bridge below five entered the proximity branch");
    require(near.bridge_destroyed({false, 5}) ==
                BridgeSpriteDestructionResult{false, true} &&
                near.state().gap_animation_counter == 0,
            "Displayed bridge five did not enter the proximity branch");
    require(near.state().automatic_entry_enabled,
            "Destruction changed the crossing rule instead of just its gap counter");

    BridgeSpriteSystem boundary;
    require(boundary.advance_after_row({river, objects, {79, 120, 0, 0}}),
            "Boundary crossing did not spawn");
    require(boundary.bridge_destroyed({false, 5}) == BridgeSpriteDestructionResult{},
            "Right-facing proximity accepted the excluded distance 41");

    const auto before_x = near.state().crossing->vic_x;
    require(near.bridge_destroyed({true, 6}) ==
                BridgeSpriteDestructionResult{true, false},
            "Joint contact did not select its visual effect");
    require(near.state().crossing->image == BridgeSpriteImage::JointExplosionA,
            "Joint contact did not select the first explosion image");
    require(!near.state().automatic_entry_enabled &&
                !near.presentation_state().automatic_entry_enabled,
            "Joint explosion retained a normal crossing entry rule");
    near.advance_tick({0, true, true});
    require(near.state().crossing->image == BridgeSpriteImage::JointExplosionB &&
                near.state().crossing->vic_x == before_x,
            "Joint effect did not use the sixteen-tick cadence or progression motion gate");
    near.advance_tick({1, true, true});
    require(near.state().crossing->image == BridgeSpriteImage::JointExplosionB,
            "Joint effect toggled outside its sixteen-tick cadence");
}

void test_retirement_and_reset() {
    auto river = regular_river();
    river.scripted_course = true;
    river.random.low = 0x20;
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Crossing did not spawn for retirement test");
    river.phase = 1;
    for (int row = 0; row < 0xad; ++row) {
        (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    }
    require(sprites.state().crossing->vic_y == 0xce,
            "Retirement vertical boundary was not reached");
    sprites.advance_tick({1, true, false});
    require(!sprites.state().crossing && !sprites.state().gap &&
                sprites.state().gap_animation_counter == 0xff &&
                !sprites.state().automatic_entry_enabled,
            "Crossing did not retire at vertical position 206");

    sprites.reset();
    require(sprites.state() == BridgeSpriteState{},
            "Reset retained special-sprite presentation state");
}

void test_repeated_proximity_hits_retire_before_frame_lookup() {
    auto river = regular_river();
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {78, 120, 0, 0}}),
            "Crossing did not spawn for repeated-hit retirement test");

    for (int hit = 0; hit < 58; ++hit) {
        require(sprites.bridge_destroyed({false, 5}).gap_triggered,
                "Eligible repeated proximity hit did not advance the counter");
    }
    require(sprites.state().gap_animation_counter == 57,
            "Repeated proximity hits did not preserve byte counter increments");
    sprites.advance_tick({1, true, true});
    require(!sprites.state().gap && sprites.state().gap_animation_counter == 0,
            "Past-final gap counter indexed beyond the semantic frame inventory");
}

void test_contact_between_crossing_and_gap_stages() {
    auto river = regular_river();
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;
    require(sprites.advance_after_row({river, objects, {78, 120, 0, 0}}),
            "Staged bridge fixture did not spawn");
    river.phase = 1;
    for (int row = 0; row < 27; ++row) {
        (void)sprites.advance_after_row({river, objects, {78, 120, 0, 0}});
    }
    sprites.advance_crossing({1, true, false});
    require(!sprites.state().gap && sprites.state().gap_animation_counter == 0xff,
            "Crossing stage published an untriggered gap");
    require(sprites.bridge_destroyed({false, 5}).gap_triggered,
            "Between-stage hit did not trigger the gap");
    sprites.advance_gap({1, true, true});
    require(sprites.state().gap && sprites.state().gap_animation_counter == 1,
            "Gap stage missed the current pass's contact mutation");

    auto combined = sprites;
    combined.advance_tick({4, true, true});
    sprites.advance_crossing({4, true, true});
    require(sprites.state().gap_animation_counter == 1,
            "Crossing stage advanced the gap animation");
    sprites.advance_gap({4, true, true});
    require(sprites.state() == combined.state(),
            "Combined sprite tick diverged from its two shared stages");
}

void test_source_stage_publication_boundaries() {
    auto river = regular_river();
    const auto objects = bridge_object_source();
    BridgeSpriteSystem sprites;

    require(sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Publication fixture crossing did not spawn");
    const auto spawned_crossing = sprites.state().crossing;
    require(spawned_crossing && !sprites.presentation_state().crossing,
            "New crossing became visible before its crossing publication stage");

    // A non-cadence crossing update publishes the staged placement without
    // advancing its horizontal animation.
    sprites.advance_crossing({1, true, false});
    const auto first_published = sprites.presentation_state().crossing;
    require(first_published == spawned_crossing,
            "Crossing update did not publish the spawned placement");

    sprites.retire_crossing_for_pending_life_end();
    require(sprites.state().crossing == spawned_crossing &&
                sprites.presentation_state().crossing == first_published,
            "Pending life-end retirement cleared an ordinary crossing");

    river.phase = 1;
    require(!sprites.advance_after_row({river, objects, {50, 100, 0, 0}}),
            "Active crossing spawned again during publication test");
    require(sprites.state().crossing && sprites.state().crossing->vic_y == 0x22 &&
                sprites.presentation_state().crossing == first_published,
            "Generated-row Y mutation changed already published crossing geometry");
    sprites.advance_crossing({1, true, false});
    require(sprites.presentation_state().crossing &&
                sprites.presentation_state().crossing->vic_y == 0x22,
            "Next crossing stage did not publish the row-updated Y");

    const auto published_before_joint = sprites.presentation_state().crossing;
    require(sprites.bridge_destroyed({true, 6}).joint_explosion &&
                sprites.state().crossing->image == BridgeSpriteImage::JointExplosionA &&
                sprites.presentation_state().crossing == published_before_joint,
            "Joint hit changed the visible crossing image before its next update");
    sprites.advance_crossing({1, true, false});
    require(sprites.presentation_state().crossing &&
                sprites.presentation_state().crossing->image ==
                    BridgeSpriteImage::JointExplosionA,
            "Next crossing stage did not publish the joint-hit image");

    // Cross into the gap entry region through normal row/crossing stages. The
    // crossing is published before the gap contact/update stage.
    for (int row = 0; row < 0x3b - 0x22; ++row) {
        sprites.advance_crossing({1, true, false});
        (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    }
    sprites.advance_crossing({0, true, false});
    require(sprites.presentation_state().crossing &&
                sprites.presentation_state().crossing->vic_y == 0x3b &&
                !sprites.presentation_state().gap,
            "Crossing publication exposed a gap before the gap stage");
    (void)sprites.advance_after_row({river, objects, {50, 100, 0, 0}});
    sprites.advance_crossing({1, true, false});
    require(sprites.presentation_state().crossing &&
                sprites.presentation_state().crossing->vic_y == 0x3c &&
                !sprites.presentation_state().gap,
            "Next crossing stage did not publish row Y before gap publication");
    sprites.advance_gap({1, true, false});
    require(sprites.state().gap && sprites.presentation_state().gap == sprites.state().gap,
            "Gap stage did not publish its own newly visible sprite");

    auto pending_retirement = sprites;
    const auto joint_presentation = pending_retirement.presentation_state();
    const auto joint_mechanics = pending_retirement.state();
    require(joint_mechanics.crossing && joint_presentation.crossing &&
                (joint_presentation.crossing->image == BridgeSpriteImage::JointExplosionA ||
                 joint_presentation.crossing->image == BridgeSpriteImage::JointExplosionB),
            "Pending life-end fixture did not start from a published joint image");
    pending_retirement.retire_crossing_for_pending_life_end();
    const auto retired_mechanics = pending_retirement.state();
    const auto after_private_retirement = pending_retirement.presentation_state();
    require(!retired_mechanics.crossing &&
                retired_mechanics.gap == joint_mechanics.gap &&
                retired_mechanics.gap_animation_counter == joint_mechanics.gap_animation_counter &&
                after_private_retirement.crossing == joint_presentation.crossing &&
                after_private_retirement.gap == joint_presentation.gap,
            "Pending life-end did not retire only the private joint crossing state");
    pending_retirement.advance_gap({1, true, false});
    const auto after_gap_publication = pending_retirement.presentation_state();
    require(!pending_retirement.state().gap && !after_gap_publication.gap &&
                after_gap_publication.crossing == joint_presentation.crossing &&
                pending_retirement.state().gap_animation_counter ==
                    joint_mechanics.gap_animation_counter,
            "Gap retirement did not occur at its own stage while preserving crossing and counter");
    pending_retirement.advance_crossing({1, true, false});
    require(!pending_retirement.presentation_state().crossing &&
                !pending_retirement.presentation_state().gap,
            "Crossing publication did not clear the retired joint image on the next pass");

    auto retiring = sprites;
    const auto published_gap_before_retirement = retiring.presentation_state().gap;
    const auto rows_to_retire = 0xce - retiring.state().crossing->vic_y;
    for (int row = 0; row < rows_to_retire; ++row) {
        (void)retiring.advance_after_row({river, objects, {50, 100, 0, 0}});
    }
    retiring.advance_crossing({1, true, false});
    require(!retiring.state().crossing &&
                !retiring.presentation_state().crossing &&
                retiring.presentation_state().gap == published_gap_before_retirement,
            "Crossing retirement cleared the independently published gap too early");
    retiring.advance_gap({1, true, false});
    require(!retiring.presentation_state().gap,
            "Gap stage did not clear the retired gap publication");

    const auto published_before_reveal = sprites.presentation_state();
    sprites.hide_for_life_reveal();
    const auto revealed = sprites.presentation_state();
    require(published_before_reveal.crossing && revealed.crossing &&
                revealed.crossing->vic_x == published_before_reveal.crossing->vic_x &&
                revealed.crossing->image == published_before_reveal.crossing->image &&
                revealed.crossing->vic_y == 0 &&
                published_before_reveal.gap && revealed.gap &&
                revealed.gap->vic_x == published_before_reveal.gap->vic_x &&
                revealed.gap->image == published_before_reveal.gap->image &&
                revealed.gap->vic_y == 0,
            "Life reveal did not preserve published sprite identity while hiding Y");
    sprites.reset();
    require(sprites.presentation_state() == BridgeSpriteState{},
            "Reset retained a published crossing or gap sprite");
}

} // namespace

int main() {
    try {
        test_scripted_spawn_and_scroll();
        test_regular_spawn_guards_and_placement();
        test_gap_trigger_and_animation_boundaries();
        test_demo_crossing_and_active_only_gap_completion();
        test_destruction_gate_and_joint_effect();
        test_retirement_and_reset();
        test_repeated_proximity_hits_retire_before_frame_lookup();
        test_contact_between_crossing_and_gap_stages();
        test_source_stage_publication_boundaries();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
