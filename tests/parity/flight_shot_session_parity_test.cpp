#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "fixtures/flight_contact_cases.hpp"
#include "game/score.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace river_raid;
    using namespace std::chrono_literals;
    try {
        FlightPreview flight;
        flight.start();
        test::wait_for_launch(flight);
        static_assert(test_data::flight_contact_pre_shot_phases.size() ==
                      test_data::flight_contact_cases.size());
        const auto initial_rows = flight.world().generated_rows();
        std::size_t expected_rows = 0;
        // Align first active shot update; fixture values are expectations only.
        flight.set_controls({false, false, false, false, true});
        std::uint8_t displayed_y = 0;
        std::uint32_t expected_score = 0;
        unsigned launches = 0;
        for (const auto& original : test_data::flight_contact_cases) {
            const auto require = [&](bool condition, const char* field) {
                if (!condition) throw std::runtime_error(
                    "Native shot session " + std::to_string(original.index) + ": " + field);
            };
            std::size_t admitted = 0;
            for (unsigned slice = 0; slice < 21 && admitted == 0; ++slice)
                admitted = flight.advance_elapsed(1ms);
            require(admitted == 1, "failed to admit exactly one foreground pass");
            require(flight.status() == FlightStatus::Flying && flight.lives() == 3,
                    "held-fire route left active flight");
            require(original.fire_input == 0, "original fixture no longer holds fire");
            const auto& shot = flight.shot();
            require(shot.state.vertical_position == original.shot_after, "shot position");
            displayed_y = shot.vertical_position_write.value_or(displayed_y);
            require(displayed_y == original.display_after, "retained display position");
            for (const auto pointer : original.pointers_after) {
                require(pointer == 0xd5 || pointer == 0x40, "unknown original shot image");
                require((pointer == 0xd5) == (shot.pose == PlayerShotPose::Visible),
                        "shot visibility");
            }
            const bool expected_launch = original.shot_input == 0;
            require(shot.start_event.has_value() == expected_launch, "launch event");
            launches += shot.start_event.has_value();
            require(flight.audio_state().explosion_countdown == original.sound_after,
                    "explosion request/countdown");
            expected_score += object_score(original.score_after);
            require(flight.score() == expected_score, "committed score");
            if (original.index + 1 < test_data::flight_contact_pre_shot_phases.size()) {
                const auto current = test_data::flight_contact_pre_shot_phases[original.index];
                const auto next = test_data::flight_contact_pre_shot_phases[original.index + 1];
                const auto rows = (next.generator_row_phase + 32U - current.generator_row_phase) % 32U;
                require(rows <= 2, "original interval exceeds two scroll attempts");
                expected_rows += rows;
                // Next pre-shot includes this pass's rows but another motion call.
                require(flight.world().river_state().phase == next.generator_row_phase,
                        "row phase at next original pre-shot boundary");
                require(flight.world().object_state().alternating_update ==
                            (next.history_alternating_parity != 0), "history parity");
                require(flight.world().generated_rows() - initial_rows == expected_rows,
                        "cumulative row advance");
            }
        }
        if (launches != 7 || expected_score != 30 || flight.ordinary_hits() != 1)
            throw std::runtime_error("Bounded original shot-session coverage changed");
        std::cout << "240 shot passes and 239 row/history boundaries match original fields\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
