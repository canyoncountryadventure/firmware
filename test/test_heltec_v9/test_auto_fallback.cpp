#include <cassert>
#include <cstdint>
#include "src/modules/AutoFallbackPolicy.h"
int main() {
    using namespace cca_auto_fallback;
    Policy p;
    assert(!p.enabled);
    for (size_t i = 0; i < p.watches.size(); ++i) {
        assert(!p.isEnabled(i));
        assert(!p.due(i, 24UL*60UL*60UL*1000UL));
        assert(p.intervalMinutes(i) == 60);
    }
    assert(Policy::thresholdMs(1949224949UL) == 3600000UL);
    assert(Policy::thresholdMs(1683604927UL) == 3600000UL);
    assert(Policy::thresholdMs(1695962694UL) == 3600000UL);
    assert(Policy::validIntervalMinutes(15));
    assert(Policy::validIntervalMinutes(30));
    assert(Policy::validIntervalMinutes(60));
    assert(Policy::validIntervalMinutes(1440));
    assert(!Policy::validIntervalMinutes(5));
    assert(!Policy::validIntervalMinutes(20));
    assert(!Policy::validIntervalMinutes(90));
    assert(!p.setNodeEnabled(1222051909UL, true)); // Battery-only node isn't polled.
    assert(p.setNodeEnabled(1949224949UL, true));
    p.enabled = true;
    assert(p.nodeEnabled(1949224949UL));
    assert(!p.nodeEnabled(1683604927UL));
    assert(!p.nodeEnabled(2004386937UL));
    p.armNode(1949224949UL, 100000UL);
    assert(!p.due(0, 3699999UL));
    assert(p.due(0, 3700000UL));
    assert(p.due(1, 3700000UL)); // Each Pack sensor is independent.
    p.attemptedAt(0, 3700000UL);
    assert(!p.due(0, 3700001UL));
    assert(p.due(0, 7300000UL));
    assert(p.receivedAuto(1949224949UL, Kind::Water, 7300000UL));
    assert(!p.due(0, 7300001UL));
    assert(p.due(1, 7300001UL)); // Water AUTO cannot conceal a failed HOBO.
    assert(p.setNodeIntervalMinutes(1949224949UL, 15));
    p.armNode(1949224949UL, 10000000UL);
    assert(!p.due(0, 10899999UL));
    assert(p.due(0, 10900000UL));
    assert(p.setNodeIntervalMinutes(1949224949UL, 30));
    p.armNode(1949224949UL, 11000000UL);
    assert(!p.due(0, 12799999UL));
    assert(p.due(0, 12800000UL));
    assert(p.setNodeIntervalMinutes(1949224949UL, 1440));
    p.armNode(1949224949UL, 200000000UL);
    assert(!p.due(0, 286399999UL));
    assert(p.due(0, 286400000UL));
    assert(p.setNodeEnabled(1949224949UL, false));
    assert(!p.due(0, 286400001UL));
    assert(p.setNodeEnabled(1683604927UL, true));
    assert(p.nodeEnabled(1683604927UL));
    assert(!p.nodeEnabled(1949224949UL));
    p.pauseAll();
    assert(!p.nodeEnabled(1683604927UL));
    assert(p.pausedMask == Policy::ALL_MASK);
    return 0;
}
