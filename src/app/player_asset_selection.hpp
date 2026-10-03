#pragma once

#include "game/life_cycle.hpp"
#include "game/player_steering.hpp"

#include <cstddef>
#include <stdexcept>

namespace river_raid {

// Rendering and contact masks must select the same shipped aircraft images.
inline std::size_t player_pose_index(FlightPose pose) {
    switch (pose) {
    case FlightPose::Straight: return 0;
    case FlightPose::Right: return 1;
    case FlightPose::Left: return 2;
    }
    throw std::invalid_argument("Invalid player pose");
}

inline std::size_t player_explosion_index(LifeCycleImage image) {
    switch (image) {
    case LifeCycleImage::ExplosionA: return 20;
    case LifeCycleImage::ExplosionB: return 21;
    default: throw std::invalid_argument("Invalid player explosion image");
    }
}

} // namespace river_raid
