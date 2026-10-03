#include "app/contact_asset_setup.hpp"
#include "app/player_overlay.hpp"

#include "object_sprites.hpp"
#include "player_data.hpp"
#include "player_detail.hpp"
#include "presentation_palette.hpp"
#include "render/object_image.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::size_t colored_pixels(const Frame& frame, std::uint32_t background) {
    std::size_t count = 0;
    for (const auto pixel : frame.pixels) {
        if (pixel != background) ++count;
    }
    return count;
}

std::uint32_t pixel_at(const Frame& frame, int x, int y) {
    return frame.pixels[static_cast<std::size_t>(y * frame.width + x)];
}

void require_crash_image(LifeCycleImage image, std::size_t image_index,
                         const PlayerSteeringState& state, std::uint32_t background,
                         std::uint8_t individual_color) {
    namespace sprites = assets::object_sprites;
    const auto& source = sprites::sprite_images[image_index];
    const auto expected = colorize_object_image(source.width, source.height, source.roles,
                                                {individual_color, 7, 6, false});
    Frame actual{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(actual, state, FlightPose::Straight, image, individual_color);

    const int left = static_cast<int>(state.horizontal_position) * 2 - 24;
    const int top = 141;
    std::size_t opaque = 0;
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const auto expected_pixel = expected.pixels[static_cast<std::size_t>(
                y * expected.width + x)];
            const auto expected_rgb = expected_pixel == 255
                ? background : assets::presentation_palette[expected_pixel];
            require(pixel_at(actual, left + x, top + y) == expected_rgb,
                    "Crash image role, palette, or geometry changed");
            opaque += expected_pixel != 255;
        }
    }
    require(colored_pixels(actual, background) == opaque,
            "Crash image unexpectedly includes the player detail overlay or expansion");

    const auto mask = make_player_contact_mask(FlightPose::Straight, image);
    require(mask.width == source.width && mask.height == source.height &&
                mask.pixels.size() == source.roles.size(),
            "Crash contact mask dimensions changed");
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const auto role = source.roles[static_cast<std::size_t>(y * source.width + x)];
            require(mask.occupied_at(x, y) == (role != 0),
                    "Crash render and contact mask disagree");
        }
    }
}

std::uint8_t color_for_role(std::uint8_t role, std::uint8_t individual_color) {
    switch (role) {
    case 0: return assets::player::transparent_pixel;
    case 1: return assets::player::multicolor_1;
    case 2: return individual_color;
    case 3: return assets::player::multicolor_2;
    default: throw std::runtime_error("Player sprite contains an invalid C64 role");
    }
}

void require_normal_image(const Frame& actual, FlightPose pose,
                         const PlayerSteeringState& state, std::uint32_t background,
                         std::uint8_t individual_color) {
    namespace data = assets::player;
    const auto index = pose == FlightPose::Straight ? 0U : pose == FlightPose::Right ? 1U : 2U;
    const auto left = static_cast<int>(state.horizontal_position) * 2 - 24;
    const auto top = static_cast<int>(data::fixed_y);
    for (int y = 0; y < actual.height; ++y) {
        for (int x = 0; x < actual.width; ++x) {
            auto expected_rgb = background;
            const auto local_x = x - left;
            const auto local_y = y - top;
            if (local_x >= 0 && local_x < data::sprite_width &&
                local_y >= 0 && local_y < data::sprite_height) {
                const auto offset = static_cast<std::size_t>(local_y * data::sprite_width + local_x);
                auto color = color_for_role(data::sprite_roles[index][offset], individual_color);
                const auto detail = assets::player_detail::pixels[offset];
                if (detail != data::transparent_pixel) {
                    color = detail == data::individual_color ? individual_color : detail;
                }
                if (color != data::transparent_pixel) {
                    expected_rgb = assets::presentation_palette[color];
                }
            }
            require(pixel_at(actual, x, y) == expected_rgb,
                    "Player color, role mapping, or geometry changed");
        }
    }
}

