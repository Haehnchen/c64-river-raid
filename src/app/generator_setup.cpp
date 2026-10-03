#include "app/generator_setup.hpp"
#include "river_generator_data.hpp"
#include "session_presets.hpp"

#include <stdexcept>
#include <vector>

namespace river_raid {
namespace {
namespace data = assets::generator;

template <std::size_t Size>
auto feature_choices(const std::array<data::RiverFeatureChoice, Size>& source) {
    std::array<TerrainFeatureChoice, Size> result{};
    for (std::size_t i = 0; i < Size; ++i) {
        result[i] = {source[i].margin, source[i].stamp_index};
    }
    return result;
}

RiverGeneratorState initial_state(RiverStartPreset preset) {
    const auto& seed = data::river_generator_initial_state;
    RiverGeneratorState state{};
    state.phase = seed.phase;
    state.scroll_buffer = seed.buffer_index;
    state.section = preset.section;
    state.course = preset.anchor.course;
    state.channel_span = seed.channel_span;
    state.bank_inset = seed.bank_inset;
    state.random = preset.anchor.random;
    state.scripted_course = seed.scripted_course;
    switch (seed.channel_mode) {
    case 0: state.channel_mode = RiverChannelMode::Single; break;
    case 1: state.channel_mode = RiverChannelMode::PreparingSplit; break;
    case 2: state.channel_mode = RiverChannelMode::SplitOpening; break;
    case 3: state.channel_mode = RiverChannelMode::SplitClosing; break;
    default: throw std::invalid_argument("Unknown initial channel mode");
    }
    state.section_transition_pending = seed.section_transition_pending;
    state.transition_bias = seed.transition_bias;
    state.target_width_units = seed.target_width_units;
    state.previous_target_width_units = seed.previous_target_width_units;
    state.width_interval = seed.width_interval;
    state.section_interval = seed.section_interval;
    state.target_bank_inset = seed.target_bank_inset;
    state.bank_inset_step = seed.bank_inset_step;
    state.course_step = seed.course_step;
    state.bridge_rows_remaining = seed.bridge_rows_remaining;
    state.section_anchor = preset.anchor;
    state.previous_section_anchor = {{seed.previous_anchor_rng[0], seed.previous_anchor_rng[1],
                                      seed.previous_anchor_rng[2]}, seed.previous_anchor_course};
    return state;
}
} // namespace

RiverStartPreset river_start_preset(std::size_t preset_index) {
    const auto& source = assets::session_presets::presets.at(preset_index);
    return {{{source.anchor_random[0], source.anchor_random[1], source.anchor_random[2]},
             source.course}, source.section};
}

RiverGenerator make_river_generator(RiverStartPreset preset) {
    static const auto regular = feature_choices(data::river_regular_feature_choices);
    static const auto early = feature_choices(data::river_early_choices);
    static const auto stamps = [] {
        std::vector<TerrainStampPattern> result;
        const std::span bytes(data::river_stamp_data);
        for (std::size_t id = 0; id < data::river_stamp_spans.size(); ++id) {
            const auto& entry = data::river_stamp_spans[id];
            if (entry.height == 0) continue;
            const auto size = static_cast<std::size_t>(entry.width) * entry.height;
            if (entry.offset > bytes.size() || size > bytes.size() - entry.offset) {
                throw std::invalid_argument("Terrain stamp exceeds its asset data");
            }
            result.push_back({static_cast<std::uint8_t>(id), entry.width, entry.height,
                              bytes.subspan(entry.offset, size)});
        }
        return result;
    }();
    return {initial_state(preset), {data::river_course_deltas, data::river_bridge_lengths,
                             regular, early, stamps}};
}

RiverGenerator make_river_generator() { return make_river_generator(river_start_preset(0)); }

std::span<const std::uint8_t, 32> generator_edge_patterns() {
    return data::river_edge_patterns;
}

} // namespace river_raid
