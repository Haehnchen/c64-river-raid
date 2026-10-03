#include "render/contact_producer.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("Contact sampling bridge differs");
}
}

int main() {
    using namespace river_raid;
    try {
        const OccupancyMask pixel{1, 1, {1}};
        const std::array pair{ContactSpritePlacement{{&pixel, 0, 0}, 2},
                              ContactSpritePlacement{{&pixel, 0, 0}, 64}};
        const std::array single{ContactSpritePlacement{{&pixel, 0, 0}, 4}};
        PixelContactAccumulator producer;
        ContactSamplingBuffer buffer;
        buffer.replace_sample({3}, {128, 1});
        buffer.replace_object_mapping({3}, ShotObjectRole::Secondary, ShotObjectIndex{5});
        producer.scan(pair, {&pixel, 0, 0}, {0, 0, 1, 1});
        const auto before = buffer.working_snapshot();
        for (const auto section : {ContactSectionIndex{6}, ContactSectionIndex{255}}) {
            bool rejected = false;
            try { producer.sample_into(buffer, section); }
            catch (const std::out_of_range&) { rejected = true; }
            require(rejected && buffer.working_snapshot() == before);
            require(producer.pending_sample() == ContactSectionSample{66, 66});
        }
        bool rejected = false;
        try { producer.sample_into(buffer, {3}, static_cast<ContactSampleChannels>(99)); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && buffer.working_snapshot() == before);
        require(producer.pending_sample() == ContactSectionSample{66, 66});

        producer.sample_into(buffer, {3}, ContactSampleChannels::Objects);
        require(buffer.working_snapshot().sections[3].sample == ContactSectionSample{66, 1});
        require(producer.pending_sample() == ContactSectionSample{0, 66});
        producer.scan(single, {&pixel, 0, 0}, {0, 0, 1, 1});
        producer.sample_into(buffer, {3}, ContactSampleChannels::Terrain);
        require(buffer.working_snapshot().sections[3].sample == ContactSectionSample{66, 70});
        require(producer.pending_sample() == ContactSectionSample{});
        require(buffer.working_snapshot().sections[3].secondary_object == ShotObjectIndex{5});

        producer.scan(pair, {&pixel, 0, 0}, {0, 0, 1, 1});
        producer.publish_into(buffer);
        require(buffer.foreground_snapshot().sections[3].sample == ContactSectionSample{66, 70});
        require(buffer.foreground_snapshot().sections[3].secondary_object == ShotObjectIndex{5});
        require(buffer.working_snapshot() == ContactSamplingSnapshot{});
        require(producer.pending_sample() == ContactSectionSample{});
        for (std::uint8_t section = 0; section < 6; ++section) {
            producer.scan(pair, {&pixel, 0, 0}, {0, 0, 1, 1});
            producer.sample_into(buffer, {section});
            require(buffer.working_snapshot().sections[section].sample == ContactSectionSample{66, 66});
            require(producer.pending_sample() == ContactSectionSample{});
        }

        // Split object reads retain terrain until its later, separate read.
        producer.publish_into(buffer);
        buffer.replace_object_mapping({0}, ShotObjectRole::Secondary, ShotObjectIndex{7});
        buffer.replace_object_mapping({1}, ShotObjectRole::Secondary, ShotObjectIndex{3});
        producer.scan(pair, {&pixel, 0, 0}, {0, 0, 1, 1});
        producer.sample_into(buffer, {1}, ContactSampleChannels::Objects);
        require(buffer.working_snapshot().sections[1].sample == ContactSectionSample{66, 0});
        require(producer.pending_sample() == ContactSectionSample{0, 66});
        producer.scan(single, {&pixel, 0, 0}, {0, 0, 1, 1});
        producer.sample_into(buffer, {0}, ContactSampleChannels::Objects);
        require(buffer.working_snapshot().sections[0].sample == ContactSectionSample{});
        require(producer.pending_sample() == ContactSectionSample{0, 70});
        producer.sample_into(buffer, {0}, ContactSampleChannels::Terrain);
        producer.publish_into(buffer);
        require(buffer.foreground_snapshot().sections[1].sample == ContactSectionSample{66, 0});
        require(buffer.foreground_snapshot().sections[0].sample == ContactSectionSample{0, 70});
        require(buffer.foreground_snapshot().sections[0].secondary_object == ShotObjectIndex{7});
        require(buffer.foreground_snapshot().sections[1].secondary_object == ShotObjectIndex{3});
        require(producer.pending_sample() == ContactSectionSample{});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
