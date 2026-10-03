#include "app/generator_setup.hpp"
#include "app/world_setup.hpp"
#include "session_presets.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void test_all_presets_seed_and_restore_the_generator() {
    using namespace river_raid;
    const auto template_state = make_river_generator().state();
    for (std::size_t index = 0; index < assets::session_presets::presets.size(); ++index) {
        const auto& asset = assets::session_presets::presets[index];
        const auto preset = river_start_preset(index);
        const RiverSectionAnchor anchor{
            {asset.anchor_random[0], asset.anchor_random[1], asset.anchor_random[2]},
            asset.course};
        require(preset.anchor == anchor && preset.section == asset.section,
                "Session preset mapping differs from the exported asset");

        auto expected = template_state;
        expected.random = anchor.random;
        expected.course = anchor.course;
        expected.section = asset.section;
        expected.section_anchor = anchor;
        auto generator = make_river_generator(preset);
        require(generator.state() == expected,
                "Session preset changed the generator reset template beyond its start fields");

        for (int row = 0; row < 50; ++row) (void)generator.advance();
        generator.reset();
        require(generator.state() == expected, "Reset lost the selected session preset");
        const auto restore = generator.restore_checkpoint();
        require(restore.selection == RiverCheckpointSelection::Current,
                "Fresh session selected another player's or section's checkpoint");
        require(generator.state().section == preset.section &&
                    generator.state().section_anchor == preset.anchor &&
                    generator.state().random == preset.anchor.random &&
                    generator.state().course == preset.anchor.course,
                "Rebuild lost the selected section anchor");

        auto world = make_river_world(preset);
        require(world.river_state() == expected,
                "World factory did not use the selected session preset");
        (void)world.advance();
        world.reset();
        require(world.river_state() == expected && world.generated_rows() == 0,
                "World reset lost the selected session preset");
        (void)world.restore_checkpoint();
        require(world.river_state().section_anchor == preset.anchor &&
                    world.river_state().section == preset.section,
                "World rebuild lost the selected section preset");
    }
}

void test_default_factories_equal_game_one() {
    using namespace river_raid;
    const auto game_one = river_start_preset(0);
    auto implicit_generator = make_river_generator();
    auto explicit_generator = make_river_generator(game_one);
    require(implicit_generator.state() == explicit_generator.state(),
            "Default generator differs from Game 1");
    auto implicit_world = make_river_world();
    auto explicit_world = make_river_world(game_one);
    require(implicit_world.river_state() == explicit_world.river_state(),
            "Default world differs from Game 1");
    for (int row = 0; row < 64; ++row) {
        (void)implicit_generator.advance();
        (void)explicit_generator.advance();
        require(implicit_generator.state() == explicit_generator.state(),
                "Default generator states differ from Game 1");
        require(implicit_world.advance() == explicit_world.advance(),
                "Default world rows differ from Game 1");
    }
}

void test_invalid_preset_index_is_rejected() {
    try {
        (void)river_raid::river_start_preset(river_raid::assets::session_presets::presets.size());
    } catch (const std::out_of_range&) {
        return;
    }
    throw std::runtime_error("Out-of-range session preset index was accepted");
}

} // namespace

int main() {
    try {
        test_all_presets_seed_and_restore_the_generator();
        test_default_factories_equal_game_one();
        test_invalid_preset_index_is_rejected();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
