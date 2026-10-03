#include "render/object_image.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_roles_and_palette_mapping() {
    constexpr std::array<std::uint8_t, 4> roles{0, 1, 2, 3};
    const auto image = river_raid::colorize_object_image(
        2, 2, roles, {4, 5, 6, false});
    require(image.width == 2 && image.height == 2, "Unexpected base dimensions");
    require((image.pixels == std::vector<std::uint8_t>{255, 5, 4, 6}),
            "Role-to-palette mapping is wrong");
}

void test_black_is_opaque_but_role_zero_is_transparent() {
    constexpr std::array<std::uint8_t, 4> roles{0, 1, 2, 3};
    const auto image = river_raid::colorize_object_image(
        4, 1, roles, {0, 0, 0, false});
    require((image.pixels == std::vector<std::uint8_t>{255, 0, 0, 0}),
            "Black palette pixels must remain opaque");
}

void test_style_does_not_change_geometry() {
    constexpr std::array<std::uint8_t, 6> roles{0, 1, 2, 3, 1, 2};
    const auto first = river_raid::colorize_object_image(
        3, 2, roles, {1, 2, 3, false});
    const auto second = river_raid::colorize_object_image(
        3, 2, roles, {15, 14, 13, false});
    require(first.width == second.width && first.height == second.height,
            "Style changed unexpanded geometry");
    require(first.pixels.size() == second.pixels.size(),
            "Style changed unexpanded pixel count");
    require(first.pixels[1] != second.pixels[1], "Style did not recolor role 1");
    require(first.pixels[2] != second.pixels[2], "Style did not recolor role 2");
}

void test_horizontal_expansion() {
    constexpr std::array<std::uint8_t, 2> roles{1, 0};
    const auto image = river_raid::colorize_object_image(
        2, 1, roles, {2, 7, 8, true});
    require(image.width == 4 && image.height == 1, "Expanded dimensions are wrong");
    require((image.pixels == std::vector<std::uint8_t>{7, 7, 255, 255}),
            "Expanded pixels were not duplicated horizontally");
}

void test_invalid_inputs() {
    constexpr std::array<std::uint8_t, 1> role{0};
    const auto expect_invalid = [](auto&& operation, const char* message) {
        bool rejected = false;
        try {
            operation();
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, message);
    };
    expect_invalid([&] { (void)river_raid::colorize_object_image(0, 1, role, {}); },
                   "Zero width was accepted");
    expect_invalid([&] { (void)river_raid::colorize_object_image(1, -1, role, {}); },
                   "Negative height was accepted");
    expect_invalid([&] { (void)river_raid::colorize_object_image(2, 1, role, {}); },
                   "Short role span was accepted");
    constexpr std::array<std::uint8_t, 1> bad_role{4};
    expect_invalid([&] {
        (void)river_raid::colorize_object_image(1, 1, bad_role, {});
    }, "Invalid role was accepted");
    expect_invalid([&] {
        (void)river_raid::colorize_object_image(1, 1, role, {16, 0, 0, false});
    }, "Invalid individual color was accepted");
    expect_invalid([&] {
        (void)river_raid::colorize_object_image(1, 1, role, {0, 16, 0, false});
    }, "Invalid first shared color was accepted");
    expect_invalid([&] {
        (void)river_raid::colorize_object_image(1, 1, role, {0, 0, 16, false});
    }, "Invalid second shared color was accepted");
}

} // namespace

int main() {
    try {
        test_roles_and_palette_mapping();
        test_black_is_opaque_but_role_zero_is_transparent();
        test_style_does_not_change_geometry();
        test_horizontal_expansion();
        test_invalid_inputs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
