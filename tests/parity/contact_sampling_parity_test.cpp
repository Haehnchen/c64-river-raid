#include "game/contact_sampling.hpp"
#include "fixtures/contact_sampling_cases.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace river_raid;
using namespace river_raid::test_data;

std::optional<ShotObjectIndex> object(std::uint8_t value) {
    return value == 255 ? std::nullopt : std::optional<ShotObjectIndex>{{value}};
}

ContactSamplingSnapshot snapshot(const ContactBufferObservation& observation, bool working) {
    ContactSamplingSnapshot result;
    for (std::size_t i = 0; i < result.sections.size(); ++i) {
        result.sections[i] = {
            {working ? observation.working_objects[i] : observation.consumed_objects[i],
             working ? observation.working_terrain[i] : observation.consumed_terrain[i]},
            object(working ? observation.working_primary[i] : observation.consumed_primary[i]),
            object(working ? observation.working_secondary[i] : observation.consumed_secondary[i])};
    }
    return result;
}

void fill(ContactSamplingBuffer& buffer, const ContactSamplingSnapshot& state) {
    for (std::uint8_t i = 0; i < state.sections.size(); ++i) {
        const auto& section = state.sections[i];
        buffer.replace_sample({i}, section.sample);
        buffer.replace_object_mapping({i}, ShotObjectRole::Primary, section.primary_object);
        buffer.replace_object_mapping({i}, ShotObjectRole::Secondary, section.secondary_object);
    }
}

ContactSamplingBuffer prepare(const ContactBufferObservation& before) {
    ContactSamplingBuffer buffer;
    fill(buffer, snapshot(before, false));
    buffer.rollover_for_foreground();
    fill(buffer, snapshot(before, true));
    return buffer;
}

void check(const ContactSamplingBuffer& buffer, const ContactBufferObservation& expected) {
    if (buffer.working_snapshot() != snapshot(expected, true) ||
        buffer.foreground_snapshot() != snapshot(expected, false)) {
        throw std::runtime_error("Measured contact-buffer state differs");
    }
}
}

int main() {
    try {
        std::size_t natural = 0;
        for (const auto& sample : contact_sampling_cases) {
            auto buffer = prepare(sample.before);
            buffer.rollover_for_foreground();
            check(buffer, sample.after);
            natural += sample.natural;
        }
        std::array<bool, 6> covered{};
        for (const auto& sample : contact_section_samples) {
            auto buffer = prepare(sample.before);
            buffer.replace_sample({sample.section}, {sample.object_mask, sample.terrain_mask});
            check(buffer, sample.after);
            covered.at(sample.section) = true;
        }
        for (std::size_t i = 1; i < covered.size(); ++i) {
            if (!covered[i]) throw std::runtime_error("Missing regular sample section");
        }
        if (natural != 64 || contact_sampling_cases.size() != 320 || contact_section_samples.size() != 64) {
            throw std::runtime_error("Incomplete buffer capture matrix");
        }
        std::cout << "320 original handoffs and 64 measured-input section updates match\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
