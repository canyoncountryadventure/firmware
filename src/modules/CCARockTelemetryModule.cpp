#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "CCARockTelemetryModule.h"

#include "MeshService.h"
#include "PowerFSM.h"
#include "main.h"

#include <Arduino.h>
#include <cstdint>
#include <cstring>

CCARockTelemetryModule *ccaRockTelemetryModule;

namespace
{
constexpr uint8_t ROCK_PIN = A0;
constexpr uint8_t MOTION_PIN = D6;
constexpr uint32_t ROCK_INTERVAL_MS = 60UL * 1000UL;
constexpr uint32_t POLL_INTERVAL_MS = 100UL;
constexpr uint32_t FIRST_TELEMETRY_DELAY_MS = 2000UL;
constexpr uint8_t ROCK_SAMPLE_COUNT = 20;
constexpr uint8_t ROCK_SCHEMA_VERSION = 1;

bool initialized = false;
bool lastMotionState = false;
uint32_t motionCount = 0;
uint32_t lastTelemetryMs = 0;

void writeLE16(uint8_t *p, uint16_t value)
{
    p[0] = static_cast<uint8_t>(value & 0xff);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void writeLE32(uint8_t *p, uint32_t value)
{
    p[0] = static_cast<uint8_t>(value & 0xff);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xff);
    p[2] = static_cast<uint8_t>((value >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}

uint16_t readRockAverage()
{
    uint32_t total = 0;
    for (uint8_t i = 0; i < ROCK_SAMPLE_COUNT; ++i) {
        total += static_cast<uint16_t>(analogRead(ROCK_PIN));
        delay(3);
    }
    return static_cast<uint16_t>(total / ROCK_SAMPLE_COUNT);
}

uint16_t batteryMv()
{
    if (powerStatus == nullptr)
        return 0;
    const uint32_t value = powerStatus->getBatteryVoltageMv();
    return value > 65535U ? 65535U : static_cast<uint16_t>(value);
}

uint8_t batteryPercent()
{
    if (powerStatus == nullptr || !powerStatus->getHasBattery())
        return 0;
    return powerStatus->getBatteryChargePercent();
}
} // namespace

CCARockTelemetryModule::CCARockTelemetryModule()
    : SinglePortModule("CCARockTelemetry", meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread("CCARockTelemetry")
{
    setIntervalFromNow(500);
}

bool CCARockTelemetryModule::wantPacket(const meshtastic_MeshPacket *p)
{
    (void)p;
    return false;
}

ProcessMessage CCARockTelemetryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    (void)mp;
    return ProcessMessage::CONTINUE;
}

bool CCARockTelemetryModule::sendRockPacket()
{
    const uint16_t adc = readRockAverage();
    const uint16_t sensorMv = static_cast<uint16_t>((static_cast<uint32_t>(adc) * 3300UL + 2047UL) / 4095UL);
    const bool motion = digitalRead(MOTION_PIN) != 0;
    const uint16_t battMv = batteryMv();
    const uint8_t battPct = batteryPercent();

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("CCA ROCK: packet allocation failed");
        return false;
    }

    // 16-byte CCA rock packet, schema 1:
    // 0..1  = 'R','K'
    // 2     = schema version
    // 3     = flags (bit 0 = current motion)
    // 4..5  = averaged 12-bit rock ADC
    // 6..7  = sensor output millivolts (3.3 V ADC reference)
    // 8..11 = motion rising-edge count since boot
    // 12..13= node battery millivolts
    // 14    = node battery percent
    // 15    = reserved
    uint8_t payload[16] = {};
    payload[0] = 'R';
    payload[1] = 'K';
    payload[2] = ROCK_SCHEMA_VERSION;
    payload[3] = motion ? 0x01 : 0x00;
    writeLE16(&payload[4], adc);
    writeLE16(&payload[6], sensorMv);
    writeLE32(&payload[8], motionCount);
    writeLE16(&payload[12], battMv);
    payload[14] = battPct;

    memcpy(packet->decoded.payload.bytes, payload, sizeof(payload));
    packet->decoded.payload.size = sizeof(payload);
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->want_ack = false;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);

    LOG_INFO("CCA ROCK: ADC=%u sensor=%u mV motion=%s count=%lu battery=%u mV/%u%%",
             adc, sensorMv, motion ? "YES" : "NO", static_cast<unsigned long>(motionCount), battMv, battPct);
    return true;
}

int32_t CCARockTelemetryModule::runOnce()
{
    const uint32_t now = millis();

    if (!initialized) {
        analogReadResolution(12);
        pinMode(ROCK_PIN, INPUT);
        pinMode(MOTION_PIN, INPUT);
        lastMotionState = digitalRead(MOTION_PIN) != 0;
        initialized = true;
        LOG_INFO("CCA ROCK 1.0.0: D0/A0 sandstone probe + D6 motion; 60 s telemetry");
    }

    const bool motion = digitalRead(MOTION_PIN) != 0;
    if (motion && !lastMotionState)
        ++motionCount;
    lastMotionState = motion;

    if ((lastTelemetryMs == 0 && now >= FIRST_TELEMETRY_DELAY_MS) ||
        (lastTelemetryMs != 0 && now - lastTelemetryMs >= ROCK_INTERVAL_MS)) {
        if (sendRockPacket())
            lastTelemetryMs = now;
    }

    return POLL_INTERVAL_MS;
}

#endif
