#include "app/audio_setup.hpp"
#include "app/auxiliary_contacts.hpp"
#include "app/world_setup.hpp"

#include <cstdint>
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
        auto world = make_river_world();
        auto audio = make_audio_controller();
        PlayerSteeringState absent;
        absent.horizontal_position = 255;
        std::uint8_t phase = 0;
        bool launched = false, collided = false;
        constexpr std::size_t contact_row = 12787;
        for (std::size_t row = 1; row <= contact_row; ++row) {
            world.advance_motion({phase++, world.river_state().section >= 2, std::nullopt});
            (void)world.advance();
            const auto contacts = sample_auxiliary_projectile_contacts(world, absent, FlightPose::Straight);
            const auto event = world.advance_auxiliary_projectile(true, false,
                audio.state().auxiliary_sprite_countdown == 0, contacts.terrain, contacts.player);
            if (event.audio_started) {
                launched = true;
                require(world.river_state().section >= 13,
                        "Generated auxiliary shot started before section thirteen");
                audio.start_auxiliary_sprite_effect();
            }
            const auto sound = audio.advance({});
            if (event.audio_started) {
                require(audio.state().auxiliary_sprite_countdown == 7 &&
                        sound.voices[2].control == AudioVoiceControl::NoiseGate,
                        "Generated launch did not produce its eight-update sound");
            }
            if (row != contact_row) continue;
            PlayerSteeringState player;
            player.horizontal_position = 49;
            const auto sampled = sample_auxiliary_projectile_contacts(world, player, FlightPose::Straight);
            bool terrain_contact = false;
            for (const auto participants : sampled.terrain) {
                terrain_contact |= (participants & 0x08U) != 0;
            }
            bool player_contact = false;
            for (const auto participants : sampled.player) {
                player_contact |= (participants & 0x09U) == 0x09U;
            }
            require(!terrain_contact && player_contact,
                    "Generated contact fixture lost its lower aircraft/auxiliary latch");
            const auto hit = world.advance_auxiliary_projectile(true, false,
                audio.state().auxiliary_sprite_countdown == 0, sampled.terrain, sampled.player);
            require(hit.fatal_player_contact, "Sampled generated projectile did not hit the player");
            collided = true;
        }
        require(launched && collided, "Generated world missed auxiliary launch/player contact");
        const auto retained = world.auxiliary_projectile_state();
        (void)world.restore_checkpoint();
        require(world.auxiliary_projectile_state() == retained,
                "Partial checkpoint restore cleared retained auxiliary state");
        world.hide_life_transients();
        require(world.auxiliary_projectile_state().vertical_position == 0 &&
                    world.auxiliary_projectile_state().horizontal_coordinate == retained.horizontal_coordinate,
                "Aircraft reveal did not clear only auxiliary Y");
        world.reset();
        require(world.auxiliary_projectile_state() == AuxiliaryProjectileState{},
                "New world retained auxiliary projectile");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
