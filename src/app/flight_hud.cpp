#include "app/flight_hud.hpp"

#include "game_data.hpp"
#include "presentation_palette.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace river_raid {
namespace {

constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 200;
constexpr std::size_t kHudTop = 176;
constexpr std::size_t kScoreColumn = 26;
constexpr std::size_t kScoreDigits = 6;
constexpr std::size_t kScoreTailColumn = kScoreColumn + kScoreDigits;
constexpr std::size_t kPlanesRow = 1;
constexpr std::size_t kPlanesColumn = 37;
constexpr std::size_t kBridgeRow = 1;
constexpr std::size_t kBridgeColumn = 6;
constexpr std::size_t kBridgeDigits = 4;
constexpr std::size_t kFuelMarkerTop = 180;
constexpr std::size_t kFuelMarkerHeight = 12;
constexpr std::size_t kFuelMarkerWidth = 4;
constexpr std::size_t kFuelMarkerMinLeft = 104;
constexpr std::size_t kFuelMarkerMaxLeft = 166;
constexpr std::size_t kFuelStripRight = kFuelMarkerMaxLeft + kFuelMarkerWidth;
constexpr std::uint8_t kDigitGlyph = 0xC0;
constexpr std::uint32_t kMaximumDisplayedScore = 999'999;

void draw_tile(Frame& destination, std::uint8_t glyph, std::size_t column, std::size_t row) {
    const auto& tile = assets::hud_tiles[glyph];
    const auto left = column * 8;
    const auto top = kHudTop + row * 8;
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            destination.pixels[(top + y) * kFrameWidth + left + x] =
                assets::presentation_palette[tile[y * 8 + x]];
        }
    }
}

std::uint32_t hud_pixel(std::size_t x, std::size_t y) {
    const auto row = (y - kHudTop) / 8;
    const auto column = x / 8;
    const auto glyph = assets::hud_cells[row * 40 + column];
    const auto& tile = assets::hud_tiles[glyph];
    return assets::presentation_palette[tile[(y - kHudTop) % 8 * 8 + x % 8]];
}

bool hud_background(std::size_t x, std::size_t y) {
    const auto row = (y - kHudTop) / 8;
    const auto column = x / 8;
    const auto glyph = assets::hud_cells[row * 40 + column];
    const auto& tile = assets::hud_tiles[glyph];
    return tile[(y - kHudTop) % 8 * 8 + x % 8] == 12;
}

void restore_fuel_strip(Frame& destination) {
    for (std::size_t y = kFuelMarkerTop; y < kFuelMarkerTop + kFuelMarkerHeight; ++y) {
        for (std::size_t x = kFuelMarkerMinLeft; x < kFuelStripRight; ++x) {
            destination.pixels[y * kFrameWidth + x] = hud_pixel(x, y);
        }
    }
}

std::size_t fuel_marker_left(std::uint16_t fuel) noexcept {
    const auto coarse = static_cast<std::size_t>(fuel >> 11);
    return kFuelMarkerMinLeft + coarse * 2;
}

void draw_fuel_marker(Frame& destination, std::uint16_t fuel) {
    restore_fuel_strip(destination);
    const auto left = fuel_marker_left(fuel);
    const auto color = assets::hud_fuel_marker.front();
    for (std::size_t y = 0; y < kFuelMarkerHeight; ++y) {
        // The fixed asset is already occluded by the full-position HUD. The
        // source sprite itself has four active pixels on every row.
        for (std::size_t x = 0; x < kFuelMarkerWidth; ++x) {
            const auto destination_x = left + x;
            if (hud_background(destination_x, kFuelMarkerTop + y)) {
                destination.pixels[(kFuelMarkerTop + y) * kFrameWidth + destination_x] =
                    assets::presentation_palette[color];
            }
        }
    }
}

template <std::size_t Digits>
std::array<std::uint8_t, Digits> decimal_glyphs(std::uint32_t value) noexcept {
    std::array<std::uint8_t, Digits> glyphs{};
    for (std::size_t index = Digits; index-- > 0;) {
        glyphs[index] = static_cast<std::uint8_t>(kDigitGlyph + value % 10);
        value /= 10;
    }
    bool significant = false;
    for (auto& glyph : glyphs) {
        if (glyph == kDigitGlyph && !significant) {
            glyph = 0;
        } else {
            significant = true;
        }
    }
    return glyphs;
}

std::array<std::uint8_t, kScoreDigits> score_glyphs(std::uint32_t score) noexcept {
    return decimal_glyphs<kScoreDigits>(std::min(score / 10, kMaximumDisplayedScore));
}

std::array<std::uint8_t, kBridgeDigits> bridge_glyphs(std::uint16_t bridge_number) noexcept {
    // The original keeps two packed BCD bytes and discards carry after 9999.
    return decimal_glyphs<kBridgeDigits>(bridge_number % 10'000);
}

} // namespace

void draw_flight_hud(Frame& destination, std::uint32_t score, int lives,
                     std::uint16_t fuel, std::uint16_t bridge_number,
                     std::uint32_t secondary_score, bool two_players) {
    if (destination.width != kFrameWidth || destination.height != kFrameHeight ||
        destination.pixels.size() != static_cast<std::size_t>(kFrameWidth * kFrameHeight)) {
        throw std::invalid_argument("Flight HUD needs a 320 by 200 frame");
    }

    const auto score_digits = score_glyphs(score);
    for (std::size_t index = 0; index < kScoreDigits; ++index) {
        draw_tile(destination, score_digits[index],
                  kScoreColumn + index, 0);
    }
    // The C64 field is six generated digits plus a fixed units digit.
    draw_tile(destination, kDigitGlyph, kScoreTailColumn, 0);
    const auto secondary_digits = score_glyphs(secondary_score);
    for (std::size_t index = 0; index < kScoreDigits; ++index) {
        draw_tile(destination, secondary_digits[index],
                  kScoreColumn + index, 1);
    }
    draw_tile(destination, kDigitGlyph, kScoreTailColumn, 1);
    draw_tile(destination, two_players ? 0xCA : 0xCD, 23, 1);
    draw_tile(destination, two_players ? 0xCC : 0xCE, 24, 1);

    const auto bridge_digits = bridge_glyphs(bridge_number);
    for (std::size_t index = 0; index < kBridgeDigits; ++index) {
        draw_tile(destination, bridge_digits[index],
                  kBridgeColumn + index, kBridgeRow);
    }

    const auto planes = std::clamp(lives, 0, 9);
    draw_tile(destination, static_cast<std::uint8_t>(kDigitGlyph + planes),
              kPlanesColumn, kPlanesRow);
    draw_fuel_marker(destination, fuel);
}

} // namespace river_raid
