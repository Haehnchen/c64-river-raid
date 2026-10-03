#include "app/generator_setup.hpp"
#include "game/contact_sampling.hpp"
#include "game/shot_contact_resolution.hpp"
#include "fixtures/flight_contact_cases.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace river_raid;
    unsigned ordinary = 0, terrain = 0;
    try {
        for (const auto& fixture : test_data::flight_contact_cases) {
            const auto require = [&](bool condition, const char* field) {
                if (!condition) throw std::runtime_error(
                    "Flight contact " + std::to_string(fixture.index) + ": " + field);
            };
            const auto updated = update_player_shot({fixture.shot_input}, fixture.fire_input == 0);
            require(updated.state.vertical_position == fixture.shot_before, "shot update");
            require(updated.vertical_position_write.value_or(fixture.display_input) == fixture.display_before,
                    "shot update presentation");
            for (const auto pointer : fixture.pointers_before) {
                require(pointer == (updated.pose == PlayerShotPose::Visible ? 0xD5 : 0x40),
                        "shot update visibility");
            }
            WorldObjectState seed{};
            seed.records = fixture.before;
            constexpr std::array<std::uint8_t, 16> delays{}, clearance{};
            constexpr std::array<std::uint8_t, 8> choices{};
            RiverWorld world(make_river_generator(), WorldObjectHistory(seed, {delays, choices, clearance}),
                             generator_edge_patterns());
            ContactSamplingSnapshot snapshot{};
            for (unsigned i = 0; i < snapshot.sections.size(); ++i) {
                const auto& source = fixture.sections[i];
                snapshot.sections[i] = {{source.objects, source.terrain},
                    ShotObjectIndex{source.primary}, ShotObjectIndex{source.secondary}};
            }
            PlayerShotResult shot{{fixture.shot_before},
                fixture.pointers_before[0] == 0x40 ? PlayerShotPose::Hidden : PlayerShotPose::Visible,
                std::nullopt, std::nullopt};
            const auto result = resolve_shot_contacts(world, shot, make_shot_contact_slots(snapshot),
                                                     fixture.score_pending != 0);
            require(result.selection.outcome != ShotContactOutcome::Bridge,
                    "Fixture unexpectedly contains an unverified bridge transaction");
            ordinary += result.selection.outcome == ShotContactOutcome::Ordinary;
            terrain += result.selection.outcome == ShotContactOutcome::Terrain;
            require(world.object_state().records == fixture.after, "object mutation");
            require(shot.state.vertical_position == fixture.shot_after, "projectile state");
            require(shot.vertical_position_write.value_or(fixture.display_before) == fixture.display_after,
                    "projectile display position");
            for (unsigned i = 0; i < fixture.pointers_after.size(); ++i) {
                const auto expected = result.selection.clear_projectile ? 0x40 : fixture.pointers_before[i];
                require(expected == fixture.pointers_after[i], "projectile visibility");
            }
            require((result.object_effect ? result.object_effect->sound_countdown : fixture.sound_before)
                        == fixture.sound_after, "destruction sound request");
            const auto score = result.object_effect
                ? result.object_effect->score_kind.value_or(fixture.score_before) : fixture.score_before;
            require(score == fixture.score_after, "score request");
        }
        if (ordinary + terrain == 0) {
            throw std::runtime_error("Natural capture contains no supported projectile contact");
        }
        std::cout << test_data::flight_contact_cases.size() << " natural contact calls: "
                  << ordinary << " ordinary, " << terrain << " terrain\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
