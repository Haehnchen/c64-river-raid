#pragma once

#include "game/player_steering.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace river_raid::test_data::game5_partial_session {

// Audited partial source observations; sampled controls include policy cleanup.
// This is not a completed route or gameplay-parity acceptance fixture.
struct BridgeSessionCase {
    std::uint16_t index;
    std::uint8_t direction;
    bool fire;
    bool inferred_input;
    std::uint8_t input_18;
    std::uint8_t fire_19;
    std::uint8_t animation_phase;
    PlayerSteeringState steering;
    FlightPose pose;
    std::uint16_t fuel;
    std::uint32_t score;
    std::uint8_t lives;
    std::uint16_t bridge_number;
    std::uint8_t shot_vertical;
};

inline constexpr std::array<BridgeSessionCase, 125> session_cases{{
    {0, 0, true, true, 15, 0, 84, {86, 0, 0, 66}, FlightPose::Straight, 65503, 0, 3, 20, 192},
    {1, 0, true, false, 15, 0, 85, {86, 0, 0, 68}, FlightPose::Straight, 65471, 0, 3, 20, 188},
    {2, 0, true, false, 15, 0, 86, {86, 0, 0, 70}, FlightPose::Straight, 65439, 0, 3, 20, 184},
    {3, 0, true, false, 15, 0, 87, {86, 0, 0, 72}, FlightPose::Straight, 65407, 0, 3, 20, 180},
    {4, 0, true, false, 15, 0, 88, {86, 0, 0, 74}, FlightPose::Straight, 65375, 0, 3, 20, 176},
    {5, 0, true, false, 15, 0, 89, {86, 0, 0, 76}, FlightPose::Straight, 65343, 0, 3, 20, 172},
    {6, 0, true, false, 15, 0, 90, {86, 0, 0, 78}, FlightPose::Straight, 65311, 0, 3, 20, 168},
    {7, 0, true, false, 15, 0, 91, {86, 0, 0, 80}, FlightPose::Straight, 65279, 0, 3, 20, 164},
    {8, 0, true, false, 15, 0, 92, {86, 0, 0, 82}, FlightPose::Straight, 65247, 0, 3, 20, 160},
    {9, 0, true, false, 15, 0, 93, {86, 0, 0, 84}, FlightPose::Straight, 65215, 0, 3, 20, 156},
    {10, 0, true, false, 15, 0, 94, {86, 0, 0, 86}, FlightPose::Straight, 65183, 0, 3, 20, 152},
    {11, 0, true, false, 15, 0, 95, {86, 0, 0, 88}, FlightPose::Straight, 65151, 0, 3, 20, 148},
    {12, 0, true, false, 15, 0, 96, {86, 0, 0, 90}, FlightPose::Straight, 65119, 0, 3, 20, 144},
    {13, 0, true, false, 15, 0, 97, {86, 0, 0, 92}, FlightPose::Straight, 65087, 0, 3, 20, 140},
    {14, 0, true, false, 15, 0, 98, {86, 0, 0, 94}, FlightPose::Straight, 65055, 0, 3, 20, 136},
    {15, 0, true, false, 15, 0, 99, {86, 0, 0, 96}, FlightPose::Straight, 65023, 0, 3, 20, 132},
    {16, 0, true, false, 15, 0, 100, {86, 0, 0, 98}, FlightPose::Straight, 64991, 0, 3, 20, 128},
    {17, 0, true, false, 15, 0, 101, {86, 0, 0, 100}, FlightPose::Straight, 64959, 0, 3, 20, 124},
    {18, 0, true, false, 15, 0, 102, {86, 0, 0, 102}, FlightPose::Straight, 64927, 0, 3, 20, 120},
    {19, 0, true, false, 15, 0, 103, {86, 0, 0, 104}, FlightPose::Straight, 64895, 0, 3, 20, 0},
    {20, 0, true, false, 15, 0, 104, {86, 0, 0, 106}, FlightPose::Straight, 64863, 0, 3, 20, 192},
    {21, 0, true, false, 15, 0, 105, {86, 0, 0, 108}, FlightPose::Straight, 64831, 0, 3, 20, 188},
    {22, 0, true, false, 15, 0, 106, {86, 0, 0, 110}, FlightPose::Straight, 64799, 0, 3, 20, 184},
    {23, 0, true, false, 15, 0, 107, {86, 0, 0, 112}, FlightPose::Straight, 64767, 0, 3, 20, 180},
    {24, 0, true, false, 15, 0, 108, {86, 0, 0, 114}, FlightPose::Straight, 64735, 0, 3, 20, 176},
    {25, 0, true, false, 15, 0, 109, {86, 0, 0, 116}, FlightPose::Straight, 64703, 0, 3, 20, 172},
    {26, 0, true, false, 15, 0, 110, {86, 0, 0, 118}, FlightPose::Straight, 64671, 0, 3, 20, 168},
    {27, 0, true, false, 15, 0, 111, {86, 0, 0, 120}, FlightPose::Straight, 64639, 0, 3, 20, 164},
    {28, 0, true, false, 15, 0, 112, {86, 0, 0, 122}, FlightPose::Straight, 64607, 0, 3, 20, 160},
    {29, 0, true, false, 15, 0, 113, {86, 0, 0, 124}, FlightPose::Straight, 64575, 0, 3, 20, 156},
    {30, 0, true, false, 15, 0, 114, {86, 0, 0, 126}, FlightPose::Straight, 64543, 0, 3, 20, 152},
    {31, 0, true, false, 15, 0, 115, {86, 0, 0, 128}, FlightPose::Straight, 64511, 0, 3, 20, 148},
    {32, 0, true, false, 15, 0, 116, {86, 0, 0, 128}, FlightPose::Straight, 64479, 0, 3, 20, 144},
    {33, 1, true, false, 7, 0, 117, {86, 16, 16, 128}, FlightPose::Right, 64447, 0, 3, 20, 140},
    {34, 1, true, false, 7, 0, 118, {86, 48, 32, 128}, FlightPose::Right, 64415, 0, 3, 20, 136},
    {35, 1, true, false, 7, 0, 119, {86, 96, 48, 128}, FlightPose::Right, 64383, 0, 3, 20, 132},
    {36, 1, true, false, 7, 0, 120, {86, 160, 64, 128}, FlightPose::Right, 64351, 0, 3, 20, 0},
    {37, 1, true, false, 7, 0, 121, {86, 240, 80, 128}, FlightPose::Right, 64319, 0, 3, 20, 192},
    {38, 1, true, false, 7, 0, 122, {87, 80, 96, 128}, FlightPose::Right, 64287, 0, 3, 20, 188},
    {39, 1, true, false, 7, 0, 123, {87, 192, 112, 128}, FlightPose::Right, 64255, 0, 3, 20, 184},
    {40, 1, true, false, 7, 0, 124, {88, 64, 128, 128}, FlightPose::Right, 64223, 0, 3, 20, 180},
    {41, 1, true, false, 7, 0, 125, {88, 208, 144, 128}, FlightPose::Right, 64191, 0, 3, 20, 176},
    {42, 1, true, false, 7, 0, 126, {89, 112, 160, 128}, FlightPose::Right, 64159, 0, 3, 20, 172},
    {43, 1, true, false, 7, 0, 127, {90, 32, 176, 128}, FlightPose::Right, 64127, 0, 3, 20, 168},
    {44, 1, true, false, 7, 0, 128, {90, 224, 192, 128}, FlightPose::Right, 64095, 0, 3, 20, 164},
    {45, 1, true, false, 7, 0, 129, {91, 176, 208, 128}, FlightPose::Right, 64063, 0, 3, 20, 160},
    {46, 1, true, false, 7, 0, 130, {92, 144, 224, 128}, FlightPose::Right, 64031, 0, 3, 20, 156},
    {47, 1, true, false, 7, 0, 131, {93, 128, 240, 128}, FlightPose::Right, 63999, 0, 3, 20, 152},
    {48, 1, true, false, 7, 0, 132, {94, 0, 240, 128}, FlightPose::Right, 63967, 0, 3, 20, 148},
    {49, 1, true, false, 7, 0, 133, {95, 0, 240, 128}, FlightPose::Right, 63935, 0, 3, 20, 0},
    {50, 1, true, false, 7, 0, 134, {96, 0, 240, 128}, FlightPose::Right, 63903, 0, 3, 20, 192},
    {51, 1, true, false, 7, 0, 135, {97, 0, 240, 128}, FlightPose::Right, 63871, 0, 3, 20, 188},
    {52, 1, true, false, 7, 0, 136, {98, 0, 240, 128}, FlightPose::Right, 63839, 0, 3, 20, 184},
    {53, 1, true, false, 7, 0, 137, {99, 0, 240, 128}, FlightPose::Right, 63807, 0, 3, 20, 180},
    {54, 1, true, false, 7, 0, 138, {100, 0, 240, 128}, FlightPose::Right, 63775, 0, 3, 20, 176},
    {55, 1, true, false, 7, 0, 139, {101, 0, 240, 128}, FlightPose::Right, 63743, 0, 3, 20, 172},
    {56, 1, true, false, 7, 0, 140, {102, 0, 240, 128}, FlightPose::Right, 63711, 0, 3, 20, 168},
    {57, 1, true, false, 7, 0, 141, {103, 0, 240, 128}, FlightPose::Right, 63679, 0, 3, 20, 164},
    {58, 1, true, false, 7, 0, 142, {104, 0, 240, 128}, FlightPose::Right, 63647, 0, 3, 20, 160},
    {59, 1, true, false, 7, 0, 143, {105, 0, 240, 128}, FlightPose::Right, 63615, 0, 3, 20, 156},
    {60, 1, true, false, 7, 0, 144, {106, 0, 240, 128}, FlightPose::Right, 63583, 0, 3, 20, 152},
    {61, 1, true, false, 7, 0, 145, {107, 0, 240, 128}, FlightPose::Right, 63551, 0, 3, 20, 148},
    {62, 1, true, false, 7, 0, 146, {108, 0, 240, 128}, FlightPose::Right, 63519, 0, 3, 20, 144},
    {63, 1, true, false, 7, 0, 147, {109, 0, 240, 128}, FlightPose::Right, 63487, 0, 3, 20, 140},
    {64, 1, true, false, 7, 0, 148, {110, 0, 240, 128}, FlightPose::Right, 63455, 0, 3, 20, 136},
    {65, 1, true, false, 7, 0, 149, {111, 0, 240, 128}, FlightPose::Right, 63423, 0, 3, 20, 132},
    {66, 1, true, false, 7, 0, 150, {112, 0, 240, 128}, FlightPose::Right, 63391, 0, 3, 20, 128},
    {67, 1, true, false, 7, 0, 151, {113, 0, 240, 128}, FlightPose::Right, 63359, 0, 3, 20, 124},
    {68, 1, true, false, 7, 0, 152, {114, 0, 240, 128}, FlightPose::Right, 63327, 0, 3, 20, 120},
    {69, 1, true, false, 7, 0, 153, {115, 0, 240, 128}, FlightPose::Right, 63295, 0, 3, 20, 116},
    {70, 1, true, false, 7, 0, 154, {116, 0, 240, 128}, FlightPose::Right, 63263, 0, 3, 20, 112},
    {71, 1, true, false, 7, 0, 155, {117, 0, 240, 128}, FlightPose::Right, 63231, 0, 3, 20, 108},
    {72, 1, true, false, 7, 0, 156, {118, 0, 240, 128}, FlightPose::Right, 63199, 0, 3, 20, 104},
    {73, 1, true, false, 7, 0, 157, {119, 0, 240, 128}, FlightPose::Right, 63167, 0, 3, 20, 100},
    {74, 1, true, false, 7, 0, 158, {120, 0, 240, 128}, FlightPose::Right, 63135, 0, 3, 20, 96},
    {75, 1, true, false, 7, 0, 159, {121, 0, 240, 128}, FlightPose::Right, 63103, 0, 3, 20, 92},
    {76, 1, true, false, 7, 0, 160, {122, 0, 240, 128}, FlightPose::Right, 63071, 0, 3, 20, 88},
    {77, 1, true, false, 7, 0, 161, {123, 0, 240, 128}, FlightPose::Right, 63039, 0, 3, 20, 84},
    {78, 1, true, false, 7, 0, 162, {124, 0, 240, 128}, FlightPose::Right, 63007, 0, 3, 20, 80},
    {79, 1, true, false, 7, 0, 163, {125, 0, 240, 128}, FlightPose::Right, 62975, 0, 3, 20, 76},
    {80, 1, true, false, 7, 0, 164, {126, 0, 240, 128}, FlightPose::Right, 62943, 0, 3, 20, 72},
    {81, 1, true, false, 7, 0, 165, {127, 0, 240, 128}, FlightPose::Right, 62911, 0, 3, 20, 68},
    {82, 1, true, false, 7, 0, 166, {128, 0, 240, 128}, FlightPose::Right, 62879, 0, 3, 20, 64},
    {83, 1, true, false, 7, 0, 167, {129, 0, 240, 128}, FlightPose::Right, 62847, 0, 3, 20, 60},
    {84, 1, true, false, 7, 0, 168, {130, 0, 240, 128}, FlightPose::Right, 62815, 0, 3, 20, 56},
    {85, 1, true, false, 7, 0, 169, {131, 0, 240, 128}, FlightPose::Right, 62783, 0, 3, 20, 52},
    {86, 1, true, false, 7, 0, 170, {132, 0, 240, 128}, FlightPose::Right, 62751, 0, 3, 20, 0},
    {87, 1, true, false, 7, 0, 171, {133, 0, 240, 128}, FlightPose::Right, 62719, 0, 3, 20, 192},
    {88, 1, true, false, 7, 0, 172, {134, 0, 240, 128}, FlightPose::Right, 62687, 0, 3, 20, 188},
    {89, 1, true, false, 7, 0, 173, {135, 0, 240, 128}, FlightPose::Right, 62655, 0, 3, 20, 184},
    {90, 1, true, false, 7, 0, 174, {136, 0, 240, 128}, FlightPose::Right, 62623, 0, 3, 20, 180},
    {91, 1, true, false, 7, 0, 175, {137, 0, 240, 128}, FlightPose::Right, 62591, 0, 3, 20, 176},
    {92, 1, true, false, 7, 0, 176, {138, 0, 240, 128}, FlightPose::Right, 62559, 0, 3, 20, 172},
    {93, 1, true, false, 7, 0, 177, {139, 0, 240, 128}, FlightPose::Right, 62527, 0, 3, 20, 168},
    {94, 1, true, false, 7, 0, 178, {140, 0, 240, 128}, FlightPose::Right, 62495, 0, 3, 20, 164},
    {95, 1, true, false, 7, 0, 179, {141, 0, 240, 128}, FlightPose::Right, 62463, 0, 3, 20, 160},
    {96, 1, true, false, 7, 0, 180, {142, 0, 240, 128}, FlightPose::Right, 62431, 0, 3, 20, 156},
    {97, 1, true, false, 7, 0, 181, {143, 0, 240, 128}, FlightPose::Right, 62399, 0, 3, 20, 152},
    {98, 1, true, false, 7, 0, 182, {144, 0, 240, 128}, FlightPose::Right, 62367, 0, 3, 20, 148},
    {99, 0, false, false, 15, 16, 183, {144, 0, 0, 128}, FlightPose::Straight, 62335, 0, 3, 20, 0},
    {100, 0, false, false, 15, 16, 184, {144, 0, 0, 128}, FlightPose::Straight, 62303, 0, 3, 20, 0},
    {101, 0, false, false, 15, 16, 185, {144, 0, 0, 128}, FlightPose::Straight, 62271, 0, 3, 20, 0},
    {102, 0, false, false, 15, 16, 186, {144, 0, 0, 128}, FlightPose::Straight, 62239, 0, 3, 20, 0},
    {103, 0, false, false, 15, 16, 187, {144, 0, 0, 128}, FlightPose::Straight, 62207, 0, 3, 20, 0},
    {104, 0, false, false, 15, 16, 188, {144, 0, 0, 128}, FlightPose::Straight, 62175, 0, 3, 20, 0},
    {105, 0, false, false, 15, 16, 189, {144, 0, 0, 128}, FlightPose::Straight, 62143, 0, 3, 20, 0},
    {106, 0, false, false, 15, 16, 190, {144, 0, 0, 128}, FlightPose::Straight, 62111, 0, 3, 20, 0},
    {107, 0, false, false, 15, 16, 191, {144, 0, 0, 128}, FlightPose::Straight, 62079, 0, 3, 20, 0},
    {108, 0, false, false, 15, 16, 192, {144, 0, 0, 128}, FlightPose::Straight, 62047, 0, 3, 20, 0},
    {109, 0, false, false, 15, 16, 193, {144, 0, 0, 128}, FlightPose::Straight, 62015, 0, 3, 20, 0},
    {110, 0, false, false, 15, 16, 194, {144, 0, 0, 128}, FlightPose::Straight, 61983, 0, 3, 20, 0},
    {111, 0, false, false, 15, 16, 195, {144, 0, 0, 128}, FlightPose::Straight, 61951, 0, 3, 20, 0},
    {112, 0, false, false, 15, 16, 196, {144, 0, 0, 128}, FlightPose::Straight, 61919, 0, 3, 20, 0},
    {113, 0, false, false, 15, 16, 197, {144, 0, 0, 128}, FlightPose::Straight, 61887, 0, 3, 20, 0},
    {114, 0, false, false, 15, 16, 198, {144, 0, 0, 128}, FlightPose::Straight, 61855, 0, 3, 20, 0},
    {115, 0, false, false, 15, 16, 199, {144, 0, 0, 128}, FlightPose::Straight, 61823, 0, 3, 20, 0},
    {116, 0, false, false, 15, 16, 200, {144, 0, 0, 128}, FlightPose::Straight, 61791, 0, 3, 20, 0},
    {117, 0, false, false, 15, 16, 201, {144, 0, 0, 128}, FlightPose::Straight, 61759, 0, 3, 20, 0},
    {118, 0, false, false, 15, 16, 202, {144, 0, 0, 128}, FlightPose::Straight, 61727, 0, 3, 20, 0},
    {119, 0, false, false, 15, 16, 203, {144, 0, 0, 128}, FlightPose::Straight, 61695, 0, 3, 20, 0},
    {120, 0, false, false, 15, 16, 204, {144, 0, 0, 128}, FlightPose::Straight, 61663, 0, 3, 20, 0},
    {121, 0, false, false, 15, 16, 205, {144, 0, 0, 128}, FlightPose::Straight, 61631, 0, 3, 20, 0},
    {122, 0, false, false, 15, 16, 206, {144, 0, 0, 128}, FlightPose::Straight, 61599, 0, 3, 20, 0},
    {123, 0, false, false, 15, 16, 207, {144, 0, 0, 128}, FlightPose::Straight, 61567, 0, 3, 20, 0},
    {124, 0, false, false, 15, 16, 208, {144, 0, 0, 128}, FlightPose::Straight, 61535, 0, 3, 20, 0},
}};

} // namespace river_raid::test_data::game5_partial_session
