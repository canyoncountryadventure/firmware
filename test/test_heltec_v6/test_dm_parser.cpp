#include "src/modules/CcaCc01DmParser.h"
#include <cassert>
#include <cstring>

int main()
{
    using namespace cca_cc01_dm;
    constexpr uint32_t heltec = 2740603892UL; // !a35a4bf4
    constexpr uint32_t cc01 = 1695962694UL;  // !65165646
    // Meshtastic delivers secure unicast DMs with decoded channel 0.
    assert(isCc01Reply(cc01, heltec, heltec, 0, true));
    assert(isCc01Reply(cc01, heltec, heltec, 1, false));
    assert(!isCc01Reply(cc01, heltec, heltec, 0, false));
    assert(!isCc01Reply(cc01, heltec, heltec, 2, true));
    assert(!isCc01Reply(cc01, 999UL, heltec, 0, true));
    assert(!isCc01Reply(1949224949UL, heltec, heltec, 0, true));
    WaterReading water;
    assert(parseWaterRead("WATER READ\nRaw: 8.770 ft (2673mm)\nStage: 0.735 ft (22.4 cm)\nSamples:10 Spread:0mm", water));
    assert(water.distanceMm == 2673);
    assert(water.stageFeet > 0.734f && water.stageFeet < 0.736f);
    assert(!parseWaterRead("WATER READ\nRaw: 8.770 ft (2673mm)\nStage: NOT CALIBRATED\nSamples:10 Spread:0mm", water));
    assert(!parseWaterRead("WATER READ\nRaw: 8.770 ft (2673mm)\nStage: nan ft\nSamples:10 Spread:0mm", water));
    assert(!parseWaterRead("WATER READ\nRaw: 8.770 ft (9999mm)\nStage: 0.735 ft\nSamples:10 Spread:0mm", water));
    assert(!parseWaterRead("WATER STATUS\nReady:YES", water));

    HoboReading hobo;
    assert(parseHoboRead("MX2001\nLogger: AA:BB:CC:DD:EE:FF\nBLE: -63 dBm\nLevel: 0.72 ft\nTemp: 54.3 F", hobo));
    assert(std::strcmp(hobo.mac, "AA:BB:CC:DD:EE:FF") == 0);
    assert(hobo.bleRssi == -63);
    assert(hobo.levelFeet > 0.71f && hobo.levelFeet < 0.73f);
    assert(hobo.temperatureF > 54.2f && hobo.temperatureF < 54.4f);
    assert(!parseHoboRead("HOBO unavailable", hobo));
    assert(!parseHoboRead("MX2001\nLogger: AA:BB:CC:DD:EE:FF\nBLE: -63 dBm\nLevel: 0.72 ft", hobo));
    assert(!parseHoboRead("MX2001\nLogger: AA:BB:CC:DD:EE:FF\nBLE: -63 dBm\nLevel: nan ft\nTemp: 54.3 F", hobo));
    return 0;
}
