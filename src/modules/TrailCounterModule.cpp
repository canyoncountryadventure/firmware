#include "TrailCounterModule.h"

#include "MeshService.h"
#include "configuration.h"
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

    // SEN0171 can power up HIGH. Do not count that as a person. Arm only after
    // the sensor has returned LOW at least once.
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

    // LOW -> HIGH: one distinct PIR event. If two people pass while the
    // SEN0171 remains continuously HIGH, the hardware provides only one event
    // and no firmware can reliably separate them.
    if (state && !lastState) {
        personCount++;
        const uint32_t lowGapMs = lowStarted > 0 ? now - lowStarted : 0;
        highStarted = now;

        LOG_INFO("Trail counter: PERSON WALKED BY #%lu; low_gap=%lu ms", (unsigned long)personCount,
                 (unsigned long)lowGapMs);

        sendPersonMessage(lowGapMs);
    }

    // HIGH -> LOW: record the actual hold time so bench/field testing can show
    // how quickly the sensor is capable of recognizing the next person.
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
    p->decoded.payload.size = strnlen(message, sizeof(message));
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);

    LOG_INFO("Trail counter: transmit id=%lu msg=%s", (unsigned long)p->id, message);
    service->sendToMesh(p);
}
