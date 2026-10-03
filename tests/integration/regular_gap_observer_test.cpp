#include "support/regular_gap_observer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace river_raid;
using namespace river_raid::test;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

// Synthetic observation data only; these tests never inject gameplay state.
RegularGapSnapshot empty(std::uint16_t bridge = 20) {
    return {{}, {}, bridge, LifeCyclePhase::Active, 3, true};
}

RegularGapSnapshot crossing(std::uint8_t y = 33, std::uint8_t counter = 0xff,
                            std::uint16_t bridge = 20, bool enabled = true) {
    auto snapshot = empty(bridge);
    snapshot.mechanics.crossing = BridgeSpritePresentation{
        BridgeSpriteImage::CrossingRightA, 80, y, 0};
    snapshot.mechanics.gap_animation_counter = counter;
    snapshot.mechanics.automatic_entry_enabled = enabled;
    snapshot.published = snapshot.mechanics;
    if (counter >= 1 && counter <= 17) {
        snapshot.mechanics.gap = BridgeSpritePresentation{
            BridgeSpriteImage::GapFlight, 94, 56, 1};
        snapshot.published.gap = snapshot.mechanics.gap;
    }
    return snapshot;
}

void arm(RegularGapObserver& observer, std::uint16_t bridge = 20) {
    require(!observer.observe(empty(bridge), crossing(33, 0xff, bridge), 1) && observer.armed(),
            "New regular crossing did not arm without prematurely succeeding");
}

void test_regular_entry_and_following_survival() {
    for (const auto entry_counter : {std::uint8_t{0}, std::uint8_t{1}}) {
        RegularGapObserver observer;
        arm(observer);
        const auto before = crossing(59);
        auto entry = crossing(60, entry_counter);
        require(!observer.observe(before, entry, 1), "Entry itself counted as following survival");
        if (entry_counter == 0) {
            const auto visible = crossing(61, 1);
            require(!observer.observe(entry, visible, 1), "Gap publication counted as survival");
            entry = visible;
        }
        require(observer.awaiting_survival() && observer.witness(), "Published gap did not await survival");
        require(!observer.observe(entry, entry, 0) && !observer.confirmed(),
                "Zero-update observation confirmed survival");
        require(observer.observe(entry, crossing(62, 2), 1) && observer.confirmed(),
                "Separate surviving observation did not confirm regular entry");
    }
}

void test_scripted_proximity_and_joint_rejected() {
    RegularGapObserver scripted;
    require(!scripted.observe(empty(), crossing(33, 0xff, 20, false), 1) && !scripted.armed(),
            "Scripted crossing armed the observer");
    require(!scripted.observe(crossing(59, 0xff, 20, false), crossing(60, 1, 21, false), 1),
            "Scripted proximity gap was accepted");
    RegularGapObserver proximity;
    arm(proximity);
    require(!proximity.observe(crossing(59), crossing(60, 1, 21), 1) && !proximity.armed(),
            "Regular proximity destruction retained provenance");
    RegularGapObserver joint;
    arm(joint);
    auto changed = crossing(60, 1);
    changed.mechanics.crossing->image = BridgeSpriteImage::JointExplosionA;
    changed.published.crossing->image = BridgeSpriteImage::JointExplosionA;
    require(!joint.observe(crossing(59), changed, 1) && !joint.armed(),
            "Joint image passed normal-crossing acceptance");
}

