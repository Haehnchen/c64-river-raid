#pragma once

#include "app/contact_asset_setup.hpp"

#include <stdexcept>

namespace river_raid::test_support {

inline void verify_hud_tail_bound(int first_line, int opaque_bottom, int y_offset) {
    if (first_line == -1 && opaque_bottom == -1) return;
    if (first_line < 0 || first_line >= 312 || opaque_bottom < 0 ||
        opaque_bottom >= first_line || y_offset < 0 || y_offset > 1 ||
        opaque_bottom + y_offset >= first_line) {
        throw std::invalid_argument("HUD pixels reach the pointer transition");
    }
}

inline OccupancyMask contact_fixture_asset(unsigned kind, unsigned image, unsigned style) {
    switch (kind) {
    case 0: return make_player_contact_mask(FlightPose::Straight);
    case 1: return make_player_contact_mask(FlightPose::Right);
    case 2: return make_player_contact_mask(FlightPose::Left);
    case 3: return make_projectile_contact_mask();
    case 6: return make_player_detail_contact_mask();
    case 4: case 5:
        if (image > 31 || style > 15) throw std::invalid_argument("Invalid object asset");
        return make_world_object_contact_mask({0, kind == 4 ? WorldObjectImageSlot::primary
                                                           : WorldObjectImageSlot::alternate,
            0, 0, static_cast<std::uint8_t>(image), static_cast<std::uint8_t>(style)});
    default: throw std::invalid_argument("Unknown pixel asset");
    }
}

}
