#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "CCARockTelemetryModule.h"

#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "SPILock.h"
#include "main.h"

#include <Arduino.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Include this after the framework/Arduino headers so the CCA PIR D6 wrappers
// only affect this module's call sites. This keeps the rock packet's motion
// state/count on the same RF-filtered signal used by CCAStationModule.
#include "CCAStationModule.h"

CCARockTelemetryModule *ccaRockTelemetryModule;

namespace
{
constexpr char ROCK_FW_VERSION[] = "1.0.6";
constexpr uint8_t ROCK_PIN = A0;
constexpr uint8_t MOTION_PIN = D6;
constexpr uint32_t ROCK_INTERVAL_MS = 60UL * 1000UL;
constexpr uint32_t POLL_INTERVAL_MS = 100UL;
constexpr uint32_t FIRST_TELEMETRY_DELAY_MS = 2000UL;
constexpr uint8_t ROCK_SAMPLE_COUNT = 20;
constexpr uint8_t ROCK_SCHEMA_VERSION = 1;
constexpr uint16_t MIN_VALID_BATTERY_MV = 2500;
constexpr uint16_t MAX_VALID_BATTERY_MV = 5000;
constexpr uint16_t MIN_CAL_SPAN_ADC = 20;
constexpr char ROCK_CAL_FILE_PATH[] = "/prefs/cca_rock_cal.bin";
constexpr uint8_t ROCK_CAL_DRY_SET = 0x01;
constexpr uint8_t ROCK_CAL_WET_SET = 0x02;

struct __attribute__((packed)) RockCalibrationRecord
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t flags;
    uint16_t dryAdc;
    uint16_t wetAdc;
    uint8_t checksum;
};

RockCalibrationRecord rockCal = {{'R', 'K', 'C', '1'}, 1, 0, 0, 0, 0};

bool initialized = false;
bool lastMotionState = false;
uint32_t motionCount = 0;
uint32_t lastTelemetryMs = 0;
uint16_t latestAdc = 0;
uint16_t latestSensorMv = 0;
bool latestMotion = false;
uint16_t latestBatteryMv = 0;
uint8_t latestBatteryPct = 0;
uint32_t latestSampleMs = 0;

uint8_t checksumBytes(const uint8_t *bytes, size_t length)
{
    uint8_t value = 0;
    for (size_t i = 0; i < length; ++i)
        value ^= bytes[i];
    return value;
}

void resetRockCalibration()
{
    memset(&rockCal, 0, sizeof(rockCal));
    rockCal.magic[0] = 'R';
    rockCal.magic[1] = 'K';
    rockCal.magic[2] = 'C';
    rockCal.magic[3] = '1';
    rockCal.version = 1;
}

bool rockCalibrationRecordValid(const RockCalibrationRecord &record)
{
    if (record.magic[0] != 'R' || record.magic[1] != 'K' ||
        record.magic[2] != 'C' || record.magic[3] != '1' || record.version != 1)
        return false;

    return record.checksum ==
           checksumBytes(reinterpret_cast<const uint8_t *>(&record), sizeof(record) - 1);
}

bool saveRockCalibration()
{
    rockCal.checksum = checksumBytes(reinterpret_cast<const uint8_t *>(&rockCal), sizeof(rockCal) - 1);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(ROCK_CAL_FILE_PATH, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("CCA ROCK: failed to open calibration file for write");
        return false;
    }

    const size_t written = file.write(reinterpret_cast<const uint8_t *>(&rockCal), sizeof(rockCal));
    file.flush();
    file.close();

    if (written != sizeof(rockCal)) {
        LOG_WARN("CCA ROCK: calibration short write");
        return false;
    }
    return true;
}

void loadRockCalibration()
{
    RockCalibrationRecord candidate = {};
    size_t readLength = 0;

    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(ROCK_CAL_FILE_PATH, FILE_O_READ);
        if (file) {
            readLength = file.read(reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
            file.close();
        }
    }

    if (readLength == sizeof(candidate) && rockCalibrationRecordValid(candidate))
        rockCal = candidate;
    else
        resetRockCalibration();
}

