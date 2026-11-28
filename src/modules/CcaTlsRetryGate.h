#pragma once
#include <cstdint>

// Config, health and cloud uploads share one host; legacy POST and HEAD
// share the other. Avoid a burst after a transport failure, but never apply
// a persistent endpoint-specific backoff to the other host.
class CcaTlsRetryGate {
    struct Backoff {
        uint32_t since = 0, duration = 0;
        uint8_t streak = 0;
        bool ready(uint32_t now) const { return !duration || uint32_t(now - since) >= duration; }
    };
    Backoff hosts[2]{};
    Backoff shared{};
    static int host(uint32_t op) {
        return op == 1 || op == 2 || op == 5 ? 0 : op == 3 || op == 4 ? 1 : -1;
    }
public:
    bool allowed(uint32_t op, uint32_t now) const {
        const int i = host(op);
        return i < 0 || (shared.ready(now) && hosts[i].ready(now));
    }
    void record(uint32_t op, int http, uint32_t now) {
        const int i = host(op);
        if (i < 0) return; // OTA staging remains controlled by its own retry policy.
        auto &h = hosts[i];
        if (http > 0) { h = {}; return; } // Any HTTP reply proves transport.
        if (h.streak < 4) ++h.streak;
        h.since = now;
        h.duration = 15000UL << (h.streak - 1); // 15/30/60/120s, capped.
        shared.since = now;
        shared.duration = 5000; // Allow other host after a short settling interval.
    }
};
