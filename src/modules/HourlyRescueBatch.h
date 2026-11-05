#pragma once
// Per-station clock-aligned missing-AUTO batch scheduler.
// A 60-minute cadence fires at UTC :00, which is also :00 Mountain local.
// All requests are scheduled only after a FULL hour without valid AUTO.
#include "AutoFallbackPolicy.h"
#include <cstddef>
#include <cstdint>

namespace cca_auto_fallback {

class HourlyRescueBatch {
public:
    // Only the first minute of a boundary qualifies. A 90-second window
    // could incorrectly trigger at 02:01 after missing the 02:00 cutoff.
    static constexpr uint32_t BOUNDARY_GRACE_SECONDS = 60;

    bool observe(uint32_t validUtcSeconds, uint32_t monotonicNowMs, const Policy &policy) {
        if (!validUtcSeconds) return false; // Fail closed until clock is trustworthy.
        const uint32_t minute = validUtcSeconds / 60UL;
        // Never repeatedly reevaluate one minute or replay an earlier RTC time.
        if (haveMinute && minute <= lastObservedMinute) return false;
        haveMinute = true;
        lastObservedMinute = minute;

        bool added = false;
        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            const uint32_t intervalSeconds =
                static_cast<uint32_t>(policy.intervalMinutes(i)) * 60UL;
            // Periods are aligned to UTC midnight. 45-minute windows are
            // :00, :45, 01:30, 02:15, etc; 60-minute periods always :00.
            if (!intervalSeconds || validUtcSeconds % intervalSeconds >= BOUNDARY_GRACE_SECONDS)
                continue;
            if (!policy.due(i, monotonicNowMs)) continue;
            const uint16_t bit = static_cast<uint16_t>(1U << i);
            if (!(pending & bit)) {
                pending |= bit;
                added = true;
            }
        }
        return added;
    }

    // Hold other stations in RAM while LoRa handles one job (with retries).
    // Recheck the watch to avoid DMing a node that just recovered.
    int takeNext(const Policy &policy, uint32_t monotonicNowMs) {
        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            const uint16_t bit = static_cast<uint16_t>(1U << i);
            if (!(pending & bit)) continue;
            pending &= static_cast<uint16_t>(~bit);
            if (policy.due(i, monotonicNowMs)) return static_cast<int>(i);
        }
        return -1;
    }

    void cancel(uint32_t node, Kind kind, const Policy &policy) {
        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            const auto &w = policy.watches[i];
            if (w.node == node && w.kind == kind)
                pending &= static_cast<uint16_t>(~(1U << i));
        }
    }

    void cancelNode(uint32_t node, const Policy &policy) {
        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            if (policy.watches[i].node == node)
                pending &= static_cast<uint16_t>(~(1U << i));
        }
    }

    void clearPending() { pending = 0; }
    uint16_t pendingMask() const { return pending; }
    bool hasValidBoundary() const { return haveMinute; }
    uint32_t lastUtcHour() const { return haveMinute ? lastObservedMinute / 60UL : 0UL; }

private:
    bool haveMinute = false;
    uint32_t lastObservedMinute = 0;
    uint16_t pending = 0;
};
} // namespace cca_auto_fallback
