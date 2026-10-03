#include "app/flight_hud.hpp"

#include "game_data.hpp"
#include "presentation_palette.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint32_t pixel_for(std::uint8_t glyph, int x, int y) {
    return assets::presentation_palette[assets::hud_tiles[glyph][y * 8 + x]];
}

std::uint32_t frame_pixel(const Frame& frame, int x, int y) {
    return frame.pixels[static_cast<std::size_t>(y * frame.width + x)];
}

bool tile_matches(const Frame& frame, std::uint8_t glyph, int column, int row) {
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (frame_pixel(frame, column * 8 + x, 176 + row * 8 + y) !=
                pixel_for(glyph, x, y)) {
                return false;
            }
        }
    }
    return true;
}

std::uint32_t base_hud_pixel(int x, int y) {
    const auto row = (y - 176) / 8;
    const auto column = x / 8;
    const auto glyph = assets::hud_cells[static_cast<std::size_t>(row * 40 + column)];
    return assets::presentation_palette[
        assets::hud_tiles[glyph][static_cast<std::size_t>((y - 176) % 8 * 8 + x % 8)]];
}

void require_marker(const Frame& frame, std::uint16_t fuel) {
    const int left = 104 + static_cast<int>(fuel >> 11) * 2;
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int screen_x = left + x;
            const int screen_y = 180 + y;
            auto expected = base_hud_pixel(screen_x, screen_y);
            if (expected == assets::presentation_palette[12]) {
                expected = assets::presentation_palette[7];
            }
            require(frame_pixel(frame, screen_x, screen_y) == expected,
                    "Fuel marker shape or occlusion changed");
        }
    }
}

void test_score_and_planes_cells() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff123456)};
    draw_flight_hud(frame, 123'450, 3, 0xFFFF, 1);

    // Native points are stored in units of ten by the original six-digit field:
    // 123450 therefore renders as blank, 1, 2, 3, 4, 5, followed by fixed 0.
    constexpr std::uint8_t score_glyphs[]{0x00, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC0};
    for (int column = 0; column < 7; ++column) {
        const auto hud_column = 26 + column;
        require(tile_matches(frame, score_glyphs[column], hud_column, 0),
                "Score glyph was placed at the wrong HUD cell");
    }
    require(tile_matches(frame, 0xC3, 37, 1),
            "Reserve-plane count was placed at the wrong HUD cell");
    require(tile_matches(frame, 0x00, 6, 1) && tile_matches(frame, 0x00, 7, 1) &&
                tile_matches(frame, 0x00, 8, 1) && tile_matches(frame, 0xC1, 9, 1),
            "Bridge number was placed at the wrong HUD cells");
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const bool score_cell = y >= 176 && y < 192 && x >= 208 && x < 264;
            const bool score_label = y >= 184 && y < 192 && x >= 184 && x < 200;
            const bool lives_cell = y >= 184 && y < 192 && x >= 296 && x < 304;
            const bool bridge_cell = y >= 184 && y < 192 && x >= 48 && x < 80;
            const bool fuel_strip = y >= 180 && y < 192 && x >= 104 && x < 170;
            if (!score_cell && !score_label && !lives_cell && !bridge_cell && !fuel_strip) {
                require(frame_pixel(frame, x, y) == 0xff123456,
                        "HUD overwrote terrain or an unrelated field");
            }
        }
    }
    draw_flight_hud(frame, 9'999'990, 9, 0xFFFF, 1);
    for (int column = 26; column < 32; ++column) {
        require(tile_matches(frame, 0xC9, column, 0), "Maximum score missing a digit");
    }
    draw_flight_hud(frame, 0, 0, 0xFFFF, 1);
    for (int column = 26; column < 32; ++column) {
        require(tile_matches(frame, 0x00, column, 0), "Restart retained stale score digits");
    }
    require(tile_matches(frame, 0xC0, 32, 0) && tile_matches(frame, 0xC0, 37, 1),
            "Zero score or zero lives rendered incorrectly");
}

void test_leading_zero_suppression_and_limits() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff123456)};
    draw_flight_hud(frame, 0, 12, 0xFFFF, 1);
    for (int column = 26; column < 32; ++column) {
        require(frame_pixel(frame, column * 8 + 2, 178) == pixel_for(0x00, 2, 2),
                "Leading zero did not remain blank");
    }
    require(frame_pixel(frame, 32 * 8 + 2, 178) == pixel_for(0xC0, 2, 2),
            "Fixed score units digit changed");
    require(frame_pixel(frame, 37 * 8 + 2, 186) == pixel_for(0xC9, 2, 2),
            "Reserve-plane count was not clamped to one digit");
}

