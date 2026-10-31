#pragma once
// V8: per-sensor automatic-telemetry watchdog, independent of Wi-Fi/Neon.
// No dynamic allocation, timestamps stored in wrapping uint32_t millis.
#include <array>
#include <cstdint>

namespace cca_auto_fallback {

enum class Kind : uint8_t { Water = 0, Mx2001 = 1, Temperature = 2 };

struct Watch {
    uint32_t node;
    Kind kind;
    uint32_t lastAutoMs = 0;
    uint32_t lastAttemptMs = 0;
    bool attempted = false;
};

class Policy {
public:
    static constexpr uint32_t MISSING_AUTO_MS = 60UL * 60UL * 1000UL;
    // One stream per independent data source. MX2001 READ contains BOTH
    // water level and temperature, so it has a single combined watchdog.
    static constexpr size_t WATCH_COUNT = 11;
    static constexpr uint8_t FALLBACK_RETRIES = 3;
    static constexpr uint32_t REPLY_TIMEOUT_MS = 30000UL;

    std::array<Watch, WATCH_COUNT> watches = {{
        {1949224949UL, Kind::Water},      // Pack Creek SEN0313 stage
        {1949224949UL, Kind::Mx2001},     // Pack Creek HOBO stage + temp
        {1695962694UL, Kind::Water},      // South Cottonwood MB7388 stage; NO HOBO
        {1683604927UL, Kind::Mx2001},    // Mill Creek HOBO stage + temp
        {2004386937UL, Kind::Mx2001},    // Matheson HOBO stage + temp
        {2425602407UL, Kind::Temperature}, // Cottonwood Relay HOBO temperature
        {3044869407UL, Kind::Temperature}, // Hidden Valley HOBO temperature
        {1577197109UL, Kind::Temperature}, // Fishlake Hightop HOBO temperature
        {647393146UL,  Kind::Temperature}, // Seed Relay 1 temperature
        {2650172798UL, Kind::Temperature}, // Thousand Lake Mountain temperature
        {4241345683UL, Kind::Temperature}  // Mini Me temperature
    }};

    // Never use device/battery telemetry to reset the sensor watchdog.
    // A successful reply to our fallback DM does NOT reset this timestamp;
    // only a true automatic packet does.
    bool receivedAuto(uint32_t node, Kind kind, uint32_t now) {
        bool matched = false;
        for (auto &watch : watches) {
            if (watch.node == node && watch.kind == kind) {
                watch.lastAutoMs = now;
                matched = true;
            }
        }
        return matched;
    }

    bool due(size_t index, uint32_t now) const {
        if (index >= watches.size() || now < MISSING_AUTO_MS) return false;
        const auto &watch = watches[index];
        if (static_cast<uint32_t>(now - watch.lastAutoMs) < MISSING_AUTO_MS) return false;
        return !watch.attempted ||
               static_cast<uint32_t>(now - watch.lastAttemptMs) >= MISSING_AUTO_MS;
    }

    void attemptedAt(size_t index, uint32_t now) {
        if (index >= watches.size()) return;
        watches[index].lastAttemptMs = now;
        watches[index].attempted = true;
    }
};

} // namespace cca_auto_fallback
