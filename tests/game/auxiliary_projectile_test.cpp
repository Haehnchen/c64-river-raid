#include "game/auxiliary_projectile.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using namespace river_raid;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

WorldObjectState eligible_objects(bool moving_left = false) {
    WorldObjectState objects{};
    objects.special_cursor = 4;
    objects.records[4].horizontal_coordinate = 0x40;
    objects.records[4].behavior_flags = moving_left ? 0x08 : 0x00;
    objects.records[4].animated_kind = 10;
    return objects;
}

void scroll_to(AuxiliaryProjectileSystem& projectile, std::uint8_t position) {
    for (std::uint16_t index = 0; index < position; ++index) {
        projectile.advance_after_row();
    }
}

void test_row_order_and_spawn_seed() {
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 7);
    projectile.advance_after_row(AuxiliaryProjectileSpawn{0});
    require(projectile.state().horizontal_coordinate == 0 &&
                projectile.state().vertical_position == 0x29 &&
                !projectile.state().presentation,
            "Phase-zero selection did not override the row increment");

    projectile.advance_after_row();
    require(projectile.state().vertical_position == 0x2a,
            "Unselected row did not increment the independent Y byte");
    projectile.advance_after_row(AuxiliaryProjectileSpawn{0x10});
    require(projectile.state().vertical_position == 0x28,
            "Nonzero-phase selection did not use its source Y seed");
}

void test_eligibility_gates_and_launch() {
    auto objects = eligible_objects();
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 0x3a);

    auto tick = AuxiliaryProjectileTick{objects};
    require(projectile.advance_tick(tick) == AuxiliaryProjectileTickResult{} &&
                !projectile.state().presentation,
            "Projectile launched below the source visibility threshold");

    projectile.advance_after_row();
    tick.lifecycle_active = false;
    require(projectile.advance_tick(tick) == AuxiliaryProjectileTickResult{},
            "Inactive lifecycle launched a normal-round projectile");

    tick.demo_mode = true;
    const auto launched = projectile.advance_tick(tick);
    require(launched == AuxiliaryProjectileTickResult{true, false} &&
                projectile.state().horizontal_coordinate == 0x48 &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{0x90, 0x3b},
            "Demo bypass or right launch offset differs");

    objects.special_cursor = 12;
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}) ==
                AuxiliaryProjectileTickResult{} &&
                projectile.state().horizontal_coordinate == 0x48 &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{0, 0x3b},
            "Ineligible cursor did not publish clipped X while retaining state");

    objects = eligible_objects();
    objects.records[4].animated_kind = 12;
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}) ==
                AuxiliaryProjectileTickResult{} &&
                projectile.state().horizontal_coordinate == 0x48 &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{0, 0x3b},
            "Wrong object kind changed retained projectile state");
}

void test_audio_gate_directions_and_wrap() {
    auto objects = eligible_objects(true);
    objects.records[4].horizontal_coordinate = 4;
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 0x3b);

    auto tick = AuxiliaryProjectileTick{objects};
    tick.audio_effect_idle = false;
    require(projectile.advance_tick(tick) == AuxiliaryProjectileTickResult{} &&
                projectile.state().vertical_position == 0x3b &&
                !projectile.state().presentation,
            "Busy auxiliary audio did not suppress a new launch");

    tick.audio_effect_idle = true;
    require(projectile.advance_tick(tick).audio_started &&
                projectile.state().horizontal_coordinate == 0xfc &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{0x1f8, 0x3b},
            "Left launch did not wrap by eight half-pixels");
    require(!projectile.advance_tick(tick).audio_started &&
                projectile.state().horizontal_coordinate == 0xfa &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{0x1f4, 0x3b},
            "Active left projectile did not move by two half-pixels");

    objects.records[4].behavior_flags = 0;
    objects.records[4].horizontal_coordinate = 0xf9;
    projectile.reset();
    scroll_to(projectile, 0x3b);
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started &&
                projectile.state().horizontal_coordinate == 1 &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{2, 0x3b},
            "Right launch did not preserve byte wrap");
}

void test_contacts_and_player_fatality() {
    auto objects = eligible_objects();
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 0x3b);
    auto tick = AuxiliaryProjectileTick{objects};
    require(projectile.advance_tick(tick).audio_started,
            "Contact setup did not launch the projectile");

    tick.player_contact_samples = {0x08, 0x01};
    require(!projectile.advance_tick(tick).fatal_player_contact,
            "Separate participant samples became a fatal contact");
    tick.player_contact_samples = {0x09, 0x00};
    require(projectile.advance_tick(tick).fatal_player_contact,
            "Joined sprite-3/player sample did not become fatal");

    tick.player_contact_samples = {};
    tick.terrain_contact_samples = {0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
    const auto cleared = projectile.advance_tick(tick);
    require(cleared == AuxiliaryProjectileTickResult{} &&
                projectile.state().horizontal_coordinate == 0 &&
                !projectile.state().presentation,
            "Terrain participant did not clear and hide the projectile");

    tick.terrain_contact_samples = {};
    tick.audio_effect_idle = false;
    tick.player_contact_samples = {0x09, 0};
    require(projectile.advance_tick(tick) == AuxiliaryProjectileTickResult{} &&
                !projectile.state().presentation,
            "Audio-gated inactive projectile consumed player contacts");
}

void test_retirement_crash_and_reset() {
    auto objects = eligible_objects();
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 0xd2);
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started,
            "Retirement setup did not launch at the last accepted Y");
    projectile.advance_after_row();
    const auto retired = projectile.advance_tick(AuxiliaryProjectileTick{objects});
    require(retired == AuxiliaryProjectileTickResult{} &&
                projectile.state().horizontal_coordinate == 0 &&
                projectile.state().vertical_position == 0 &&
                !projectile.state().presentation,
            "Y boundary did not retire and clear both coordinates");

    scroll_to(projectile, 0x3b);
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started,
            "Reveal setup did not relaunch");
    const auto horizontal = projectile.state().horizontal_coordinate;
    projectile.hide_for_life_reveal();
    require(projectile.state().horizontal_coordinate == horizontal &&
                projectile.state().vertical_position == 0 &&
                projectile.state().presentation == AuxiliaryProjectilePresentation{
                                                       static_cast<std::uint16_t>(horizontal * 2U), 0},
            "Reveal boundary cleared more than stored/published Y");

    projectile.reset();
    require(projectile.state() == AuxiliaryProjectileState{},
            "Reset retained auxiliary projectile state");
}

void test_zero_coordinate_hides_after_launch_or_motion() {
    auto objects = eligible_objects();
    objects.records[4].horizontal_coordinate = 0xf8;
    AuxiliaryProjectileSystem projectile;
    scroll_to(projectile, 0x3b);
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started &&
                projectile.state().horizontal_coordinate == 0 &&
                !projectile.state().presentation,
            "Zero launch coordinate did not retain the source inactive sentinel");

    objects.records[4].horizontal_coordinate = 0xf6;
    projectile.reset();
    scroll_to(projectile, 0x3b);
    require(projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started,
            "Zero-motion setup did not launch");
    require(!projectile.advance_tick(AuxiliaryProjectileTick{objects}).audio_started &&
                projectile.state().horizontal_coordinate == 0 &&
                !projectile.state().presentation,
            "Motion into the zero sentinel did not hide the image");
}

} // namespace

int main() {
    try {
        test_row_order_and_spawn_seed();
        test_eligibility_gates_and_launch();
        test_audio_gate_directions_and_wrap();
        test_contacts_and_player_fatality();
        test_retirement_crash_and_reset();
        test_zero_coordinate_hides_after_launch_or_motion();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
