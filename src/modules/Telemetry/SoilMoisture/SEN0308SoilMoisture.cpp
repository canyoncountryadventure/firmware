#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "SEN0308SoilMoisture.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "main.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include "pb_encode.h"

#include <Arduino.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
static constexpr uint8_t SOIL_PIN = A1; // RAK19007 AIN1 -> RAK4631 P0.31/AIN7
static constexpr uint16_t DRY_ADC10 = 580;
static constexpr uint16_t WET_ADC10 = 0;
static constexpr uint8_t SAMPLE_COUNT = 20;
static constexpr uint32_t SAMPLE_DELAY_MS = 10;
static constexpr uint32_t STARTUP_DELAY_MS = 30000UL;
static constexpr uint32_t AUTO_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour

bool haveLastReading = false;
SEN0308SoilMoistureModule::Reading lastReading = {};
uint16_t telemetrySequence = 0;

bool commandEquals(const uint8_t *bytes, size_t size, const char *expected)
{
    if (bytes == nullptr || expected == nullptr || size == 0)
        return false;

    char command[48] = {};
    size_t n = size;
    if (n > sizeof(command) - 1)
        n = sizeof(command) - 1;
    memcpy(command, bytes, n);

    char *p = command;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p == '/')
        ++p;

    char *end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';

    const size_t expectedLength = strlen(expected);
    if (strlen(p) != expectedLength)
        return false;

    for (size_t i = 0; i < expectedLength; ++i) {
        if (std::toupper(static_cast<unsigned char>(p[i])) !=
            std::toupper(static_cast<unsigned char>(expected[i])))
            return false;
    }
    return true;
}

} // namespace

SEN0308SoilMoistureModule::SEN0308SoilMoistureModule()
    : SinglePortModule("sen0308_soil", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("sen0308_soil")
{
    isPromiscuous = true;
    pinMode(SOIL_PIN, INPUT);
    setIntervalFromNow(STARTUP_DELAY_MS);
}

bool SEN0308SoilMoistureModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

SEN0308SoilMoistureModule::Reading SEN0308SoilMoistureModule::sample()
{
    // Calibration was established at 10-bit resolution. Keep each acquisition
    // deterministic; Meshtastic battery sampling configures its own ADC path.
    analogReadResolution(10);

    uint32_t total = 0;
    for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
        total += static_cast<uint16_t>(analogRead(SOIL_PIN));
        delay(SAMPLE_DELAY_MS);
    }

    const uint16_t adc10 = static_cast<uint16_t>(lroundf(static_cast<float>(total) / SAMPLE_COUNT));

    int32_t moisture = 0;
    if (adc10 <= WET_ADC10) {
        moisture = 100;
    } else if (adc10 >= DRY_ADC10) {
        moisture = 0;
    } else {
        moisture = static_cast<int32_t>(lroundf(
            (static_cast<float>(DRY_ADC10 - adc10) * 100.0f) /
            static_cast<float>(DRY_ADC10 - WET_ADC10)));
    }

    if (moisture < 0)
        moisture = 0;
    if (moisture > 100)
        moisture = 100;

    Reading reading = {adc10, static_cast<uint8_t>(moisture)};
    lastReading = reading;
    haveLastReading = true;

    LOG_INFO("SEN0308 soil: ADC10=%u moisture=%u%%", reading.adc10, reading.moisturePercent);
    return reading;
}

