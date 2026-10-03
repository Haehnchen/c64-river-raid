#include "game/player_steering.hpp"
#include "fixtures/player_control_cases.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace river_raid;
    try {
        std::array<bool, 16> directions{};
        std::array<std::array<bool, 256>, 4> speeds{};
        unsigned biased_add_cases = 0;
        std::uint64_t fingerprint = 14695981039346656037ULL;
        const auto hash_byte = [&](std::uint8_t value) {
            fingerprint = (fingerprint ^ value) * 1099511628211ULL;
        };
        const auto hash_state = [&](const test_data::PlayerControlState& state) {
            for (const auto value : {state.horizontal_step, state.horizontal_position,
                                     state.horizontal_fraction, state.scroll_speed,
                                     state.sprite_pointer, state.fraction_bias_byte}) hash_byte(value);
        };
        unsigned index = 0;
        for (const auto& sample : test_data::player_control_cases) {
            if (sample.index != index++) throw std::runtime_error("Nonsequential fixture index");
            const auto bits = sample.active_low_directions;
            const FlightControls controls{!(bits & 1), !(bits & 2), !(bits & 4), !(bits & 8)};
            const auto& before = sample.before;
            const auto& after = sample.after;
            hash_byte(bits);
            hash_byte(before.fire);
            hash_state(before);
            hash_state(after);
            const PlayerSteeringState input{before.horizontal_position, before.horizontal_fraction,
                                            before.horizontal_step, before.scroll_speed};
            const PlayerSteeringState expected{after.horizontal_position, after.horizontal_fraction,
                                               after.horizontal_step, after.scroll_speed};
            const bool bias = (before.fraction_bias_byte & 128) != 0;
            const auto result = steer_player(input, controls, {bias});
            const unsigned pointer = result.pose == FlightPose::Straight ? 0x9D :
                                     result.pose == FlightPose::Right ? 0x9E : 0x9F;
            if (result.state != expected || pointer != after.sprite_pointer ||
                after.input_nibble != bits || after.fire != 0 ||
                before.fraction_bias_byte != after.fraction_bias_byte) {
                throw std::runtime_error("Controlled cartridge case differs: " + std::to_string(sample.index));
            }
            directions.at(bits) = true;
            if ((bits & 12) == 12) speeds.at(bits & 3).at(before.scroll_speed) = true;
            if (bias && controls.right && before.horizontal_step < 240) ++biased_add_cases;
        }
        for (const bool covered : directions) {
            if (!covered) throw std::runtime_error("Missing direction coverage");
        }
        for (const auto& vertical : speeds) {
            for (const bool covered : vertical) {
                if (!covered) throw std::runtime_error("Missing vertical speed coverage");
            }
        }
        if (!biased_add_cases) throw std::runtime_error("Missing biased rightward addition");
        if (fingerprint != test_data::player_control_stream_fnv1a) {
            throw std::runtime_error("Fixture fingerprint differs");
        }
        std::cout << test_data::player_control_cases.size() << " controlled cartridge cases match\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
