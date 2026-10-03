#include "game/river_row.hpp"

#include <cstddef>
#include <stdexcept>

namespace river_raid {
namespace {
std::uint8_t first_edge(const RiverRowInput& input) {
    return static_cast<std::uint8_t>(input.course - 16 +
                                    (input.split_channel ? 0 : input.bank_inset));
}

class RowBuilder {
public:
    RowBuilder(std::span<const std::uint8_t, 32> edges, bool bridge)
        : edges_(edges), land_(bridge ? 0xff : 0xaa), bridge_(bridge) {}

    void fill(int count, std::uint8_t pattern) {
        if (count > static_cast<int>(row_.size()) - written_) {
            throw std::invalid_argument("River geometry exceeds row width");
        }
        for (int i = 0; i < count; ++i) {
            row_[static_cast<std::size_t>(written_++)] = pattern;
        }
    }

    void land(int count) { fill(count, land_); }
    void water_to(int coordinate) { fill(coordinate / 4 - written_, 0x55); }
    void edge(int coordinate, bool outgoing, bool bridge) {
        const auto index = static_cast<std::size_t>(
            (coordinate & 3) * 4 + (outgoing ? 2 : 0) + (bridge ? 16 : 0));
        fill(1, edges_[index]);
        fill(1, edges_[index + 1]);
    }
    void outer_edge(int coordinate, bool outgoing) { edge(coordinate, outgoing, bridge_); }
    void stamp(std::span<const std::uint8_t> pixels) {
        for (const auto value : pixels) fill(1, value);
    }
    int written() const { return written_; }
    PackedRiverRow finish() {
        land(static_cast<int>(row_.size()) - written_);
        return row_;
    }

private:
    std::span<const std::uint8_t, 32> edges_;
    PackedRiverRow row_{};
    int written_{};
    std::uint8_t land_{};
    bool bridge_{};
};
} // namespace

RiverGuides river_guides(const RiverRowInput& input) {
    const auto guide = [](int coordinate, bool outgoing) {
        return static_cast<std::uint8_t>(coordinate + (coordinate & 3) + (outgoing ? -2 : 10));
    };
    int edge = first_edge(input);
    RiverGuides result{guide(edge, false), 0, 0, 0};
    if (input.split_channel && input.bank_inset >= 6) {
        edge += input.channel_span / 2;
        result.first_right = guide(edge, true);
        edge += 2 * input.bank_inset;
        result.second_left = guide(edge, false);
        edge += input.channel_span / 2;
        result.second_right = guide(edge, true);
    } else {
        edge += input.channel_span;
        if (input.split_channel) edge += 2 * input.bank_inset;
        result.first_right = guide(edge, true);
    }
    return result;
}

PackedRiverRow compose_river_row(const RiverRowInput& input,
                                std::span<const std::uint8_t, 32> edge_patterns) {
    const auto& stamp = input.stamp;
    if (stamp.pixels.size() > 40 || (stamp.placement != StampPlacement::None && stamp.pixels.empty())) {
        throw std::invalid_argument("Invalid terrain stamp row");
    }
    if (stamp.placement == StampPlacement::Island && !input.split_channel) {
        throw std::invalid_argument("Island stamp requires split channel");
    }
    if (input.split_channel && (stamp.placement == StampPlacement::LeftBank ||
                                stamp.placement == StampPlacement::RightBank)) {
        throw std::invalid_argument("Bank stamp cannot be used on split channel");
    }
    RowBuilder row(edge_patterns, input.bridge);
    int edge = first_edge(input);
    if (edge < 0 || edge >= 160) throw std::invalid_argument("First river edge outside view");

    if (stamp.placement == StampPlacement::LeftBank) {
        row.land(stamp.margin);
        row.stamp(stamp.pixels);
    }
    row.land(edge / 4 - 1 - row.written());
    row.outer_edge(edge, false);

    if (input.split_channel && input.bank_inset >= 6) {
        edge += input.channel_span / 2;
        row.water_to(edge);
        row.edge(edge, true, input.bank_inset < 8);
        edge += 2 * input.bank_inset;
        const int island_end = edge / 4 - 1;
        if (input.bank_inset < 8) {
            row.fill(island_end - row.written(), 0xff);
        } else {
            if (stamp.placement == StampPlacement::Island) {
                row.land(stamp.margin - row.written());
                row.stamp(stamp.pixels);
            }
            row.land(island_end - row.written());
        }
        row.edge(edge, false, input.bank_inset < 8);
        edge += input.channel_span / 2;
    } else {
        edge += input.channel_span;
        if (input.split_channel) edge += 2 * input.bank_inset;
    }
    if (edge >= 160) throw std::invalid_argument("Last river edge outside view");
    row.water_to(edge);
    row.outer_edge(edge, true);
    if (stamp.placement == StampPlacement::RightBank) {
        const int trailing = static_cast<int>(stamp.margin) - 1;
        row.land(40 - row.written() - trailing - static_cast<int>(stamp.pixels.size()));
        row.stamp(stamp.pixels);
    }
    return row.finish();
}

} // namespace river_raid
