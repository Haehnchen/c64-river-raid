#include "app/audio_setup.hpp"
#include "game/bridge_sprites.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace river_raid;
    try {
        RiverGeneratorState river{};
        river.section = 8;
        river.course = 32;
        river.target_width_units = river.previous_target_width_units = 7;
        river.section_interval = 5;
        WorldObjectState objects{};
        objects.generator_cursor = 3;
        objects.records[3].animated_kind = 8;
        BridgeSpriteSystem sprites;
        const RiverGuides guides{100, 140, 0, 0};
        require(sprites.advance_after_row({river, objects, guides}), "Regular spawn failed");
        river.phase = 1;
        for (int row = 0; row < 27; ++row) {
            (void)sprites.advance_after_row({river, objects, guides});
        }
        sprites.advance_tick({0, true, false});
        require(sprites.state().gap && sprites.state().gap_animation_counter == 1,
                "Regular crossing did not launch its gap sprite");
        const auto crossing = *sprites.state().crossing;
        auto audio = make_audio_controller();
        AudioControlTick sound;
        sound.flight_active = false;
        sound.timer_entropy = 5;
        bool heard_flight = false, heard_explosion = false, saw_explosion = false;
        for (std::uint8_t phase = 1; phase <= 60; ++phase) {
            sprites.advance_tick({phase, false, false});
            const auto state = sprites.state();
            sound.special_sprite_counter = state.gap_animation_counter;
            const auto output = audio.advance(sound);
            require(state.crossing && state.crossing->vic_x == crossing.vic_x &&
                    state.crossing->vic_y == crossing.vic_y,
                    "Inactive round moved the crossing");
            if (state.gap_animation_counter >= 1 && state.gap_animation_counter <= 16) {
                require(output.voices[2].control == AudioVoiceControl::TriangleGate,
                        "Crash muted the gap flight tone");
                heard_flight = true;
            } else if (state.gap_animation_counter != 0xff) {
                require(output.voices[2].control == AudioVoiceControl::NoiseGate &&
                        output.voices[2].frequency == 0x0500,
                        "Crash muted the gap explosion");
                heard_explosion = true;
                saw_explosion |= state.gap && state.gap->image != BridgeSpriteImage::GapFlight;
            } else {
                require(!state.gap && output.voices[2].control == AudioVoiceControl::Silent,
                        "Retired gap left graphics or audio active");
            }
        }
        require(heard_flight && heard_explosion && saw_explosion &&
                sprites.state().gap_animation_counter == 0xff && !sprites.state().gap,
                "Inactive-round gap cycle did not complete once");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
