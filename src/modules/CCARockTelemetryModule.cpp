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
constexpr uint16_t MIN_VALID_BATTERY_MV = 2500;
constexpr uint16_t MAX_VALID_BATTERY_MV = 5000;

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
    // XIAO battery sensing is calibrated for a 10-bit ADC. analogReadResolution()
    // is global on nRF52, so never switch the MCU to 12-bit here. Average native
    // 10-bit readings and scale to the established 0..4095 CCA calibration scale.
    uint32_t total10 = 0;
    for (uint8_t i = 0; i < ROCK_SAMPLE_COUNT; ++i) {
        total10 += static_cast<uint16_t>(analogRead(ROCK_PIN));
        delay(3);
    }
    const uint16_t average10 = static_cast<uint16_t>(total10 / ROCK_SAMPLE_COUNT);
    return static_cast<uint16_t>((static_cast<uint32_t>(average10) * 4095UL + 511UL) / 1023UL);
}

uint16_t batteryMv()
{
    if (powerStatus == nullptr || !powerStatus->getHasBattery())
        return 0;

    const uint32_t value = powerStatus->getBatteryVoltageMv();
    if (value < MIN_VALID_BATTERY_MV || value > MAX_VALID_BATTERY_MV)
        return 0;
    return static_cast<uint16_t>(value);
}

uint8_t batteryPercent(uint16_t millivolts)
{
    if (millivolts == 0 || powerStatus == nullptr || !powerStatus->getHasBattery())
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
    const uint8_t battPct = batteryPercent(battMv);

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("CCA ROCK: packet allocation failed");
        return false;
    }

    // 16-byte CCA rock packet, schema 1:
    // 0..1  = 'R','K'
    // 2     = schema version
    // 3     = flags (bit 0 = current motion)
    // 4..5  = averaged rock ADC on the established 0..4095 CCA calibration scale
    // 6..7  = sensor output millivolts (3.3 V reference)
    // 8..11 = motion rising-edge count since boot
    // 12..13= node battery millivolts; 0 means unavailable/invalid
    // 14    = node battery percent; 0 when battery voltage is unavailable
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
#ifdef BATTERY_SENSE_RESOLUTION_BITS
        analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS);
#else
        analogReadResolution(10);
#endif
        pinMode(ROCK_PIN, INPUT);
        pinMode(MOTION_PIN, INPUT);
        lastMotionState = digitalRead(MOTION_PIN) != 0;
        initialized = true;
        LOG_INFO("CCA ROCK 1.0.3: D0/A0 sandstone + D6 PIR; 60 s + PIR edge telemetry; safe battery ADC");
    }

    const bool motion = digitalRead(MOTION_PIN) != 0;
    const bool motionChanged = motion != lastMotionState;
    if (motionChanged) {
        if (motion)
            ++motionCount;
        lastMotionState = motion;

        // Send immediately on both LOW->HIGH and HIGH->LOW. This gives the cloud
        // an edge timestamp (within the 100 ms poll interval) for Last Motion and
        // Last Clear instead of relying on the next 60-second periodic sample.
        if (sendRockPacket())
            lastTelemetryMs = now;
    }

    if ((lastTelemetryMs == 0 && now >= FIRST_TELEMETRY_DELAY_MS) ||
        (lastTelemetryMs != 0 && now - lastTelemetryMs >= ROCK_INTERVAL_MS)) {
        if (sendRockPacket())
            lastTelemetryMs = now;
    }

    return POLL_INTERVAL_MS;
}

#endif
