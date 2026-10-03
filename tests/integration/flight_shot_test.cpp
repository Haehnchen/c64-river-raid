#include "app/flight_preview.hpp"
#include "app/shot_overlay.hpp"
#include "support/flight_start_helpers.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
river_raid::Frame blank() {
    return {320, 200, std::vector<std::uint32_t>(64000, 0xff010203)};
}
unsigned visible_pixels(const river_raid::Frame& frame) {
    unsigned count = 0;
    for (const auto value : frame.pixels) count += value != 0xff010203;
    return count;
}
}

int main() {
    using namespace river_raid;
    using namespace std::chrono_literals;
    try {
        FlightPreview flight;
        flight.set_controls({false, false, false, false, true});
        flight.advance_elapsed(1s);
        require(flight.shot().pose == PlayerShotPose::Hidden, "Inactive scene fired");
        flight.start();
        river_raid::test::wait_for_launch(flight);
        require(flight.advance_elapsed(20ms) == 1, "Expected first flight tick");
        const auto launched = flight.shot();
        require(launched.start_event.has_value() && launched.state.vertical_position == 192,
                "Launch was not connected after steering");
        auto image = blank();
        draw_shot_preview(image, flight.steering(), launched);
        require(visible_pixels(image) == 10 && image.pixels[142 * 320 + 160] != 0xff010203,
                "Original projectile mask/placement differs");
        auto shifted = blank();
        auto moved_player = flight.steering();
        ++moved_player.horizontal_position;
        draw_shot_preview(shifted, moved_player, launched);
        require(shifted.pixels[142 * 320 + 160] == 0xff010203 &&
                shifted.pixels[142 * 320 + 162] != 0xff010203,
                "Projectile did not follow current aircraft X");
        for (int tick = 0; tick < 36; ++tick) flight.advance_elapsed(20ms);
        require(flight.shot().pose == PlayerShotPose::Hidden, "Projectile failed to expire");
        flight.advance_elapsed(20ms);
        require(flight.shot().start_event.has_value(), "Held fire failed to repeat after expiry");
        flight.set_controls({});
        for (int tick = 0; tick < 40; ++tick) flight.advance_elapsed(20ms);
        require(flight.shot().pose == PlayerShotPose::Hidden, "Released fire launched another shot");
        image = blank();
        draw_shot_preview(image, flight.steering(), flight.shot());
        require(visible_pixels(image) == 0, "Hidden projectile changed image");
        const auto clipped = update_player_shot({53}, false);
        image = blank();
        draw_shot_preview(image, flight.steering(), clipped);
        require(visible_pixels(image) == 8, "Top clipping differs");
        image = blank();
        draw_shot_preview(image, {255, 0, 0, 0}, launched);
        require(visible_pixels(image) == 0, "Offscreen projectile changed image");
        flight.start();
        require(flight.shot().pose == PlayerShotPose::Hidden && !flight.shot().start_event,
                "Restart retained shot event");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
