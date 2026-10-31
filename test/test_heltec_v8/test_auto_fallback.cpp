#include <cassert>
#include <cstring>
#include "src/modules/AutoFallbackPolicy.h"
#include "src/modules/CcaCc01DmParser.h"

int main() {
    using namespace cca_auto_fallback;
    Policy p;
    assert(p.watches.size() == 11);
    assert(Policy::thresholdMs(1949224949UL) == 900000); // Pack Creek, both sensors
    assert(Policy::thresholdMs(1683604927UL) == 900000); // Mill Creek
    assert(Policy::thresholdMs(1695962694UL) == 900000); // South Cottonwood
    assert(Policy::thresholdMs(2004386937UL) == 3600000); // Matheson
    assert(Policy::thresholdMs(2425602407UL) == 3600000); // Other stations
    for (size_t i = 0; i < p.watches.size(); ++i) {
        const uint32_t threshold = Policy::thresholdMs(p.watches[i].node);
        assert(!p.due(i, threshold - 1));
        assert(p.due(i, threshold)); // rescue begins immediately after missed interval
    }
    // All-watch and per-station DM pause only stop rescue. Their
    // automatic timestamps still update independently.
    p.pauseAll();
    assert(p.pausedMask == Policy::ALL_MASK);
    assert(!p.due(0, 3600000));
    // OFF then ON <node> must NOT accidentally enable every station.
    assert(p.setNodeEnabled(2004386937UL, true));
    p.enabled = true;
    assert(p.nodeEnabled(2004386937UL));
    assert(!p.nodeEnabled(1949224949UL));
    p.pauseAll();
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
    assert(!p.due(1, 4500001));
    assert(p.due(1, 4500002)); // Pack retries only once per 15-minute missing interval
    assert(!p.receivedAuto(1222051909UL, Kind::Water, 7200002)); // battery-only relay
    // MX2001 covers stage+temp with ONE request.
    assert(p.receivedAuto(2004386937UL, Kind::Mx2001, 7200002));
    assert(!p.due(4, 7200003));
    assert(p.due(4, 10800002));
    // Matheson cannot request again earlier than its one-hour interval.
    p.attemptedAt(4, 10800002);
    assert(!p.due(4, 14399999));
    assert(p.due(4, 14400002));
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
