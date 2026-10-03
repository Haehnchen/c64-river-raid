#include "game/player_shot.hpp"
#include "fixtures/player_shot_cases.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace river_raid;
    try {
        std::array<std::array<bool, 256>, 2> covered{};
        unsigned sentinels = 0, index = 0;
        std::uint64_t fingerprint = 14695981039346656037ULL;
        const auto hash_byte = [&](std::uint8_t byte) {
            fingerprint = (fingerprint ^ byte) * 1099511628211ULL;
        };
        const auto hash_state = [&](const test_data::PlayerShotObservation& state) {
            for (const auto byte : {state.vertical_position, state.fire, state.display_y,
                                    state.color_index, state.sound_countdown}) hash_byte(byte);
            for (const auto pointer : state.pointers) hash_byte(pointer);
        };
        for (const auto& sample : test_data::player_shot_cases) {
            const auto& before = sample.before;
            const auto& after = sample.after;
            const auto result = update_player_shot({before.vertical_position}, sample.fire_pressed);
            const auto color = result.start_event ? result.start_event->color_index : before.color_index;
            const auto countdown = result.start_event ? result.start_event->sound_countdown : before.sound_countdown;
            const std::uint8_t pointer = result.pose == PlayerShotPose::Visible ? 0xD5 : 0x40;
            if (sample.index != index++ || sample.fire_pressed != (before.fire == 0) ||
                before.fire != after.fire || result.state.vertical_position != after.vertical_position ||
                result.vertical_position_write.value_or(before.display_y) != after.display_y ||
                color != after.color_index || countdown != after.sound_countdown) {
                throw std::runtime_error("Cartridge shot state differs at case " + std::to_string(sample.index));
            }
            for (const auto observed : after.pointers) {
                if (observed != pointer) throw std::runtime_error("Cartridge shot visibility differs");
            }
            if (sample.sentinel) ++sentinels;
            else covered.at(sample.fire_pressed).at(before.vertical_position) = true;
            hash_byte(sample.fire_pressed);
            hash_byte(sample.sentinel);
            hash_state(before);
            hash_state(after);
        }
        for (const auto& fire : covered) {
            for (const auto found : fire) {
                if (!found) throw std::runtime_error("Incomplete shot input coverage");
            }
        }
        if (sentinels != 4 || fingerprint != test_data::player_shot_stream_fnv1a) {
            throw std::runtime_error("Shot fixture integrity differs");
        }
        std::cout << test_data::player_shot_cases.size() << " controlled cartridge shot cases match\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
