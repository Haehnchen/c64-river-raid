#include "app/bridge_overlay.hpp"
#include "bridge_sprites.hpp"
#include "presentation_palette.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace river_raid;
namespace data = assets::bridge_sprites;

constexpr std::uint32_t kSentinel = 0x7f123456U;
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 200;
constexpr int kRiverTop = 4;
constexpr int kRiverBottom = kRiverTop + 157;

constexpr std::array all_images{
    BridgeSpriteImage::CrossingRightA,
    BridgeSpriteImage::CrossingRightB,
    BridgeSpriteImage::CrossingLeftA,
    BridgeSpriteImage::CrossingLeftB,
    BridgeSpriteImage::GapFlight,
    BridgeSpriteImage::GapExplosion0,
    BridgeSpriteImage::GapExplosion1,
    BridgeSpriteImage::GapExplosion2,
    BridgeSpriteImage::GapExplosion3,
    BridgeSpriteImage::GapExplosion4,
    BridgeSpriteImage::JointExplosionA,
    BridgeSpriteImage::JointExplosionB,
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Frame blank_frame() {
    return {kFrameWidth, kFrameHeight,
            std::vector<std::uint32_t>(kFrameWidth * kFrameHeight, kSentinel)};
}

std::uint8_t image_color(std::size_t index) {
    return index >= 4 && index < 10 ? 1 : 0;
}

BridgeSpritePresentation presentation(BridgeSpriteImage image, std::uint16_t vic_x,
                                       std::uint8_t vic_y) {
    return {image, vic_x, vic_y, image_color(static_cast<std::size_t>(image))};
}

void expected_sprite(Frame& frame, const BridgeSpritePresentation& sprite) {
    const auto index = static_cast<std::size_t>(sprite.image);
    const auto& source = data::images[index];
    const auto& style = data::image_styles[index];
    const int left = static_cast<int>(sprite.vic_x) - kBridgeViewportOriginX;
    const int top = static_cast<int>(sprite.vic_y) - kBridgeViewportOriginY;
    for (int y = 0; y < data::sprite_height; ++y) {
        for (int x = 0; x < data::sprite_width; ++x) {
            const auto role = source.roles[static_cast<std::size_t>(y) * data::sprite_width + x];
            if (role == data::transparent_role) continue;
            const int destination_x = left + x;
            const int destination_y = top + y;
            if (destination_x < 0 || destination_x >= kFrameWidth ||
                destination_y < kRiverTop || destination_y >= kRiverBottom) {
                continue;
            }
            const auto color = role == 1 ? style.multicolor_1
                             : role == 2 ? sprite.color
                                         : style.multicolor_2;
            frame.pixels[static_cast<std::size_t>(destination_y) * kFrameWidth + destination_x] =
                assets::presentation_palette[color];
        }
    }
}

BridgeSpriteState single_sprite_state(const BridgeSpritePresentation& sprite) {
    BridgeSpriteState state{};
    if (static_cast<std::size_t>(sprite.image) < 4 ||
        static_cast<std::size_t>(sprite.image) >= 10) {
        state.crossing = sprite;
    } else {
        state.gap = sprite;
    }
    return state;
}

void test_masks_and_all_semantic_images() {
    for (const auto image : all_images) {
        const auto index = static_cast<std::size_t>(image);
        const auto mask = make_bridge_sprite_contact_mask(image);
        require(mask.width == data::sprite_width && mask.height == data::sprite_height,
                "Bridge contact mask dimensions changed");
        require(mask.pixels.size() == data::images[index].roles.size(),
                "Bridge contact mask area changed");
        for (std::size_t pixel = 0; pixel < mask.pixels.size(); ++pixel) {
            require(mask.pixels[pixel] ==
                        static_cast<std::uint8_t>(data::images[index].roles[pixel] != 0),
                    "Bridge contact mask differs from generated alpha");
        }

        const auto sprite = presentation(image, 96, 80);
        auto actual = blank_frame();
        auto expected = blank_frame();
        expected_sprite(expected, sprite);
        draw_bridge_sprites(actual, single_sprite_state(sprite));
        require(actual.pixels == expected.pixels,
                "Bridge sprite pixels differ from generated roles and style");
    }
}

void test_blank_state_and_clip() {
    auto actual = blank_frame();
    const auto before = actual.pixels;
    draw_bridge_sprites(actual, {});
    require(actual.pixels == before, "Blank bridge sprite state changed the frame");

    const std::array clipped_sprites{
        presentation(BridgeSpriteImage::CrossingRightA, 20, 51),
        presentation(BridgeSpriteImage::GapExplosion4, 324, 195),
    };
    for (const auto& sprite : clipped_sprites) {
        actual = blank_frame();
        auto expected = blank_frame();
        expected_sprite(expected, sprite);
        draw_bridge_sprites(actual, single_sprite_state(sprite));
        require(actual.pixels == expected.pixels,
                "Bridge sprite did not use the fixed river clip");
        for (int y = 0; y < kFrameHeight; ++y) {
            if (y >= kRiverTop && y < kRiverBottom) continue;
            for (int x = 0; x < kFrameWidth; ++x) {
                require(actual.pixels[static_cast<std::size_t>(y) * kFrameWidth + x] == kSentinel,
                        "Bridge sprite wrote outside the river clip or into the HUD");
            }
        }
    }
}

void test_gap_has_layer_priority_over_crossing() {
    const auto crossing = presentation(BridgeSpriteImage::CrossingRightA, 96, 80);
    const auto gap = presentation(BridgeSpriteImage::GapFlight, 96, 80);
    BridgeSpriteState state{};
    state.crossing = crossing;
    state.gap = gap;

    auto actual = blank_frame();
    auto expected = blank_frame();
    auto crossing_only = blank_frame();
    expected_sprite(expected, crossing);
    expected_sprite(expected, gap);
    expected_sprite(crossing_only, crossing);
    draw_bridge_sprites(actual, state);
    require(actual.pixels == expected.pixels,
            "Bridge gap/crossing layer composition differs from generated assets");

    std::size_t gap_overwrites = 0;
    const auto& crossing_source = data::images[static_cast<std::size_t>(crossing.image)];
    const auto& gap_source = data::images[static_cast<std::size_t>(gap.image)];
    for (int y = 0; y < data::sprite_height; ++y) {
        for (int x = 0; x < data::sprite_width; ++x) {
            const auto offset = static_cast<std::size_t>(y) * data::sprite_width + x;
            if (crossing_source.roles[offset] != 0 && gap_source.roles[offset] != 0) {
                const auto destination = static_cast<std::size_t>(y + 30) * kFrameWidth + 72 + x;
                if (actual.pixels[destination] != crossing_only.pixels[destination]) ++gap_overwrites;
            }
        }
    }
    require(gap_overwrites > 0, "Bridge gap test had no overlapping opaque pixels");
}

} // namespace

int main() {
    try {
        test_masks_and_all_semantic_images();
        test_blank_state_and_clip();
        test_gap_has_layer_priority_over_crossing();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