bool calibrationReady()
{
    if ((rockCal.flags & (ROCK_CAL_DRY_SET | ROCK_CAL_WET_SET)) !=
        (ROCK_CAL_DRY_SET | ROCK_CAL_WET_SET))
        return false;

    const int32_t span = static_cast<int32_t>(rockCal.wetAdc) - static_cast<int32_t>(rockCal.dryAdc);
    const uint32_t absSpan = span < 0 ? static_cast<uint32_t>(-span) : static_cast<uint32_t>(span);
    return absSpan >= MIN_CAL_SPAN_ADC;
}

bool wetnessPercent(uint16_t adc, uint8_t &percent)
{
    if (!calibrationReady())
        return false;

    const int32_t dry = rockCal.dryAdc;
    const int32_t wet = rockCal.wetAdc;
    const int32_t span = wet - dry;
    int32_t value = ((static_cast<int32_t>(adc) - dry) * 100L) / span;

    if (value < 0)
        value = 0;
    if (value > 100)
        value = 100;

    percent = static_cast<uint8_t>(value);
    return true;
}

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

void normalizeCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize)
{
    if (outSize == 0)
        return;

    out[0] = '\0';
    if (bytes == nullptr || size == 0)
        return;

    size_t begin = 0;
    while (begin < size && std::isspace(static_cast<unsigned char>(bytes[begin])))
        ++begin;
    if (begin < size && bytes[begin] == '/')
        ++begin;

    size_t end = size;
    while (end > begin && std::isspace(static_cast<unsigned char>(bytes[end - 1])))
        --end;

    size_t j = 0;
    bool previousSpace = false;
    for (size_t i = begin; i < end && j + 1 < outSize; ++i) {
        const unsigned char c = bytes[i];
        if (std::isspace(c)) {
            if (!previousSpace && j > 0) {
                out[j++] = ' ';
                previousSpace = true;
            }
        } else {
            out[j++] = static_cast<char>(std::toupper(c));
            previousSpace = false;
        }
    }

    if (j > 0 && out[j - 1] == ' ')
        --j;
    out[j] = '\0';
}

void ensureInitialized()
{
    if (initialized)
        return;

#ifdef BATTERY_SENSE_RESOLUTION_BITS
    analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS);
#else
    analogReadResolution(10);
#endif
    pinMode(ROCK_PIN, INPUT);
    pinMode(MOTION_PIN, INPUT);
    loadRockCalibration();
    lastMotionState = digitalRead(MOTION_PIN) != 0;
    initialized = true;

    LOG_INFO("CCA ROCK %s: D0/A0 sandstone + RF-filtered D6 PIR; 60 s telemetry; persistent calibration; safe battery ADC",
             ROCK_FW_VERSION);
}

void sampleRock()
{
    ensureInitialized();
    latestAdc = readRockAverage();
    latestSensorMv = static_cast<uint16_t>((static_cast<uint32_t>(latestAdc) * 3300UL + 2047UL) / 4095UL);
    latestMotion = digitalRead(MOTION_PIN) != 0;
    latestBatteryMv = batteryMv();
    latestBatteryPct = batteryPercent(latestBatteryMv);
    latestSampleMs = millis();
}

void formatWetness(char *out, size_t outSize, uint16_t adc)
{
    uint8_t wet = 0;
    if (wetnessPercent(adc, wet))
        snprintf(out, outSize, "%u%%", wet);
    else if ((rockCal.flags & (ROCK_CAL_DRY_SET | ROCK_CAL_WET_SET)) !=
             (ROCK_CAL_DRY_SET | ROCK_CAL_WET_SET))
        snprintf(out, outSize, "CAL NEEDED");
    else
        snprintf(out, outSize, "CAL SPAN INVALID");
}

} // namespace

CCARockTelemetryModule::CCARockTelemetryModule()
    : SinglePortModule("CCARockTelemetry", meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread("CCARockTelemetry")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool CCARockTelemetryModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

bool CCARockTelemetryModule::sendText(uint32_t destination, uint8_t channel, const char *text, bool wantAck)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("CCA ROCK: text packet allocation failed");
        return false;
    }

    size_t length = strlen(text);
    if (length > sizeof(packet->decoded.payload.bytes))
        length = sizeof(packet->decoded.payload.bytes);

    memcpy(packet->decoded.payload.bytes, text, length);
    packet->decoded.payload.size = length;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = wantAck;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

