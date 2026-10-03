#include "game/shot_contact_resolution.hpp"
#include "app/generator_setup.hpp"
#include "fixtures/shot_contact_cases.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace river_raid;
    using namespace river_raid::test_data;
    std::size_t effects = 0;
    for (const auto& fixture : shot_contact_cases) {
        const auto require = [&](bool ok, const char* message) {
            if (!ok) throw std::runtime_error("Case " + std::to_string(fixture.index) + ": " + message);
        };
        try {
            ShotContactSnapshot input{};
            input.object_kinds = fixture.snapshot.object_kinds;
            input.suppress_score_kind_write = fixture.snapshot.suppress_score_kind_write;
            for (std::size_t i = 0; i < input.slots.size(); ++i) {
                const auto& slot = fixture.snapshot.slots[i];
                input.slots[i] = {slot.terrain_contact, slot.object_contact,
                    slot.primary_present, slot.secondary_present,
                    {slot.primary_object}, {slot.secondary_object}};
            }
            const auto actual = select_shot_contact(input);
            const auto& expected = fixture.result;
            require(static_cast<unsigned>(actual.outcome) == static_cast<unsigned>(expected.outcome), "outcome");
            require(actual.slot.has_value() == expected.has_slot, "slot presence");
            if (actual.slot) require(actual.slot->value == expected.slot, "slot priority");
            require(actual.object.has_value() == expected.has_object, "object presence");
            if (actual.object) {
                require(actual.object->index.value == expected.object_index, "object mapping");
                require(static_cast<unsigned>(actual.object->role) == static_cast<unsigned>(expected.role), "role");
            }
            // Bridge fixtures stop at the branch entry, before shot clearing.
            if (actual.outcome == ShotContactOutcome::Bridge) {
                require(actual.clear_projectile, "Bridge failed to request shot clearing");
            } else {
                require(actual.clear_projectile == expected.clear_projectile, "clear contract");
            }
            require(actual.ordinary_effect.has_value() == expected.has_ordinary_effect, "effect presence");
            if (actual.ordinary_effect) {
                const auto& effect = *actual.ordinary_effect;
                require(effect.replacement_animated_kind == expected.replacement_animated_kind &&
                    effect.animation_count == expected.animation_count &&
                    effect.effect_countdown == expected.effect_countdown, "effect values");
                require(effect.score_kind_write.has_value() == expected.has_score_kind_write, "score gate");
                if (effect.score_kind_write) require(*effect.score_kind_write == expected.score_kind_write, "score kind");
            }
            if (!fixture.capture_effect) continue;
            ++effects;
            constexpr std::array<std::uint8_t, 16> delay{}, clearance{};
            constexpr std::array<std::uint8_t, 8> choices{};
            WorldObjectState seed{};
            for (std::size_t i = 0; i < seed.records.size(); ++i) {
                seed.records[i].animated_kind = input.object_kinds[i];
                seed.records[i].base_kind = input.object_kinds[i];
                seed.records[i].animation_count = fixture.pre_animation_counts[i];
            }
            RiverWorld world(make_river_generator(), WorldObjectHistory(seed, {delay, choices, clearance}),
                             generator_edge_patterns());
            auto shot = update_player_shot({}, true);
            const auto resolved = resolve_shot_contacts(world, shot, input.slots, input.suppress_score_kind_write);
            require(resolved.object_effect.has_value(), "ordinary resolution");
            auto expected_world = seed;
            for (std::size_t i = 0; i < seed.records.size(); ++i) {
                expected_world.records[i].animated_kind = fixture.post_object_kinds[i];
                expected_world.records[i].animation_count = fixture.post_animation_counts[i];
            }
            require(world.object_state() == expected_world, "world mutation");
            require(resolved.object_effect->sound_countdown == fixture.post_effect_countdown, "sound request");
            require(resolved.object_effect->score_kind.value_or(fixture.pre_score_kind) ==
                fixture.post_score_kind, "written/preserved score kind");
            require(shot.state.vertical_position == fixture.post_projectile_y &&
                shot.vertical_position_write == fixture.post_display_y &&
                shot.pose == PlayerShotPose::Hidden, "projectile removal");
            for (auto pointer : fixture.post_pointers) require(pointer == 0x40, "original hidden sprite");
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
    if (shot_contact_cases.size() != 356 || effects != 34) {
        std::cerr << "Incomplete original contact matrix\n";
        return 1;
    }
    std::cout << shot_contact_cases.size() << " original selections, " << effects << " ordinary effects matched\n";
}
