#include "game/fuel.hpp"

#include <iostream>
#include <stdexcept>

namespace {

using namespace river_raid;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_initial_and_drain_boundaries() {
    require(FuelState{}.amount == kInitialFuel && kInitialFuel == kFullFuel,
            "Fuel did not start full");

    const auto first = consume_fuel({kInitialFuel});
    require(first.state.amount == 0xFFDF && !first.exhausted,
            "First fuel drain differs");

    const auto borrow = consume_fuel({0x010F});
    require(borrow.state.amount == 0x00EF && !borrow.exhausted,
            "Fuel drain did not borrow from the high byte");

    const auto exact = consume_fuel({kFuelDrainPerTick});
    require(exact.state.amount == kEmptyFuel && !exact.exhausted,
            "Exact final drain exhausted one tick early");

    const auto empty = consume_fuel(exact.state);
    require(empty.state.amount == kEmptyFuel && empty.exhausted,
            "Empty tank did not exhaust on the next drain");

    const auto partial = consume_fuel({kFuelDrainPerTick - 1});
    require(partial.state.amount == kFuelDrainPerTick - 1 && partial.exhausted,
            "Unpayable drain overwrote the retained partial tank");
    for (unsigned amount = 0; amount <= kFullFuel; ++amount) {
        const auto result = consume_fuel({static_cast<std::uint16_t>(amount)});
        const bool exhausted = amount < kFuelDrainPerTick;
        require(result.exhausted == exhausted && result.state.amount ==
                    (exhausted ? amount : amount - kFuelDrainPerTick),
                "Fuel subtraction or retained underflow differs");
    }
}

void test_refuel_boundaries() {
    require(refuel_fuel({0x40A5}).amount == 0x43A5,
            "Refuel did not add three high-byte units");
    require(refuel_fuel({0xFC7E}).amount == 0xFF7E,
            "Refuel changed the low byte before overflow");

    for (const auto amount : {std::uint16_t{0xFD00}, std::uint16_t{0xFE80},
                              std::uint16_t{0xFFFF}}) {
        require(refuel_fuel({amount}).amount == kFullFuel,
                "Overflowing refuel did not cap at full");
    }
}

void test_original_operation_order() {
    auto fuel = FuelState{0x4000};
    const auto consumed = consume_fuel(fuel);
    require(!consumed.exhausted && consumed.state.amount == 0x3FE0,
            "Contact tick did not drain first");
    fuel = refuel_fuel(consumed.state);
    require(fuel.amount == 0x42E0,
            "Contact tick did not refill after draining");

    fuel = {};
    for (int tick = 0; tick < 2047; ++tick) {
        const auto drained = consume_fuel(fuel);
        require(!drained.exhausted, "Full tank exhausted before 2047 payable drains");
        fuel = drained.state;
    }
    require(fuel.amount == 31 && consume_fuel(fuel).exhausted,
            "Full tank did not exhaust on drain 2048");
}

} // namespace

int main() {
    try {
        test_initial_and_drain_boundaries();
        test_refuel_boundaries();
        test_original_operation_order();
        std::cout << "Fuel drain and refuel boundaries passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