ProcessMessage CCARockTelemetryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP || nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    char command[48] = {};
    normalizeCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, sizeof(command));
    if (command[0] == '\0')
        return ProcessMessage::CONTINUE;

    if (strcmp(command, "ROCK") != 0 && strncmp(command, "ROCK ", 5) != 0)
        return ProcessMessage::CONTINUE;

    ensureInitialized();
    char reply[230] = {};

    if (strcmp(command, "ROCK") == 0 || strcmp(command, "ROCK STATUS") == 0) {
        sampleRock();
        char wet[24] = {};
        formatWetness(wet, sizeof(wet), latestAdc);
        snprintf(reply, sizeof(reply),
                 "ROCK %s\nADC: %u / 4095\nSensor: %u mV\nWetness: %s\nMotion: %s | Count: %lu\nBattery: %u mV / %u%%\nAuto TX: 60s",
                 ROCK_FW_VERSION, latestAdc, latestSensorMv, wet,
                 latestMotion ? "YES" : "NO", static_cast<unsigned long>(motionCount),
                 latestBatteryMv, latestBatteryPct);
    } else if (strcmp(command, "ROCK ADC") == 0) {
        sampleRock();
        snprintf(reply, sizeof(reply), "ROCK ADC\nADC: %u / 4095\nSensor: %u mV", latestAdc, latestSensorMv);
    } else if (strcmp(command, "ROCK STATE") == 0) {
        sampleRock();
        char wet[24] = {};
        formatWetness(wet, sizeof(wet), latestAdc);
        snprintf(reply, sizeof(reply),
                 "ROCK STATE\nADC: %u\nSensor: %u mV\nWetness: %s\nScale: 0=dry 100=wet",
                 latestAdc, latestSensorMv, wet);
    } else if (strcmp(command, "ROCK NOW") == 0) {
        const bool sent = sendRockPacket();
        char wet[24] = {};
        formatWetness(wet, sizeof(wet), latestAdc);
        snprintf(reply, sizeof(reply),
                 "ROCK TX NOW: %s\nADC: %u\nSensor: %u mV\nWetness: %s",
                 sent ? "QUEUED" : "FAILED", latestAdc, latestSensorMv, wet);
    } else if (strcmp(command, "ROCK CAL DRY") == 0) {
        sampleRock();
        rockCal.dryAdc = latestAdc;
        rockCal.flags |= ROCK_CAL_DRY_SET;
        const bool saved = saveRockCalibration();
        snprintf(reply, sizeof(reply),
                 "ROCK CAL DRY\nADC: %u\nSensor: %u mV\nSaved: %s",
                 latestAdc, latestSensorMv, saved ? "YES" : "NO");
    } else if (strcmp(command, "ROCK CAL WET") == 0) {
        sampleRock();
        rockCal.wetAdc = latestAdc;
        rockCal.flags |= ROCK_CAL_WET_SET;
        const bool saved = saveRockCalibration();
        snprintf(reply, sizeof(reply),
                 "ROCK CAL WET\nADC: %u\nSensor: %u mV\nSaved: %s",
                 latestAdc, latestSensorMv, saved ? "YES" : "NO");
    } else if (strcmp(command, "ROCK CAL") == 0 || strcmp(command, "ROCK CAL STATUS") == 0) {
        sampleRock();
        char dry[18] = "NOT SET";
        char wetEndpoint[18] = "NOT SET";
        char wetness[24] = {};
        if (rockCal.flags & ROCK_CAL_DRY_SET)
            snprintf(dry, sizeof(dry), "%u", rockCal.dryAdc);
        if (rockCal.flags & ROCK_CAL_WET_SET)
            snprintf(wetEndpoint, sizeof(wetEndpoint), "%u", rockCal.wetAdc);
        formatWetness(wetness, sizeof(wetness), latestAdc);
        snprintf(reply, sizeof(reply),
                 "ROCK CAL\nDry ADC: %s\nWet ADC: %s\nNow ADC: %u\nWetness: %s\nPersistent: YES",
                 dry, wetEndpoint, latestAdc, wetness);
    } else if (strcmp(command, "ROCK CAL CLEAR") == 0) {
        resetRockCalibration();
        const bool saved = saveRockCalibration();
        snprintf(reply, sizeof(reply), "ROCK CAL CLEARED\nDry/Wet: NOT SET\nSaved: %s", saved ? "YES" : "NO");
    } else if (strcmp(command, "ROCK HELP") == 0) {
        snprintf(reply, sizeof(reply),
                 "ROCK COMMANDS\nROCK | ROCK STATUS | ROCK ADC\nROCK STATE | ROCK NOW\nROCK CAL DRY | ROCK CAL WET\nROCK CAL STATUS | ROCK CAL CLEAR");
    } else {
        snprintf(reply, sizeof(reply), "Unknown ROCK command\nSend ROCK HELP");
    }

    sendText(mp.from, mp.channel, reply, true);
    return ProcessMessage::CONTINUE;
}

