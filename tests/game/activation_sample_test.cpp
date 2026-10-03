#include "game/activation_sample.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        constexpr std::array<unsigned, 16> expected{0, 1, 2, 3, 4, 5, 6, 7,
                                                   8, 9, 10, 11, 8, 9, 10, 11};
        std::array<unsigned, 12> preimages{};
        for (unsigned byte = 0; byte < 256; ++byte) {
            const auto value = static_cast<std::uint8_t>(byte);
            const auto candidate = river_raid::activation_candidate_from_sample(value);
            if (candidate != expected[byte % 16]) throw std::runtime_error("Sample projection differs");
            ++preimages[candidate];
            if (river_raid::is_activation_phase(value) != (byte % 16 == 0)) {
                throw std::runtime_error("Activation cadence differs");
            }
        }
        for (unsigned slot = 0; slot < preimages.size(); ++slot) {
            if (preimages[slot] != (slot < 8 ? 16U : 32U)) {
                throw std::runtime_error("Sample preimage count differs");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
