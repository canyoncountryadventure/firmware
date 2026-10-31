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
    static constexpr uint32_t FAST_MISSING_AUTO_MS = 15UL * 60UL * 1000UL;
    // One stream per independent data source. MX2001 READ contains BOTH
    // water level and temperature, so it has a single combined watchdog.
    static constexpr size_t WATCH_COUNT = 11;
    static constexpr uint8_t FALLBACK_RETRIES = 3;
    static constexpr uint32_t REPLY_TIMEOUT_MS = 30000UL;

    // A bit per stream; station ON/OFF affects all streams of the same node.
    // Compact enough for the existing ESP32 Preferences namespace (11 bits).
    bool enabled = true;
    uint16_t pausedMask = 0;

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

    static constexpr uint16_t ALL_MASK = (1U << WATCH_COUNT) - 1U;

    // Fast stations monitor each independent stage/temperature stream every 15m.
    // All other monitored stations use 60m. This controls gateway fallback
    // requests, not the sampling interval programmed into the field logger.
    static constexpr uint32_t thresholdMs(uint32_t node) {
        return node == 1949224949UL || // Pack Creek: ultrasonic + HOBO
               node == 1683604927UL || // Mill Creek: MX2001
               node == 1695962694UL    // South Cottonwood: MB7388
            ? FAST_MISSING_AUTO_MS : MISSING_AUTO_MS;
    }

    bool isKnownNode(uint32_t node) const {
        for (const auto &watch : watches)
            if (watch.node == node) return true;
        return false;
    }
    bool isEnabled(size_t index) const {
        return index < watches.size() && enabled &&
            (pausedMask & static_cast<uint16_t>(1U << index)) == 0;
    }
    bool setNodeEnabled(uint32_t node, bool allow) {
        bool found = false;
        for (size_t i = 0; i < watches.size(); ++i) {
            if (watches[i].node != node) continue;
            found = true;
            const uint16_t bit = static_cast<uint16_t>(1U << i);
            if (allow) pausedMask &= static_cast<uint16_t>(~bit);
            else pausedMask |= bit;
        }
        return found;
    }
    bool nodeEnabled(uint32_t node) const {
        for (size_t i = 0; i < watches.size(); ++i)
            if (watches[i].node == node && isEnabled(i)) return true;
        return false;
    }
    void setOnlyNode(uint32_t node) {
        pausedMask = ALL_MASK;
        setNodeEnabled(node, true);
        enabled = true;
    }
    void enableAll() { pausedMask = 0; enabled = true; }
    void pauseAll() { pausedMask = ALL_MASK; enabled = false; }

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
        if (!isEnabled(index)) return false;
        const auto &watch = watches[index];
        const uint32_t threshold = thresholdMs(watch.node);
        if (now < threshold || static_cast<uint32_t>(now - watch.lastAutoMs) < threshold) return false;
        // Bound repeat rescue cycles to the node's own auto-reporting deadline.
        return !watch.attempted ||
               static_cast<uint32_t>(now - watch.lastAttemptMs) >= threshold;
    }

    void attemptedAt(size_t index, uint32_t now) {
        if (index >= watches.size()) return;
        watches[index].lastAttemptMs = now;
        watches[index].attempted = true;
    }
};

} // namespace cca_auto_fallback
