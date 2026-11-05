#pragma once
// Schedule missing-AUTO rescue batches on actual UTC hour boundaries.
// UTC :00 is also local :00 for every whole-hour timezone, including
// Mountain Standard/Daylight Time. Never infer wall time from millis().
#include "AutoFallbackPolicy.h"
#include <cstddef>
#include <cstdint>

namespace cca_auto_fallback {

class HourlyRescueBatch {
public:
    // Allow 90 seconds for RTC synchronization and loop scheduling after :00.
    // Once captured, the due batch drains sequentially through the LoRa queue.
    static constexpr uint32_t BOUNDARY_GRACE_SECONDS = 90;

    bool observe(uint32_t validUtcSeconds, uint32_t monotonicNowMs, const Policy &policy) {
        if (!validUtcSeconds) return false; // No trusted time: do not poll off-hour.
        const uint32_t utcHour = validUtcSeconds / 3600UL;
        if (haveHour && utcHour == lastObservedHour) return false;
        haveHour = true;
        lastObservedHour = utcHour;
        pending = 0;
        if (validUtcSeconds % 3600UL >= BOUNDARY_GRACE_SECONDS) return false;

        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            if (policy.due(i, monotonicNowMs))
                pending |= static_cast<uint16_t>(1U << i);
        }
        return pending != 0;
    }

    // Only watches that were missing at the last top-of-hour can be queued.
    // An AUTO recovery or operator OFF invalidates a pending request.
    int takeNext(const Policy &policy, uint32_t monotonicNowMs) {
        for (std::size_t i = 0; i < policy.watches.size(); ++i) {
            const uint16_t bit = static_cast<uint16_t>(1U << i);
            if (!(pending & bit)) continue;
            pending &= static_cast<uint16_t>(~bit);
            if (policy.due(i, monotonicNowMs))
                return static_cast<int>(i);
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
    bool hasValidBoundary() const { return haveHour; }
    uint32_t lastUtcHour() const { return lastObservedHour; }

private:
    bool haveHour = false;
    uint32_t lastObservedHour = 0;
    uint16_t pending = 0;
};
} // namespace cca_auto_fallback