bool CCARockTelemetryModule::sendRockPacket()
{
    sampleRock();

    uint8_t wetPct = 255;
    wetnessPercent(latestAdc, wetPct);

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("CCA ROCK: packet allocation failed");
        return false;
    }

    // 16-byte CCA rock packet, schema 1. Existing byte layout is preserved.
    // 0..1  = 'R','K'
    // 2     = schema version
    // 3     = flags (bit 0 = current validated motion)
    // 4..5  = averaged rock ADC on the established 0..4095 CCA calibration scale
    // 6..7  = sensor output millivolts (3.3 V reference)
    // 8..11 = validated motion rising-edge count since boot
    // 12..13= node battery millivolts; 0 means unavailable/invalid
    // 14    = node battery percent; 0 when battery voltage is unavailable
    // 15    = normalized rock wetness 0..100; 255 means dry/wet calibration unavailable
    uint8_t payload[16] = {};
    payload[0] = 'R';
    payload[1] = 'K';
    payload[2] = ROCK_SCHEMA_VERSION;
    payload[3] = latestMotion ? 0x01 : 0x00;
    writeLE16(&payload[4], latestAdc);
    writeLE16(&payload[6], latestSensorMv);
    writeLE32(&payload[8], motionCount);
    writeLE16(&payload[12], latestBatteryMv);
    payload[14] = latestBatteryPct;
    payload[15] = wetPct;

    memcpy(packet->decoded.payload.bytes, payload, sizeof(payload));
    packet->decoded.payload.size = sizeof(payload);
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->want_ack = false;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    lastTelemetryMs = millis();

    if (wetPct <= 100) {
        LOG_INFO("CCA ROCK: ADC=%u sensor=%u mV wetness=%u%% motion=%s count=%lu battery=%u mV/%u%%",
                 latestAdc, latestSensorMv, wetPct, latestMotion ? "YES" : "NO",
                 static_cast<unsigned long>(motionCount), latestBatteryMv, latestBatteryPct);
    } else {
        LOG_INFO("CCA ROCK: ADC=%u sensor=%u mV wetness=UNCAL motion=%s count=%lu battery=%u mV/%u%%",
                 latestAdc, latestSensorMv, latestMotion ? "YES" : "NO",
                 static_cast<unsigned long>(motionCount), latestBatteryMv, latestBatteryPct);
    }
    return true;
}

int32_t CCARockTelemetryModule::runOnce()
{
    const uint32_t now = millis();
    ensureInitialized();

    // Sample the same RF-filtered D6 signal as CCAStationModule. A LOW->HIGH
    // pulse that starts during this node's own LoRa TX or within 15 seconds
    // afterward is held LOW until the physical PIR output returns LOW.
    const bool motion = digitalRead(MOTION_PIN) != 0;
    if (motion && !lastMotionState)
        ++motionCount;
    lastMotionState = motion;

    if ((lastTelemetryMs == 0 && now >= FIRST_TELEMETRY_DELAY_MS) ||
        (lastTelemetryMs != 0 && now - lastTelemetryMs >= ROCK_INTERVAL_MS))
        sendRockPacket();

    return POLL_INTERVAL_MS;
}

#endif
