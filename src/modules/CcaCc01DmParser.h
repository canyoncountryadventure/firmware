#pragma once

// Strict, dependency-free parsers for the two CC01 on-demand Meshtastic DM
// replies. Never invent stage, temperature, or raw values when a reply fails.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace cca_cc01_dm {

// PKI messages are presented to MeshModules on decoded channel 0, regardless
// of the channel index selected by the sender. Non-PKI DMs must be on the
// configured LongFast channel (1). Source and destination must still match.
inline bool isCc01Reply(uint32_t from, uint32_t to, uint32_t gateway,
                        uint8_t decodedChannel, bool pkiEncrypted)
{
    constexpr uint32_t CC01_NODE = 1695962694UL; // !65165646
    constexpr uint8_t LONGFAST_LOCAL_CHANNEL = 1;
    return from == CC01_NODE && to == gateway &&
           ((pkiEncrypted && decodedChannel == 0) ||
            (!pkiEncrypted && decodedChannel == LONGFAST_LOCAL_CHANNEL));
}

struct WaterReading {
    uint32_t distanceMm = 0;
    float stageFeet = 0;
};

struct HoboReading {
    float levelFeet = 0;
    float temperatureF = 0;
    int bleRssi = 0;
    char mac[18] = {};
};

inline bool parseWaterRead(const char *s, WaterReading &out)
{
    if (s == nullptr || std::strncmp(s, "WATER READ\n", 11) != 0)
        return false;
    const char *raw = std::strstr(s, "\nRaw: ");
    const char *stage = std::strstr(s, "\nStage: ");
    if (!raw || !stage || !std::strstr(stage, "\nSamples:"))
        return false;

    float rawFeet = 0;
    float stageFeet = 0;
    unsigned long mm = 0;
    if (std::sscanf(raw, "\nRaw: %f ft (%lu mm)", &rawFeet, &mm) != 2 ||
        std::sscanf(stage, "\nStage: %f ft", &stageFeet) != 1)
        return false;
    if (!std::isfinite(rawFeet) || !std::isfinite(stageFeet) ||
        rawFeet < 0 || rawFeet > 120 || mm == 0 || mm > 40000 ||
        stageFeet < -20 || stageFeet > 120 ||
        std::fabs(rawFeet - static_cast<float>(mm) / 304.8f) > 0.01f)
        return false;

    out.distanceMm = static_cast<uint32_t>(mm);
    out.stageFeet = stageFeet;
    return true;
}

inline bool parseHoboRead(const char *s, HoboReading &out)
{
    if (s == nullptr || std::strncmp(s, "MX2001\nLogger: ", 15) != 0)
        return false;
    unsigned int mac[6] = {};
    int rssi = 0;
    float levelFeet = 0;
    float temperatureF = 0;
    const int count = std::sscanf(
        s,
        "MX2001\nLogger: %2x:%2x:%2x:%2x:%2x:%2x\nBLE: %d dBm\nLevel: %f ft\nTemp: %f F",
        &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5],
        &rssi, &levelFeet, &temperatureF);
    if (count != 9 || !std::isfinite(levelFeet) || !std::isfinite(temperatureF) ||
        levelFeet < -20 || levelFeet > 120 || temperatureF < -40 ||
        temperatureF > 180 || rssi < -127 || rssi > 20)
        return false;

    std::snprintf(out.mac, sizeof(out.mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    out.bleRssi = rssi;
    out.levelFeet = levelFeet;
    out.temperatureF = temperatureF;
    return true;
}

} // namespace cca_cc01_dm