void test_secondary_score_and_mode_label() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200)};
    draw_flight_hud(frame, 120, 3, 0xFFFF, 1, 450, true);
    require(tile_matches(frame, 0xC1, 30, 0) && tile_matches(frame, 0xC2, 31, 0),
            "Player 1 score moved out of its fixed row");
    require(tile_matches(frame, 0xC4, 30, 1) && tile_matches(frame, 0xC5, 31, 1) &&
                tile_matches(frame, 0xC0, 32, 1), "Player 2 score differs");
    require(tile_matches(frame, 0xCA, 23, 1) && tile_matches(frame, 0xCC, 24, 1),
            "Player 2 label differs");
    draw_flight_hud(frame, 0, 3, 0xFFFF, 1, 0, false);
    require(tile_matches(frame, 0xCD, 23, 1) && tile_matches(frame, 0xCE, 24, 1),
            "High-score label differs");
    for (int column = 26; column < 32; ++column) {
        require(tile_matches(frame, 0, column, 1), "Stale secondary score digit");
    }
}

void test_bridge_field_wraps_and_clears_stale_digits() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff123456)};
    draw_flight_hud(frame, 0, 3, 0xFFFF, 9'999);
    for (int column = 6; column < 10; ++column) {
        require(tile_matches(frame, 0xC9, column, 1),
                "Maximum bridge number missing a digit");
    }
    draw_flight_hud(frame, 0, 3, 0xFFFF, 10'000);
    for (int column = 6; column < 10; ++column) {
        require(tile_matches(frame, 0x00, column, 1),
                "Bridge number did not wrap at four BCD digits");
    }
    draw_flight_hud(frame, 0, 3, 0xFFFF, 42);
    require(tile_matches(frame, 0x00, 6, 1) && tile_matches(frame, 0x00, 7, 1) &&
                tile_matches(frame, 0xC4, 8, 1) && tile_matches(frame, 0xC2, 9, 1),
            "Bridge number leading zero suppression changed");
    for (const int column : {4, 5, 10, 11}) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                require(frame_pixel(frame, column * 8 + x, 184 + y) == 0xff123456,
                        "Bridge number overwrote an unrelated HUD field");
            }
        }
    }
}

void test_invalid_frame_rejected() {
    Frame frame{319, 200, std::vector<std::uint32_t>(319 * 200)};
    bool rejected = false;
    try {
        draw_flight_hud(frame, 0, 3, 0xFFFF, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Invalid HUD frame was accepted");
}

void test_fuel_marker_scale_and_replacement() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff123456)};
    for (unsigned step = 0; step < 32; ++step) {
        const auto fuel = static_cast<std::uint16_t>(step << 11);
        draw_flight_hud(frame, 0, 3, fuel, 1);
        require_marker(frame, fuel);
    }
    draw_flight_hud(frame, 0, 3, 0xFFFF, 1);
    require_marker(frame, 0xFFFF);
    draw_flight_hud(frame, 0, 3, 0x8000, 1);
    require_marker(frame, 0x8000);
    for (int y = 180; y < 192; ++y) {
        for (int x = 104; x < 170; ++x) {
            const bool current_marker = x >= 136 && x < 140;
            const bool base = frame_pixel(frame, x, y) == base_hud_pixel(x, y);
            if (!current_marker) {
                require(base, "Previous fuel marker was not restored");
            }
        }
    }
    draw_flight_hud(frame, 0, 3, 0, 1);
    require_marker(frame, 0);
    for (int y = 180; y < 192; ++y) {
        for (int x = 104; x < 170; ++x) {
            const bool current_marker = x >= 104 && x < 108;
            if (!current_marker) {
                require(frame_pixel(frame, x, y) == base_hud_pixel(x, y),
                        "Intermediate fuel marker was not restored");
            }
        }
    }
}

} // namespace

int main() {
    try {
        test_score_and_planes_cells();
        test_leading_zero_suppression_and_limits();
        test_secondary_score_and_mode_label();
        test_bridge_field_wraps_and_clears_stale_digits();
        test_invalid_frame_rejected();
        test_fuel_marker_scale_and_replacement();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
