#include <cassert>
#include <cstring>
#include "src/modules/AutoFallbackPolicy.h"
#include "src/modules/CcaCc01DmParser.h"

int main() {
    using namespace cca_auto_fallback;
    Policy p;
    assert(p.watches.size() == 11);
    for (size_t i = 0; i < p.watches.size(); ++i) {
        assert(!p.due(i, 3599999));
        assert(p.due(i, 3600000)); // first missed hour after boot
    }
    // All-watch and per-station DM pause only stop rescue. Their
    // automatic timestamps still update independently.
    p.pauseAll();
    assert(!p.due(0, 3600000));
    assert(p.receivedAuto(1949224949UL, Kind::Water, 3600000));
    p.enableAll();
    assert(!p.due(0, 3600001));
    assert(p.setNodeEnabled(1949224949UL, false));
    assert(!p.due(0, 7200001) && !p.due(1, 7200001));
    assert(p.due(2, 7200001)); // South Cottonwood independent
    assert(p.setNodeEnabled(1949224949UL, true));
    assert(p.due(0, 7200001));
    assert(!p.setNodeEnabled(1222051909UL, false)); // battery-only
    p.setOnlyNode(2004386937UL);
    assert(!p.due(0, 7200001) && p.due(4, 7200001));
    p.setNodeEnabled(1683604927UL, true);
    assert(p.due(3, 7200001));
    p.enableAll();
    assert(p.pausedMask == 0 && p.enabled);
    // Fresh Pack Creek ultrasonic telemetry does NOT silence its HOBO watchdog.
    assert(p.receivedAuto(1949224949UL, Kind::Water, 3600001));
    assert(!p.due(0, 3600002));
    assert(p.due(1, 3600002));
    p.attemptedAt(1, 3600002);
    assert(!p.due(1, 7199999));
    assert(p.due(1, 7200002)); // at most one rescue cycle per missing hour
    assert(!p.receivedAuto(1222051909UL, Kind::Water, 7200002)); // battery-only relay
    // MX2001 covers stage+temp with ONE request.
    assert(p.receivedAuto(2004386937UL, Kind::Mx2001, 7200002));
    assert(!p.due(4, 7200003));
    assert(p.due(4, 10800002));
    // Distinguish environmental temperature from MX2001 on the same node.
    assert(!p.receivedAuto(1695962694UL, Kind::Temperature, 7200002));
    cca_cc01_dm::HoboTemperatureReading t;
    assert(cca_cc01_dm::parseHoboTemperatureRead(
        "MX2201\\nLogger: E3:1A:98:DC:FB:C2\\nBLE: -76 dBm\\nTemp: 49.4 F", t));
    assert(t.temperatureF > 49.3f && t.temperatureF < 49.5f);
    assert(cca_cc01_dm::parseHoboTemperatureRead(
        "MX2203\\nLogger: E3:1A:98:DC:FB:C2\\nBLE: -76 dBm\\nTemp: 49.41 F / 9.67 C", t));
    assert(!cca_cc01_dm::parseHoboTemperatureRead(
        "MX2001\\nLogger: E3:1A:98:DC:FB:C2\\nBLE: -76 dBm\\nLevel: 0.5 ft\\nTemp: 49.4 F", t));
    assert(!cca_cc01_dm::parseHoboTemperatureRead("HOBO unavailable", t));
    return 0;
}
