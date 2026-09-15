#include "configuration.h"

#if defined(TRAIL_COUNTER_SEN0171)

#include "TrailCounterModule.h"

#include "MeshService.h"
#include "main.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

TrailCounterModule *trailCounterModule;

namespace {
constexpr uint8_t PIR_PIN = D0;
constexpr uint32_t POLL_INTERVAL_MS = 20;
} // namespace

int32_t TrailCounterModule::runOnce()
{
    const uint32_t now = millis();

    if (!initialized) {
        pinMode(PIR_PIN, INPUT);

        const bool state = digitalRead(PIR_PIN);
        initialized = true;
        lastState = state;
        armed = !state;

        if (!state) {
            lowStarted = now;
        }

        if (armed) {
            LOG_INFO("Trail counter: SEN0171 armed on D0");
        } else {
            LOG_INFO("Trail counter: SEN0171 on D0; waiting for startup HIGH to clear before arming");
        }

        return POLL_INTERVAL_MS;
    }

    const bool state = digitalRead(PIR_PIN);

    if (!armed) {
        if (!state) {
            armed = true;
            lastState = false;
            lowStarted = now;
            LOG_INFO("Trail counter: PIR LOW; counter armed");
        } else {
            lastState = true;
        }
        return POLL_INTERVAL_MS;
    }

    if (state && !lastState) {
        personCount++;
        const uint32_t lowGapMs = lowStarted > 0 ? now - lowStarted : 0;
        highStarted = now;

        LOG_INFO("Trail counter: PERSON WALKED BY #%lu; low_gap=%lu ms", (unsigned long)personCount,
                 (unsigned long)lowGapMs);

        sendPersonMessage(lowGapMs);
    }

    if (!state && lastState) {
        const uint32_t highDurationMs = highStarted > 0 ? now - highStarted : 0;
        lowStarted = now;

        LOG_INFO("Trail counter: PIR LOW; detection_duration=%lu ms; ready for next distinct person",
                 (unsigned long)highDurationMs);
    }

    lastState = state;
    return POLL_INTERVAL_MS;
}

void TrailCounterModule::sendPersonMessage(uint32_t lowGapMs)
{
    char message[80];

    if (personCount > 1 && lowGapMs > 0) {
        snprintf(message, sizeof(message), "PERSON WALKED BY #%lu (gap %lu ms)", (unsigned long)personCount,
                 (unsigned long)lowGapMs);
    } else {
        snprintf(message, sizeof(message), "PERSON WALKED BY #%lu", (unsigned long)personCount);
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    p->want_ack = false;
    p->decoded.payload.size = strlen(message);
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);

    LOG_INFO("Trail counter: transmit id=%lu msg=%s", (unsigned long)p->id, message);
    service->sendToMesh(p);
}

#endif // defined(TRAIL_COUNTER_SEN0171)
