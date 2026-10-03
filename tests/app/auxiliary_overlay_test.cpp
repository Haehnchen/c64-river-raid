#include "app/auxiliary_overlay.hpp"
#include "auxiliary_sprite.hpp"
#include "presentation_palette.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace river_raid;
namespace data = assets::auxiliary_sprite;

constexpr std::uint32_t kSentinel = 0x7f123456U;
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 200;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Frame blank_frame() {
    return {kFrameWidth, kFrameHeight,
            std::vector<std::uint32_t>(kFrameWidth * kFrameHeight, kSentinel)};
}

AuxiliaryProjectileState visible_state(std::uint16_t vic_x = 24,
                                       std::uint8_t vic_y = 50) {
    AuxiliaryProjectileState state{};
    state.presentation = AuxiliaryProjectilePresentation{vic_x, vic_y};
    return state;
}

void test_roles_and_contact_mask() {
    require(data::sprite_style.individual_color == 15,
            "Auxiliary sprite individual color is not source color 15");
    require(!data::sprite_style.expand_horizontal && !data::expand_horizontal,
            "Auxiliary sprite unexpectedly uses horizontal expansion");

    const auto mask = make_auxiliary_projectile_contact_mask();
    require(mask.width == data::sprite_width && mask.height == data::sprite_height,
            "Auxiliary contact mask dimensions changed");
    require(mask.pixels.size() == data::sprite_roles.size(),
            "Auxiliary contact mask area changed");
    std::size_t occupied = 0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * mask.width + x;
            const bool expected = y == 9 && x >= 8 && x < 14;
            require(mask.pixels[index] == static_cast<std::uint8_t>(expected),
                    "Auxiliary contact mask differs from generated role alpha");
            occupied += mask.pixels[index] != 0;
        }
    }
    require(occupied == 6, "Auxiliary contact mask has more than six opaque pixels");
}

void test_pixels_and_empty_state() {
    auto actual = blank_frame();
    draw_auxiliary_projectile(actual, visible_state());
    const auto opaque = assets::presentation_palette[data::sprite_style.individual_color];
    std::size_t changed = 0;
    for (int y = 0; y < kFrameHeight; ++y) {
        for (int x = 0; x < kFrameWidth; ++x) {
            const auto pixel = actual.pixels[static_cast<std::size_t>(y) * kFrameWidth + x];
            const bool expected = y == 9 && x >= 8 && x < 14;
            require(pixel == (expected ? opaque : kSentinel),
                    "Auxiliary sprite pixels differ from six role-2 pixels");
            changed += pixel != kSentinel;
        }
    }
    require(changed == 6, "Auxiliary sprite was expanded or drew extra pixels");

    actual = blank_frame();
    const auto before = actual.pixels;
    draw_auxiliary_projectile(actual, {});
    require(actual.pixels == before, "Absent auxiliary sprite changed the frame");
}

void test_offscreen_and_hud_clips() {
    for (const auto state : {visible_state(0, 50), visible_state(344, 50),
                             visible_state(24, 41), visible_state(24, 211)}) {
        auto actual = blank_frame();
        draw_auxiliary_projectile(actual, state);
        require(actual.pixels == std::vector<std::uint32_t>(kFrameWidth * kFrameHeight, kSentinel),
                "Auxiliary sprite escaped the viewport or HUD clip");
    }
}

} // namespace

int main() {
    try {
        test_roles_and_contact_mask();
        test_pixels_and_empty_state();
        test_offscreen_and_hud_clips();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