void test_stale_ff_never_rearmed_and_new_crossing_allowed() {
    RegularGapObserver observer;
    arm(observer);
    const auto stale = crossing(40, 0xff, 21);
    require(!observer.observe(crossing(39), stale, 1) && !observer.armed(),
            "Bridge change with unchanged FF did not invalidate the instance");
    require(!observer.observe(stale, crossing(41, 0xff, 21), 1) && !observer.armed(),
            "Stale enabled FF crossing re-armed after destruction");
    require(!observer.observe(crossing(41, 0xff, 21), crossing(0, 0xff, 22), 1) && !observer.armed(),
            "Coordinate reset plus bridge change re-armed the same enabled crossing");
    require(!observer.observe(crossing(59, 0xff, 21), crossing(60, 1, 21), 1),
            "Invalidated instance later accepted automatic-looking entry");
    require(!observer.observe(crossing(206, 5, 21), empty(21), 1) && !observer.armed(),
            "Retired crossing did not invalidate its instance");
    require(!observer.observe(empty(20), crossing(33, 0xff, 21), 1) && !observer.armed(),
            "Bridge change during the observed spawn armed ambiguous provenance");
    require(!observer.observe(empty(21), crossing(33, 0xff, 21), 1) && observer.armed(),
            "New regular crossing after an earlier bridge did not arm");
    require(!observer.observe(crossing(59, 0xff, 21), crossing(60, 1, 21), 1) &&
                observer.observe(crossing(60, 1, 21), crossing(61, 2, 21), 1),
            "Earlier bridge progression prevented a fresh regular witness");
}

void test_multiupdate_inactivity_retirement_and_pending_death() {
    RegularGapObserver multiple;
    require(!multiple.observe(empty(), crossing(), 2) && !multiple.armed(),
            "Multiupdate birth armed ambiguous provenance");
    require(!multiple.observe(crossing(), crossing(34), 1) && !multiple.armed(),
            "Ambiguously born instance re-armed on a later snapshot");
    RegularGapObserver multiple_entry;
    arm(multiple_entry);
    require(!multiple_entry.observe(crossing(59), crossing(60, 1), 2) && !multiple_entry.armed(),
            "Multiupdate entry retained provenance");
    RegularGapObserver multiple_survival;
    arm(multiple_survival);
    require(!multiple_survival.observe(crossing(59), crossing(60, 1), 1) &&
                !multiple_survival.observe(crossing(60, 1), crossing(62, 3), 2) &&
                !multiple_survival.witness(),
            "Multiupdate following observation confirmed survival");
    RegularGapObserver inactive;
    auto waiting = empty();
    waiting.life_phase = LifeCyclePhase::AwaitInput;
    for (int index = 0; index < 3; ++index)
        require(!inactive.observe(waiting, waiting, 0) && !inactive.armed(),
                "Repeated inactive snapshots armed the observer");
    arm(inactive);
    require(!inactive.observe(crossing(60, 1), empty(), 1) && !inactive.armed(),
            "Retirement retained a witness");
    arm(inactive, 22);
    require(!inactive.observe(crossing(59, 0xff, 22), crossing(60, 1, 22), 1),
            "Entry prematurely confirmed after a new spawn");
    auto dead = crossing(61, 2, 22);
    dead.flying = false;
    dead.life_phase = LifeCyclePhase::Exploding;
    require(!inactive.observe(crossing(60, 1, 22), dead, 1) && !inactive.witness(),
            "Queued death confirmed the pending witness");
}

void test_publication_and_counter_guards() {
    RegularGapObserver observer;
    arm(observer);
    auto private_only = crossing(60, 1);
    private_only.published.gap.reset();
    require(!observer.observe(crossing(59), private_only, 1) && !observer.awaiting_survival(),
            "Private-only gap satisfied publication acceptance");
    require(!observer.observe(private_only, crossing(61, 2), 1) && observer.awaiting_survival(),
            "Later publication did not establish a witness");
    RegularGapObserver skipped;
    arm(skipped);
    require(!skipped.observe(crossing(59), crossing(60, 2), 1) && !skipped.armed(),
            "Skipped FF-to-entry counter transition was accepted");
}

} // namespace

int main() {
    try {
        test_regular_entry_and_following_survival();
        test_scripted_proximity_and_joint_rejected();
        test_stale_ff_never_rearmed_and_new_crossing_allowed();
        test_multiupdate_inactivity_retirement_and_pending_death();
        test_publication_and_counter_guards();
        std::cout << "Regular gap snapshot observer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