void test_player_individual_colors() {
    namespace data = assets::player;
    constexpr std::uint32_t background = 0xff010203;
    const PlayerSteeringState start{86, 0, 0, 0x40};

    // Keep the pre-colored asset arrays byte-for-byte compatible with the
    // default rendering while exposing raw roles for the second player color.
    for (std::size_t pose_index = 0; pose_index < data::player_pixels.size(); ++pose_index) {
        for (std::size_t pixel_index = 0; pixel_index < data::player_pixels[pose_index].size();
             ++pixel_index) {
            require(data::player_pixels[pose_index][pixel_index] ==
                        color_for_role(data::sprite_roles[pose_index][pixel_index], 6),
                    "Default player pixels no longer match their source roles");
        }
    }

    for (const auto pose : {FlightPose::Straight, FlightPose::Right, FlightPose::Left}) {
        Frame player1{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
        Frame player2 = player1;
        draw_player_preview(player1, start, pose, LifeCycleImage::Straight, 6);
        draw_player_preview(player2, start, pose, LifeCycleImage::Straight, 7);
        require_normal_image(player1, pose, start, background, 6);
        require_normal_image(player2, pose, start, background, 7);

        for (std::size_t pixel = 0; pixel < player1.pixels.size(); ++pixel) {
            require((player1.pixels[pixel] != background) == (player2.pixels[pixel] != background),
                    "Player color changed the rendered aircraft geometry");
        }
        const auto mask = make_player_contact_mask(pose);
        const auto pose_index = pose == FlightPose::Straight ? 0U
            : pose == FlightPose::Right ? 1U : 2U;
        require(mask.width == data::sprite_width && mask.height == data::sprite_height,
                "Player color changed the contact mask dimensions");
        for (int y = 0; y < mask.height; ++y) {
            for (int x = 0; x < mask.width; ++x) {
                const auto offset = static_cast<std::size_t>(y * mask.width + x);
                require(mask.occupied_at(x, y) ==
                            (data::player_pixels[pose_index][offset] != data::transparent_pixel),
                        "Player color changed the contact mask pixels");
            }
        }
    }

    Frame default_color{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    Frame explicit_color{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(default_color, start, FlightPose::Straight);
    draw_player_preview(explicit_color, start, FlightPose::Straight,
                        LifeCycleImage::Straight, 6);
    require(default_color.pixels == explicit_color.pixels,
            "Default player color no longer matches explicit color 6");
}

void test_normal_poses_and_crash_frames() {
    constexpr std::uint32_t background = 0xff010203;
    const PlayerSteeringState start{86, 0, 0, 0x40};
    Frame straight{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(straight, start, FlightPose::Straight, LifeCycleImage::Straight);

    // The normal aircraft retains its source origin and detail overlay.
    require(pixel_at(straight, 160, 147) != background, "Straight pose is not visible");
    require(pixel_at(straight, 154, 146) == background, "Sprite was placed too high");
    require(pixel_at(straight, 154, 147) == background, "Sprite was placed too far left");
    require(pixel_at(straight, 148, 147) == background, "Transparent pixels overwrote the scene");
    require(colored_pixels(straight, background) == 86, "Combined aircraft pixel count changed");

    Frame right = straight;
    draw_player_preview(right, start, FlightPose::Right, LifeCycleImage::Straight);
    Frame left = straight;
    draw_player_preview(left, start, FlightPose::Left, LifeCycleImage::Straight);
    require(right.pixels != left.pixels, "Left and right poses are identical");

    // Source $D6/$D7 are shipped as object role images 20/21 and are rendered
    // as a single unexpanded 24x21 multicolor sprite with style {6,7,6}.
    for (const auto color : {std::uint8_t{6}, std::uint8_t{7}}) {
        require_crash_image(LifeCycleImage::ExplosionA, 20, start, background, color);
        require_crash_image(LifeCycleImage::ExplosionB, 21, start, background, color);
    }
}

void test_hidden_and_offscreen_states() {
    constexpr std::uint32_t background = 0xff010203;
    const PlayerSteeringState start{86, 0, 0, 0x40};
    Frame hidden{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(hidden, start, FlightPose::Straight, LifeCycleImage::Hidden);
    require(colored_pixels(hidden, background) == 0, "Hidden aircraft altered the frame");
    const auto hidden_mask = make_player_contact_mask(FlightPose::Straight, LifeCycleImage::Hidden);
    require(hidden_mask.width == 1 && hidden_mask.height == 1 &&
                hidden_mask.pixels.size() == 1 && !hidden_mask.occupied_at(0, 0),
            "Hidden aircraft retained a collision mask");

    Frame outside{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(outside, PlayerSteeringState{255, 0, 0, 0}, FlightPose::Straight,
                        LifeCycleImage::ExplosionA);
    require(colored_pixels(outside, background) == 0, "Offscreen aircraft altered the scene");

    Frame edge{320, 200, std::vector<std::uint32_t>(320 * 200, background)};
    draw_player_preview(edge, PlayerSteeringState{12, 0, 0, 0}, FlightPose::Right,
                        LifeCycleImage::Straight);
    require(colored_pixels(edge, background) != 0, "Left-edge sprite was clipped incorrectly");
}

void test_invalid_asset_selection() {
    Frame frame{320, 200, std::vector<std::uint32_t>(320 * 200, 0xff010203)};
    const auto before = frame.pixels;
    const auto invalid_pose = static_cast<FlightPose>(99);
    const auto invalid_image = static_cast<LifeCycleImage>(99);
    const auto rejects = [](auto operation) {
        try { operation(); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Invalid aircraft asset selection was silently substituted");
    };
    rejects([&] { draw_player_preview(frame, {}, invalid_pose); });
    rejects([&] { draw_player_preview(frame, {}, FlightPose::Straight, invalid_image); });
    rejects([&] { (void)make_player_contact_mask(invalid_pose); });
    rejects([&] { (void)make_player_contact_mask(FlightPose::Straight, invalid_image); });
    require(frame.pixels == before, "Invalid aircraft selection partially changed the frame");
}

} // namespace

int main() {
    try {
        test_normal_poses_and_crash_frames();
        test_player_individual_colors();
        test_hidden_and_offscreen_states();
        test_invalid_asset_selection();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
