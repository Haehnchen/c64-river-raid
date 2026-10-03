#include "game/world_objects.hpp"
#include "app/world_setup.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    using namespace river_raid;
    try {
        constexpr std::array<std::uint8_t, 16> delays{}, clearance{};
        constexpr std::array<std::uint8_t, 8> choices{};
        for (unsigned kind = 0; kind < 16; ++kind) {
            for (const auto slot : {WorldObjectHitSlot::Primary, WorldObjectHitSlot::Alternate}) {
                for (const bool pending : {false, true}) {
                    WorldObjectState seed{};
                    seed.active_begin = 0;
                    seed.records[4] = {80, 86, 0x48, static_cast<std::uint8_t>(kind),
                                      static_cast<std::uint8_t>(kind), 30, 130, 99};
                    WorldObjectHistory history(seed, {delays, choices, clearance});
                    const auto effect = history.apply_projectile_hit(4, slot, pending);
                    if (kind < 7) {
                        require(!effect && history.state() == seed, "Nondestructible hit changed state");
                        continue;
                    }
                    auto expected = seed;
                    expected.records[4].animated_kind = slot == WorldObjectHitSlot::Primary ? 2 : 4;
                    expected.records[4].animation_count = 6;
                    require(effect && history.state() == expected, "Hit altered unrelated world fields");
                    require(effect->destroyed_kind == kind && effect->record_index == 4 &&
                            effect->sound_countdown == 31, "Hit event differs");
                    require(effect->score_kind == (pending ? std::nullopt : std::optional<std::uint8_t>{kind}),
                            "Pending round event failed to suppress score request");
                    require(!history.apply_projectile_hit(4, slot, pending), "Explosion was hit again");
                    for (unsigned transition = 0; transition < 6; ++transition) {
                        history.advance_motion({static_cast<std::uint8_t>(transition * 8), false, std::nullopt});
                        require(history.state().records[4].animation_count == 5 - transition,
                                "Explosion lifetime differs");
                    }
                    require(history.state().records[4].animated_kind == 0 &&
                            history.state().records[4].base_kind == kind, "Explosion did not expire cleanly");
                    history.reset();
                    require(history.state() == seed, "Reset retained destruction");
                    require(!history.apply_projectile_hit(12, slot, false) && history.state() == seed,
                            "Invalid record changed world");
                }
            }
        }
        auto world = make_river_world();
        bool exercised = false;
        for (unsigned row = 0; row < 512 && !exercised; ++row) {
            (void)world.advance();
            for (std::size_t index = 0; index < kWorldObjectCapacity; ++index) {
                const auto kind = world.object_state().records[index].animated_kind;
                if (kind >= 7 && kind != 14) {
                    const auto rows_before = world.generated_rows();
                    const auto effect = world.apply_projectile_hit(index, WorldObjectHitSlot::Primary, false);
                    require(effect && world.object_state().records[index].animated_kind == 2 &&
                            world.generated_rows() == rows_before, "RiverWorld hit handoff failed");
                    exercised = true;
                    break;
                }
            }
        }
        require(exercised, "No generated ordinary object tested");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
