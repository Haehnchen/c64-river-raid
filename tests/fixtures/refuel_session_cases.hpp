#pragma once

#include "game/player_steering.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace river_raid::test_data {

// Source observations only; the replay supplies ordinary controls.
struct RefuelSessionCase {
    std::uint16_t index;
    std::uint8_t direction;
    bool fire;
    bool inferred_input;
    PlayerSteeringState steering;
    FlightPose pose;
    std::uint16_t fuel;
    std::uint32_t score;
    std::uint8_t lives;
    bool refueled;
    bool saturated;
};

inline constexpr std::array<RefuelSessionCase, 154> refuel_session_cases{{
    {0, 2, true, true, {85, 240, 16, 66}, FlightPose::Left, 65503, 0, 3, false, false},
    {1, 2, true, false, {85, 208, 32, 68}, FlightPose::Left, 65471, 0, 3, false, false},
    {2, 2, true, false, {85, 160, 48, 70}, FlightPose::Left, 65439, 0, 3, false, false},
    {3, 2, true, false, {85, 96, 64, 72}, FlightPose::Left, 65407, 0, 3, false, false},
    {4, 2, true, false, {85, 16, 80, 74}, FlightPose::Left, 65375, 0, 3, false, false},
    {5, 0, true, false, {85, 0, 0, 76}, FlightPose::Straight, 65343, 0, 3, false, false},
    {6, 0, true, false, {85, 0, 0, 78}, FlightPose::Straight, 65311, 0, 3, false, false},
    {7, 0, true, false, {85, 0, 0, 80}, FlightPose::Straight, 65279, 0, 3, false, false},
    {8, 0, true, false, {85, 0, 0, 82}, FlightPose::Straight, 65247, 0, 3, false, false},
    {9, 0, true, false, {85, 0, 0, 84}, FlightPose::Straight, 65215, 0, 3, false, false},
    {10, 0, true, false, {85, 0, 0, 86}, FlightPose::Straight, 65183, 0, 3, false, false},
    {11, 0, true, false, {85, 0, 0, 88}, FlightPose::Straight, 65151, 0, 3, false, false},
    {12, 0, true, false, {85, 0, 0, 90}, FlightPose::Straight, 65119, 0, 3, false, false},
    {13, 0, true, false, {85, 0, 0, 92}, FlightPose::Straight, 65087, 0, 3, false, false},
    {14, 0, true, false, {85, 0, 0, 94}, FlightPose::Straight, 65055, 0, 3, false, false},
    {15, 0, true, false, {85, 0, 0, 96}, FlightPose::Straight, 65023, 0, 3, false, false},
    {16, 0, true, false, {85, 0, 0, 98}, FlightPose::Straight, 64991, 0, 3, false, false},
    {17, 0, true, false, {85, 0, 0, 100}, FlightPose::Straight, 64959, 0, 3, false, false},
    {18, 0, true, false, {85, 0, 0, 102}, FlightPose::Straight, 64927, 0, 3, false, false},
    {19, 0, true, false, {85, 0, 0, 104}, FlightPose::Straight, 64895, 0, 3, false, false},
    {20, 0, true, false, {85, 0, 0, 106}, FlightPose::Straight, 64863, 0, 3, false, false},
    {21, 0, true, false, {85, 0, 0, 108}, FlightPose::Straight, 64831, 0, 3, false, false},
    {22, 0, true, false, {85, 0, 0, 110}, FlightPose::Straight, 64799, 0, 3, false, false},
    {23, 0, true, false, {85, 0, 0, 112}, FlightPose::Straight, 64767, 0, 3, false, false},
    {24, 0, true, false, {85, 0, 0, 114}, FlightPose::Straight, 64735, 0, 3, false, false},
    {25, 0, true, false, {85, 0, 0, 116}, FlightPose::Straight, 64703, 0, 3, false, false},
    {26, 0, true, false, {85, 0, 0, 118}, FlightPose::Straight, 64671, 0, 3, false, false},
    {27, 0, true, false, {85, 0, 0, 120}, FlightPose::Straight, 64639, 0, 3, false, false},
    {28, 0, true, false, {85, 0, 0, 122}, FlightPose::Straight, 64607, 0, 3, false, false},
    {29, 0, true, false, {85, 0, 0, 124}, FlightPose::Straight, 64575, 0, 3, false, false},
    {30, 0, true, false, {85, 0, 0, 126}, FlightPose::Straight, 64543, 0, 3, false, false},
    {31, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64511, 0, 3, false, false},
    {32, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64479, 0, 3, false, false},
    {33, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64447, 0, 3, false, false},
    {34, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64415, 0, 3, false, false},
    {35, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64383, 0, 3, false, false},
    {36, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64351, 0, 3, false, false},
    {37, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64319, 0, 3, false, false},
    {38, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64287, 0, 3, false, false},
    {39, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64255, 0, 3, false, false},
    {40, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64223, 0, 3, false, false},
    {41, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64191, 0, 3, false, false},
    {42, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64159, 0, 3, false, false},
    {43, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64127, 0, 3, false, false},
    {44, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64095, 0, 3, false, false},
    {45, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64063, 0, 3, false, false},
    {46, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64031, 0, 3, false, false},
    {47, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63999, 0, 3, false, false},
    {48, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63967, 0, 3, false, false},
    {49, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63935, 0, 3, false, false},
    {50, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63903, 0, 3, false, false},
    {51, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63871, 0, 3, false, false},
    {52, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63839, 0, 3, false, false},
    {53, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63807, 0, 3, false, false},
    {54, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63775, 0, 3, false, false},
    {55, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63743, 0, 3, false, false},
    {56, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63711, 0, 3, false, false},
    {57, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63679, 0, 3, false, false},
    {58, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63647, 0, 3, false, false},
    {59, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63615, 0, 3, false, false},
    {60, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63583, 0, 3, false, false},
    {61, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63551, 0, 3, false, false},
    {62, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63519, 0, 3, false, false},
    {63, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63487, 0, 3, false, false},
    {64, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63455, 0, 3, false, false},
    {65, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64191, 0, 3, true, false},
    {66, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64927, 0, 3, true, false},
    {67, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 0, 3, true, true},
    {68, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {69, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {70, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {71, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {72, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {73, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65503, 30, 3, false, false},
    {74, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65471, 30, 3, false, false},
    {75, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65439, 30, 3, false, false},
    {76, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65407, 30, 3, false, false},
    {77, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {78, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {79, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {80, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {81, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {82, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {83, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {84, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {85, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {86, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {87, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65535, 30, 3, true, true},
    {88, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65503, 30, 3, false, false},
    {89, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65471, 30, 3, false, false},
    {90, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65439, 30, 3, false, false},
    {91, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65407, 30, 3, false, false},
    {92, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65375, 30, 3, false, false},
    {93, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65343, 30, 3, false, false},
    {94, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65311, 30, 3, false, false},
    {95, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65279, 30, 3, false, false},
    {96, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65247, 30, 3, false, false},
    {97, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65215, 30, 3, false, false},
    {98, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65183, 30, 3, false, false},
    {99, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65151, 30, 3, false, false},
    {100, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65119, 30, 3, false, false},
    {101, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65087, 30, 3, false, false},
    {102, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65055, 30, 3, false, false},
    {103, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 65023, 30, 3, false, false},
    {104, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64991, 30, 3, false, false},
    {105, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64959, 30, 3, false, false},
    {106, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64927, 30, 3, false, false},
    {107, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64895, 30, 3, false, false},
    {108, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64863, 30, 3, false, false},
    {109, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64831, 30, 3, false, false},
    {110, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64799, 30, 3, false, false},
    {111, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64767, 30, 3, false, false},
    {112, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64735, 30, 3, false, false},
    {113, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64703, 30, 3, false, false},
    {114, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64671, 30, 3, false, false},
    {115, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64639, 30, 3, false, false},
    {116, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64607, 30, 3, false, false},
    {117, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64575, 30, 3, false, false},
    {118, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64543, 30, 3, false, false},
    {119, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64511, 30, 3, false, false},
    {120, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64479, 30, 3, false, false},
    {121, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64447, 30, 3, false, false},
    {122, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64415, 30, 3, false, false},
    {123, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64383, 30, 3, false, false},
    {124, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64351, 30, 3, false, false},
    {125, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64319, 30, 3, false, false},
    {126, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64287, 30, 3, false, false},
    {127, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64255, 30, 3, false, false},
    {128, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64223, 30, 3, false, false},
    {129, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64191, 30, 3, false, false},
    {130, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64159, 30, 3, false, false},
    {131, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64127, 30, 3, false, false},
    {132, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64095, 30, 3, false, false},
    {133, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64063, 30, 3, false, false},
    {134, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 64031, 30, 3, false, false},
    {135, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63999, 30, 3, false, false},
    {136, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63967, 30, 3, false, false},
    {137, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63935, 30, 3, false, false},
    {138, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63903, 30, 3, false, false},
    {139, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63871, 30, 3, false, false},
    {140, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63839, 30, 3, false, false},
    {141, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63807, 30, 3, false, false},
    {142, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63775, 30, 3, false, false},
    {143, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63743, 30, 3, false, false},
    {144, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63711, 30, 3, false, false},
    {145, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63679, 30, 3, false, false},
    {146, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63647, 30, 3, false, false},
    {147, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63615, 30, 3, false, false},
    {148, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63583, 30, 3, false, false},
    {149, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63551, 30, 3, false, false},
    {150, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63519, 30, 3, false, false},
    {151, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63487, 30, 3, false, false},
    {152, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63455, 30, 3, false, false},
    {153, 0, true, false, {85, 0, 0, 128}, FlightPose::Straight, 63423, 30, 3, false, false},
}};

// Positive source consumer checkpoints, not pixel-producer proof.
struct RefuelContactWitness {
    std::size_t active_case_index;
    std::size_t pre_event;
    std::size_t post_event;
    std::uint8_t contact_slot;
    std::uint8_t record_index;
    std::uint8_t contact_mask;
    std::uint16_t fuel_before;
    std::uint16_t fuel_after;
    std::uint8_t audio_kind;
};

inline constexpr std::array<RefuelContactWitness, 19> refuel_contact_witnesses{{
    {65, 196, 197, 1, 11, 129, 63423, 64191, 4},
    {66, 201, 202, 1, 11, 129, 64159, 64927, 4},
    {67, 206, 207, 1, 11, 129, 64895, 65535, 3},
    {68, 211, 212, 1, 11, 129, 65503, 65535, 3},
    {69, 216, 217, 1, 11, 129, 65503, 65535, 3},
    {70, 221, 222, 1, 11, 129, 65503, 65535, 3},
    {71, 226, 227, 1, 11, 129, 65503, 65535, 3},
    {72, 231, 232, 1, 11, 129, 65503, 65535, 3},
    {77, 248, 249, 0, 11, 129, 65375, 65535, 3},
    {78, 253, 254, 0, 11, 129, 65503, 65535, 3},
    {79, 258, 259, 0, 11, 129, 65503, 65535, 3},
    {80, 263, 264, 0, 11, 129, 65503, 65535, 3},
    {81, 268, 269, 0, 11, 129, 65503, 65535, 3},
    {82, 273, 274, 0, 11, 129, 65503, 65535, 3},
    {83, 278, 279, 0, 11, 129, 65503, 65535, 3},
    {84, 283, 284, 0, 11, 129, 65503, 65535, 3},
    {85, 288, 289, 0, 11, 129, 65503, 65535, 3},
    {86, 293, 294, 0, 11, 129, 65503, 65535, 3},
    {87, 298, 299, 0, 11, 129, 65503, 65535, 3},
}};

} // namespace river_raid::test_data