bool SEN0308SoilMoistureModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr)
        return false;

    size_t len = strlen(text);
    if (len > sizeof(packet->decoded.payload.bytes))
        len = sizeof(packet->decoded.payload.bytes);

    memcpy(packet->decoded.payload.bytes, text, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

bool SEN0308SoilMoistureModule::sendTelemetry(const Reading &reading)
{
    meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
    telemetry.time = getTime();
    telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
    telemetry.variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;
    telemetry.variant.environment_metrics.has_soil_moisture = true;
    telemetry.variant.environment_metrics.soil_moisture = reading.moisturePercent;

    if (nodeDB != nullptr)
        nodeDB->updateTelemetry(nodeDB->getNodeNum(), telemetry, RX_SRC_LOCAL);

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr)
        return false;

    const size_t encoded = pb_encode_to_bytes(
        packet->decoded.payload.bytes,
        sizeof(packet->decoded.payload.bytes),
        &meshtastic_Telemetry_msg,
        &telemetry);

    if (encoded == 0)
        return false;

    packet->decoded.payload.size = encoded;
    packet->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

bool SEN0308SoilMoistureModule::sendRawPacket(const Reading &reading)
{
    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr)
        return false;

    telemetrySequence++;
    uint8_t *payload = packet->decoded.payload.bytes;
    payload[0] = 'S';
    payload[1] = 'M';
    payload[2] = 1; // packet version
    payload[3] = reading.moisturePercent;
    payload[4] = static_cast<uint8_t>(reading.adc10 & 0xFF);
    payload[5] = static_cast<uint8_t>((reading.adc10 >> 8) & 0xFF);
    payload[6] = static_cast<uint8_t>(telemetrySequence & 0xFF);
    payload[7] = static_cast<uint8_t>((telemetrySequence >> 8) & 0xFF);

    packet->decoded.payload.size = 8;
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

ProcessMessage SEN0308SoilMoistureModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (nodeDB == nullptr || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    const uint8_t *payload = mp.decoded.payload.bytes;
    const size_t payloadSize = mp.decoded.payload.size;
    char reply[230] = {};

    if (commandEquals(payload, payloadSize, "SOIL HELP")) {
        sendTextReply(mp.from, mp.channel,
                      "SOIL CMDS: SOIL | SOIL READ | SOIL STATUS | SOIL TX | SOIL CAL | SOIL HELP");
        return ProcessMessage::CONTINUE;
    }

    if (commandEquals(payload, payloadSize, "SOIL") || commandEquals(payload, payloadSize, "SOIL READ")) {
        const Reading reading = sample();
        snprintf(reply, sizeof(reply), "SOIL: %u%% ADC10=%u", reading.moisturePercent, reading.adc10);
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (commandEquals(payload, payloadSize, "SOIL STATUS")) {
        if (haveLastReading) {
            snprintf(reply, sizeof(reply),
                     "SOIL STATUS: %u%% ADC10=%u auto=1h pin=AIN1 dry=%u wet=%u",
                     lastReading.moisturePercent, lastReading.adc10, DRY_ADC10, WET_ADC10);
        } else {
            snprintf(reply, sizeof(reply),
                     "SOIL STATUS: waiting first sample auto=1h pin=AIN1 dry=%u wet=%u",
                     DRY_ADC10, WET_ADC10);
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (commandEquals(payload, payloadSize, "SOIL CAL")) {
        snprintf(reply, sizeof(reply),
                 "SOIL CAL: 0%%=ADC10 %u (super-dry soil), 100%%=ADC10 %u (saturated); higher ADC=drier",
                 DRY_ADC10, WET_ADC10);
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (commandEquals(payload, payloadSize, "SOIL TX")) {
        const Reading reading = sample();
        const bool standardOk = sendTelemetry(reading);
        const bool rawOk = sendRawPacket(reading);
        snprintf(reply, sizeof(reply), "SOIL TX: %u%% ADC10=%u telemetry=%s raw=%s",
                 reading.moisturePercent, reading.adc10,
                 standardOk ? "OK" : "FAIL", rawOk ? "OK" : "FAIL");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    return ProcessMessage::CONTINUE;
}

int32_t SEN0308SoilMoistureModule::runOnce()
{
    const Reading reading = sample();
    const bool standardOk = sendTelemetry(reading);
    const bool rawOk = sendRawPacket(reading);

    LOG_INFO("SEN0308 soil AUTO TX moisture=%u%% ADC10=%u standard=%s raw=%s",
             reading.moisturePercent, reading.adc10,
             standardOk ? "OK" : "FAIL", rawOk ? "OK" : "FAIL");

    return AUTO_INTERVAL_MS;
}

#endif
