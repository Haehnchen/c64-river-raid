#include "app/flight_preview.hpp"
#include "app/flight_preview_contacts.hpp"
#include "app/world_setup.hpp"
#include "game/player_shot.hpp"
#include "game/player_steering.hpp"
#include "game/score.hpp"
#include "game/shot_contact_resolution.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

using namespace river_raid;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_values() {
    constexpr std::array<std::uint32_t, 17> expected{
        0, 0, 0, 0, 0, 0, 0, 100, 60, 60, 150, 150, 60, 30, 500, 80, 750};
    for (unsigned kind = 0; kind < 256; ++kind) {
        require(object_score(static_cast<std::uint8_t>(kind)) ==
                    (kind < expected.size() ? expected[kind] : 0),
                "Wrong object point value");
    }
    const auto below = award_object_score(9960, 13);
    require(below.total == 9990 && !below.extra_life && !below.exhausted,
            "Bonus awarded before threshold");
    const auto crossing = award_object_score(9970, 13);
    require(crossing.total == 10000 && crossing.extra_life && !crossing.exhausted,
            "Missing threshold bonus");
    const auto joint_crossing = award_object_score(9700, 16);
    require(joint_crossing.total == 10450 && joint_crossing.extra_life &&
                !joint_crossing.exhausted,
            "Joint bridge award missed 750 points or its threshold bonus");
    const auto rollover = award_object_score(99970, 13);
    require(rollover.total == 100000 && rollover.extra_life,
            "Missing bonus across decimal carry");
    const auto exact_max = award_object_score(9999960, 13);
    require(exact_max.total == kMaximumScore && !exact_max.exhausted,
            "Exact maximum incorrectly ended round");
    const auto overflow = award_object_score(kMaximumScore, 13);
    require(overflow.total == kMaximumScore && overflow.exhausted && !overflow.extra_life,
            "Score overflow did not saturate/end round");
    for (std::uint32_t threshold = 10000; threshold < kMaximumScore; threshold += 10000) {
        const auto award = award_object_score(threshold - 20, 13);
        require(award.total == threshold + 10 && award.extra_life && !award.exhausted,
                "Bonus crossing failed at a higher score");
        require(!award_object_score(threshold, 13).extra_life,
                "Bonus repeated after crossing");
    }
}

void test_bridge_score_write_order() {
    for (const bool joint : {false, true}) {
        for (const bool pending : {false, true}) {
            const auto write = bridge_score_kind_write(joint, pending);
            require(write.has_value() == joint,
                    "Non-joint bridge contact changed the pending score latch");
            if (joint) require(*write == (pending ? 0 : 16),
                               "Joint bridge score write differs from round-event priority");
            for (std::uint8_t prior = 0; prior <= 16; ++prior) {
                auto score_kind = prior;
                if (write) score_kind = *write;
                const auto expected = joint ? (pending ? 0U : 750U) : object_score(prior);
                const auto award = award_object_score(9970, score_kind);
                require(award.total == 9970 + expected &&
                            award.extra_life == (expected >= 30) && !award.exhausted,
                        "Bridge score write lost ordering or granted a cancelled reserve");
            }
        }
    }
    require(bridge_score_kind_write(true, true).has_value() &&
                *bridge_score_kind_write(true, true) == 0,
            "Cancelled score must write zero, not suppress the write");
}

void test_generated_joint_bridge_award() {
    auto world = make_river_world();
    std::uint8_t animation_phase = 0;
    for (int row = 0; row < 1105; ++row) {
        world.advance_motion({animation_phase, world.river_state().section >= 2, std::nullopt});
        world.advance_bridge_sprites(animation_phase, true);
        ++animation_phase;
        (void)world.advance();
    }

    auto player = FlightPreview{}.steering();
    for (int tick = 0; tick < 18; ++tick) {
        player = steer_player(player, {false, false, true, false, false}).state;
    }
    auto shot = update_player_shot({}, true);
    for (int tick = 0; tick < 20; ++tick) shot = update_player_shot(shot.state, false);
    const auto contacts = sample_flight_preview_shot_contacts(world, player, shot);
    const auto resolution = resolve_shot_contacts(world, shot, contacts, false);
    require(resolution.selection.outcome == ShotContactOutcome::Bridge &&
                resolution.bridge_sprite_contact && resolution.object_effect &&
                resolution.object_effect->score_kind == std::optional<std::uint8_t>{16},
            "Generated joint bridge contact did not produce the special score index");
    const auto award = award_object_score(9700, *resolution.object_effect->score_kind);
    require(award.total == 10450 && award.extra_life && !award.exhausted,
            "Generated joint bridge contact did not award 750 points and a reserve");
}

std::uint32_t play_round(FlightPreview& flight) {
    flight.start();
    require(flight.score() == 0, "New game retained score");
    flight.set_controls({false, false, false, false, true});
    bool scored = false;
    bool retained_on_respawn = false;
    for (int slice = 0; slice < 20000 && flight.running(); ++slice) {
        const auto before_score = flight.score();
        const auto before_hits = flight.ordinary_hits();
        const auto before_bridge = flight.bridge_number();
        const auto before_status = flight.status();
        require(flight.advance_elapsed(10ms) <= 1, "Score probe advanced multiple ticks");
        const auto hits = flight.ordinary_hits() - before_hits;
        const auto points = flight.score() - before_score;
        require(hits <= 1, "One tick scored multiple hits");
        if (hits == 0) {
            require(points == 0 || ((points == 500 || points == 750) &&
                                    flight.bridge_number() != before_bridge),
                    "Score changed without an ordinary or bridge hit");
        } else {
            require(points == 30 || points == 60 || points == 80 ||
                        points == 100 || points == 150,
                    "Ordinary hit awarded wrong points");
            scored = true;
        }
        if (before_status == FlightStatus::Crashed &&
            flight.status() == FlightStatus::Flying && before_score != 0) {
            require(flight.score() == before_score, "Respawn cleared score");
            retained_on_respawn = true;
        }
    }
    require(!flight.running() && scored && retained_on_respawn,
            "Natural round missed score/respawn coverage");
    const auto total = flight.score();
    const auto terminal_rows = flight.world().generated_rows();
    const auto terminal_fuel = flight.fuel();
    const auto terminal_ticks = flight.advance_elapsed(1s);
    require(terminal_ticks != 0 && flight.score() == total && flight.fuel() == terminal_fuel &&
                flight.world().generated_rows() == terminal_rows &&
                flight.idle_state().phase == SessionIdlePhase::Waiting &&
                flight.idle_state().countdown < 0xFF,
            "Terminal foreground failed to idle while preserving gameplay state");
    return total;
}

} // namespace

int main() {
    try {
        test_values();
        test_bridge_score_write_order();
        test_generated_joint_bridge_award();
        FlightPreview flight;
        require(flight.score() == 0, "Ready score was not zero");
        const auto first = play_round(flight);
        require(first != 0 && flight.high_score() == first,
                "Terminal score did not update the session record");
        require(play_round(flight) == first, "Restart changed score sequence");
        flight.start(1);
        require(flight.high_score() == first && flight.score() == 0,
                "Paired mode did not retain its record independently of current score");
        flight.start(2);
        require(flight.high_score() == 0, "Another preset inherited the wrong record");
        flight.start(0);
        require(flight.high_score() == first, "Restart cleared the in-process record");
        FlightPreview cold;
        require(cold.high_score() == 0, "A new session inherited unrelated records");
        std::cout << "Score values and natural round passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
