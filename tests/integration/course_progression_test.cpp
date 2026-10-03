#include "app/generator_setup.hpp"
#include "app/audio_setup.hpp"
#include "app/world_setup.hpp"
#include "app/world_scene_setup.hpp"
#include "render/world_object_schedule.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_native_course_progression() {
    auto world = river_raid::make_river_world();
    std::array<std::size_t, 128> transition_rows{};
    std::size_t transition_count = 0;
    std::size_t first_bridge_row = 0;
    river_raid::WorldObjectState first_bridge_state{};
    river_raid::PackedRiverRow first_bridge_pixels{};
    std::uint8_t previous_section = 0;
    bool saw_section_79 = false;
    bool saw_section_80 = false;

    for (std::size_t row = 1; row <= 131072; ++row) {
        const auto before = world.object_state();
        const auto pixels = world.advance();
        const auto& state = world.river_state();
        require(state.phase < 32, "Native phase became unrepresentable");
        require(state.scroll_buffer >= 1 && state.scroll_buffer <= 5,
                "Native scroll buffer became unrepresentable");
        saw_section_79 = saw_section_79 || state.section == 79;
        saw_section_80 = saw_section_80 || state.section == 80;

        if (row == 1) previous_section = state.section;
        if (state.section != previous_section) {
            require(transition_count < transition_rows.size(),
                    "Too many native section transitions");
            transition_rows[transition_count++] = row;
            previous_section = state.section;
        }

        bool has_bridge = false;
        for (const auto& record : before.records) {
            has_bridge = has_bridge || record.animated_kind == 14;
        }
        if (has_bridge && first_bridge_row == 0) {
            first_bridge_row = row;
            first_bridge_state = before;
            first_bridge_pixels = pixels;
        }
    }

    require(transition_count >= 63, "Long native run did not cross section 2");
    require(saw_section_79 && saw_section_80,
            "Long native run did not exercise the generator section clamp");
    require(transition_rows[0] == 1056 && transition_rows[1] == 2080,
            "Native section transition rows changed");
    for (std::size_t index = 1; index < transition_count; ++index) {
        require(transition_rows[index] - transition_rows[index - 1] == 1024,
                "Native section interval changed during long run");
    }

    require(first_bridge_row == 1025, "First native bridge row changed");
    require(first_bridge_state.active_begin == 2 &&
                first_bridge_state.records[2].animated_kind == 14 &&
                first_bridge_state.records[2].base_kind == 14 &&
                first_bridge_state.records[2].vertical_position == 33,
            "First native bridge record changed");
    require(first_bridge_pixels[0] == 0xff && first_bridge_pixels[16] == 0xff,
            "First bridge row was not composed as bridge terrain");

    const auto bands = river_raid::schedule_world_object_snapshot(
        1, first_bridge_state);
    require(!bands.empty() && bands.front().placements.size() == 1,
            "First bridge was not scheduled in the top band");
    const auto& placement = bands.front().placements.front();
    require(placement.record_index == 2 &&
                placement.image_slot == river_raid::WorldObjectImageSlot::alternate &&
                placement.image_key == 14 && placement.style_key == 14,
            "First bridge scheduler mapping changed");

    std::array<river_raid::PackedRiverRow, 157> terrain{};
    const auto frame = river_raid::make_world_scene(terrain, bands.front().placements, 0);
    require(frame.width == 320 && frame.height == 200,
            "First bridge render path changed dimensions");
}

void test_native_course_with_live_motion() {
    auto world = river_raid::make_river_world();
    auto audio = river_raid::make_audio_controller();
    std::uint8_t animation_phase = 0;
    bool saw_crossing = false;
    bool saw_gap = false;
    bool saw_auxiliary = false;
    std::size_t auxiliary_launches = 0;
    for (std::size_t row = 1; row <= 131072; ++row) {
        world.advance_motion({animation_phase, world.river_state().section >= 2,
                              std::nullopt});
        world.advance_bridge_sprites(animation_phase, true);
        ++animation_phase;
        (void)world.advance();
        const auto auxiliary_event = world.advance_auxiliary_projectile(
            true, false, audio.state().auxiliary_sprite_countdown == 0);
        if (auxiliary_event.audio_started) {
            ++auxiliary_launches;
            audio.start_auxiliary_sprite_effect();
        }
        (void)audio.advance({});
        const auto& auxiliary = world.auxiliary_projectile_state().presentation;
        if (auxiliary) {
            require(auxiliary->vic_x < 512, "Auxiliary projectile exceeded VIC X range");
            saw_auxiliary |= auxiliary->vic_x > 0 && auxiliary->vic_x < 344;
        }
        const auto& state = world.river_state();
        require(state.phase < 32, "Live motion made native phase unrepresentable");
        require(state.scroll_buffer >= 1 && state.scroll_buffer <= 5,
                "Live motion made native scroll buffer unrepresentable");
        const auto sprites = world.bridge_sprite_state();
        saw_crossing |= sprites.crossing.has_value();
        saw_gap |= sprites.gap.has_value();
        for (const auto& sprite : {sprites.crossing, sprites.gap}) {
            if (!sprite) continue;
            require(sprite->vic_x < 512 &&
                    sprite->image <= river_raid::BridgeSpriteImage::JointExplosionB,
                    "Special sprite left its coordinate or image range");
        }
    }
    require(saw_crossing && saw_gap,
            "Long native motion did not exercise crossing and automatic gap sprites");
    require(saw_auxiliary && auxiliary_launches > 0,
            "Long native motion missed the auxiliary producer");
}

} // namespace

int main() {
    try {
        test_native_course_progression();
        test_native_course_with_live_motion();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
