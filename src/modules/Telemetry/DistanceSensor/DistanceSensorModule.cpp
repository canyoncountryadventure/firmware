#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "DistanceSensorModule.h"

#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "SPILock.h"
#include "gps/RTC.h"
#include "main.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
static constexpr uint32_t CONFIG_MAGIC = 0x31545344UL; // "DST1" little-endian
static constexpr uint8_t CONFIG_VERSION = 1;
static constexpr char CONFIG_PATH[] = "/prefs/distance_sensor.bin";
static constexpr uint32_t BOOT_SETTLE_MS = 8000UL;
static constexpr uint32_t SENSOR_RETRY_MS = 30000UL;
static constexpr uint32_t TRAIL_SAMPLE_MS = 100UL;
static constexpr uint32_t OBSTRUCTION_MS = 30000UL;
static constexpr uint8_t BLOCK_CONFIRM_SAMPLES = 2;
static constexpr uint8_t CLEAR_CONFIRM_SAMPLES = 2;
static constexpr uint8_t COUNT_SAVE_EVERY = 5;
static constexpr uint32_t DEFAULT_REPORT_INTERVAL_SEC = 900UL;
static constexpr uint16_t DEFAULT_TRIGGER_MM = 305; // 12 in
static constexpr uint16_t DEFAULT_CLEAR_MM = 152;   // 6 in
static constexpr uint8_t DISTANCE_PACKET_VERSION = 1;

void putLE16(uint8_t *p, uint16_t value)
{
    p[0] = static_cast<uint8_t>(value & 0xFFU);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void putLE32(uint8_t *p, uint32_t value)
{
    p[0] = static_cast<uint8_t>(value & 0xFFU);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    p[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    p[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

const char *platformName()
{
#if defined(RAK_4631)
    return "RAK4631/RAK19007";
#else
    return "SEEED-XIAO";
#endif
}
} // namespace

DistanceSensorModule::DistanceSensorModule()
    : SinglePortModule("distance_sensor", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("distance_sensor"),
      uartDriver(DistanceSensorType::UART_GENERIC)
{
    isPromiscuous = true;
    setDefaults();
    setIntervalFromNow(1000);
}

const char *DistanceSensorModule::modeName(Mode mode)
{
    switch (mode) {
    case Mode::WATER:
        return "WATER";
    case Mode::TRAIL:
        return "TRAIL";
    default:
        return "IDLE";
    }
}

void DistanceSensorModule::setDefaults()
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    cfg.sensorType = static_cast<uint8_t>(DistanceSensorType::AUTO);
    cfg.mode = static_cast<uint8_t>(Mode::IDLE);
    cfg.triggerDeltaMm = DEFAULT_TRIGGER_MM;
    cfg.clearDeltaMm = DEFAULT_CLEAR_MM;
    cfg.reportIntervalSec = DEFAULT_REPORT_INTERVAL_SEC;
}

uint8_t DistanceSensorModule::configChecksum(const PersistentConfig &record) const
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(PersistentConfig) - 1; ++i)
        checksum ^= bytes[i];
    return checksum;
}

bool DistanceSensorModule::configValid(const PersistentConfig &record) const
{
    if (record.magic != CONFIG_MAGIC || record.version != CONFIG_VERSION)
        return false;
    if (record.sensorType > static_cast<uint8_t>(DistanceSensorType::UART_GENERIC))
        return false;
    if (record.mode > static_cast<uint8_t>(Mode::TRAIL))
        return false;
    if (record.reportIntervalSec < 5 || record.reportIntervalSec > 86400UL)
        return false;
    if (record.triggerDeltaMm < 20 || record.clearDeltaMm >= record.triggerDeltaMm)
        return false;
    return record.checksum == configChecksum(record);
}

bool DistanceSensorModule::loadConfig()
{
    PersistentConfig loaded = {};
    size_t readLength = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(CONFIG_PATH, FILE_O_READ);
        if (!file) {
            LOG_INFO("DistanceSensor: no saved config; using defaults");
            return false;
        }
        readLength = file.read(reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded));
        file.close();
    }

    if (readLength != sizeof(loaded) || !configValid(loaded)) {
        LOG_WARN("DistanceSensor: ignoring invalid saved config");
        return false;
    }

    cfg = loaded;
    LOG_INFO("DistanceSensor: restored mode=%s sensor=%s interval=%lus", modeName(static_cast<Mode>(cfg.mode)),
             distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)),
             static_cast<unsigned long>(cfg.reportIntervalSec));
    return true;
}

bool DistanceSensorModule::saveConfig()
{
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    cfg.checksum = configChecksum(cfg);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(CONFIG_PATH, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("DistanceSensor: failed to open config file for write");
        return false;
    }

    const size_t written = file.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg));
    file.flush();
    file.close();
    if (written != sizeof(cfg)) {
        LOG_WARN("DistanceSensor: short config write");
        return false;
    }
    return true;
}

bool DistanceSensorModule::autoDetectSensor()
{
    activeDriver = nullptr;
    activeType = DistanceSensorType::NONE;

    if (sen0590.probe()) {
        if (sen0590.begin()) {
            activeDriver = &sen0590;
            activeType = DistanceSensorType::SEN0590;
            LOG_INFO("DistanceSensor: AUTO detected SEN0590 on I2C 0x74");
            return true;
        }
    }

    uartDriver.setType(DistanceSensorType::UART_GENERIC);
    uartDriver.begin();
    const DistanceReading uartProbe = uartDriver.read();
    if (uartProbe.valid()) {
        activeDriver = &uartDriver;
        activeType = DistanceSensorType::UART_GENERIC;
        latestReading = uartProbe;
        LOG_INFO("DistanceSensor: AUTO detected compatible DFRobot UART ultrasonic sensor");
        return true;
    }

    LOG_WARN("DistanceSensor: AUTO found no supported distance sensor");
    return false;
}

bool DistanceSensorModule::configureSensor(DistanceSensorType requested, bool persistSelection)
{
    if (persistSelection) {
        cfg.sensorType = static_cast<uint8_t>(requested);
        saveConfig();
    }

    activeDriver = nullptr;
    activeType = DistanceSensorType::NONE;
    lastSensorAttemptMs = millis();

    if (requested == DistanceSensorType::AUTO)
        return autoDetectSensor();

    if (requested == DistanceSensorType::SEN0590) {
        if (sen0590.begin()) {
            activeDriver = &sen0590;
            activeType = requested;
            return true;
        }
        return false;
    }

    if (requested == DistanceSensorType::SEN0311_A02YYUW || requested == DistanceSensorType::SEN0313_A01NYUB ||
        requested == DistanceSensorType::UART_GENERIC) {
        uartDriver.setType(requested);
        uartDriver.begin();
        activeDriver = &uartDriver;
        activeType = requested;
        return true;
    }

    return false;
}

DistanceReading DistanceSensorModule::readDistance()
{
    if (activeDriver == nullptr) {
        DistanceReading missing;
        missing.status = DistanceReadStatus::NO_SENSOR;
        missing.measuredAtMs = millis();
        latestReading = missing;
        sensorReadErrors++;
        return missing;
    }

    latestReading = activeDriver->read();
    if (latestReading.valid())
        sensorReadSuccesses++;
    else
        sensorReadErrors++;

    latestStageValid = false;
    if (latestReading.valid() && (cfg.flags & FLAG_STAGE_CALIBRATED) != 0) {
        latestStageMm = cfg.stageReferenceMm - static_cast<int32_t>(latestReading.distanceMm);
        latestStageValid = true;
    }
    return latestReading;
}

void DistanceSensorModule::updateDailyRollover()
{
    const uint32_t validTime = getValidTime(RTCQualityDevice);
    if (validTime == 0)
        return;

    const uint32_t dayKey = validTime / SEC_PER_DAY;
    if (cfg.dayKey == 0) {
        cfg.dayKey = dayKey;
        saveConfig();
        return;
    }

    if (dayKey != cfg.dayKey) {
        cfg.dayKey = dayKey;
        cfg.dailyCount = 0;
        unsavedCountEvents = 0;
        saveConfig();
        LOG_INFO("DistanceSensor: UTC day rollover; daily trail count reset");
    }
}

void DistanceSensorModule::updateTrailState(const DistanceReading &reading)
{
    if (!reading.valid() || (cfg.flags & FLAG_TRAIL_CALIBRATED) == 0)
        return;

    if (cfg.trailBaselineMm <= cfg.triggerDeltaMm || cfg.trailBaselineMm <= cfg.clearDeltaMm)
        return;

    updateDailyRollover();

    const uint32_t blockThreshold = cfg.trailBaselineMm - cfg.triggerDeltaMm;
    const uint32_t clearThreshold = cfg.trailBaselineMm - cfg.clearDeltaMm;
    const uint32_t now = millis();

    if (!trailBlocked) {
        if (reading.distanceMm < blockThreshold) {
            if (blockedSamples == 0)
                lastEventMinDistanceMm = reading.distanceMm;
            else if (reading.distanceMm < lastEventMinDistanceMm)
                lastEventMinDistanceMm = reading.distanceMm;

            if (++blockedSamples >= BLOCK_CONFIRM_SAMPLES) {
                trailBlocked = true;
                blockedSinceMs = now;
                blockedSamples = 0;
                clearSamples = 0;
                obstruction = false;
                cfg.dailyCount++;
                cfg.lifetimeCount++;
                unsavedCountEvents++;

                LOG_INFO("DistanceSensor TRAIL count daily=%lu lifetime=%lu min=%lumm", static_cast<unsigned long>(cfg.dailyCount),
                         static_cast<unsigned long>(cfg.lifetimeCount), static_cast<unsigned long>(lastEventMinDistanceMm));

                if (unsavedCountEvents >= COUNT_SAVE_EVERY) {
                    saveConfig();
                    unsavedCountEvents = 0;
                }
            }
        } else {
            blockedSamples = 0;
        }
        return;
    }

    if (reading.distanceMm < lastEventMinDistanceMm)
        lastEventMinDistanceMm = reading.distanceMm;

    if (!obstruction && blockedSinceMs != 0 && (now - blockedSinceMs) >= OBSTRUCTION_MS) {
        obstruction = true;
        LOG_WARN("DistanceSensor TRAIL obstruction: beam blocked for >=30 sec");
    }

    if (reading.distanceMm >= clearThreshold) {
        if (++clearSamples >= CLEAR_CONFIRM_SAMPLES) {
            trailBlocked = false;
            blockedSinceMs = 0;
            clearSamples = 0;
            blockedSamples = 0;
            obstruction = false;
        }
    } else {
        clearSamples = 0;
    }
}

bool DistanceSensorModule::sendDistanceTelemetry(bool eventFlag)
{
    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("DistanceSensor: telemetry packet allocation failed");
        return false;
    }

    telemetrySequence++;
    uint8_t *payload = packet->decoded.payload.bytes;
    memset(payload, 0, 24);
    payload[0] = 'D';
    payload[1] = 'S';
    payload[2] = DISTANCE_PACKET_VERSION;
    payload[3] = cfg.mode;
    payload[4] = static_cast<uint8_t>(activeType);

    const uint32_t validTime = getValidTime(RTCQualityDevice);
    uint8_t flags = 0;
    if (latestReading.valid())
        flags |= 0x01;
    if ((static_cast<Mode>(cfg.mode) == Mode::WATER && (cfg.flags & FLAG_STAGE_CALIBRATED) != 0) ||
        (static_cast<Mode>(cfg.mode) == Mode::TRAIL && (cfg.flags & FLAG_TRAIL_CALIBRATED) != 0))
        flags |= 0x02;
    if (obstruction)
        flags |= 0x04;
    if (validTime != 0)
        flags |= 0x08;
    if (eventFlag)
        flags |= 0x10;
    payload[5] = flags;

    putLE16(&payload[6], telemetrySequence);
    putLE32(&payload[8], latestReading.valid() ? latestReading.distanceMm : 0U);

    if (static_cast<Mode>(cfg.mode) == Mode::WATER) {
        const int32_t stage = latestStageValid ? latestStageMm : INT32_MIN;
        putLE32(&payload[12], static_cast<uint32_t>(stage));
        putLE32(&payload[16], static_cast<uint32_t>(cfg.stageReferenceMm));
    } else {
        putLE32(&payload[12], cfg.dailyCount);
        putLE32(&payload[16], cfg.lifetimeCount);
    }
    putLE32(&payload[20], validTime);

    packet->decoded.payload.size = 24;
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);

    LOG_INFO("DistanceSensor TX mode=%s sensor=%s raw=%lumm seq=%u", modeName(static_cast<Mode>(cfg.mode)),
             distanceSensorTypeName(activeType), static_cast<unsigned long>(latestReading.distanceMm), telemetrySequence);
    return true;
}

bool DistanceSensorModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
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

bool DistanceSensorModule::calibrateTrail(uint32_t requester, uint8_t channel)
{
    if (activeDriver == nullptr) {
        sendTextReply(requester, channel, "CAL CLEAR failed: no active sensor");
        return false;
    }

    uint32_t samples[9] = {};
    size_t count = 0;
    for (uint8_t attempt = 0; attempt < 18 && count < 9; ++attempt) {
        const DistanceReading r = readDistance();
        if (r.valid())
            samples[count++] = r.distanceMm;
        delay(40);
    }

    if (count < 5) {
        sendTextReply(requester, channel, "CAL CLEAR failed: not enough valid readings. Check sensor/wiring and retry.");
        return false;
    }

    std::sort(samples, samples + count);
    const uint32_t median = samples[count / 2];
    if (median <= cfg.triggerDeltaMm + 50U) {
        sendTextReply(requester, channel, "CAL CLEAR failed: clear-path distance is too short for current trigger setting.");
        return false;
    }

    cfg.trailBaselineMm = median;
    cfg.flags |= FLAG_TRAIL_CALIBRATED;
    trailBlocked = false;
    obstruction = false;
    blockedSamples = 0;
    clearSamples = 0;
    saveConfig();

    char reply[190] = {};
    snprintf(reply, sizeof(reply), "CAL CLEAR saved\nBaseline: %.2f ft (%lumm)\nTrigger: %.1f in closer\nClear hysteresis: %.1f in",
             median / 304.8f, static_cast<unsigned long>(median), cfg.triggerDeltaMm / 25.4f, cfg.clearDeltaMm / 25.4f);
    sendTextReply(requester, channel, reply);
    return true;
}

bool DistanceSensorModule::calibrateStage(int32_t knownStageMm, uint32_t requester, uint8_t channel)
{
    if (activeDriver == nullptr) {
        sendTextReply(requester, channel, "CAL STAGE failed: no active sensor");
        return false;
    }

    uint32_t samples[7] = {};
    size_t count = 0;
    for (uint8_t attempt = 0; attempt < 14 && count < 7; ++attempt) {
        const DistanceReading r = readDistance();
        if (r.valid())
            samples[count++] = r.distanceMm;
        delay(40);
    }

    if (count < 5) {
        sendTextReply(requester, channel, "CAL STAGE failed: not enough valid readings. Check sensor/wiring and retry.");
        return false;
    }

    std::sort(samples, samples + count);
    const uint32_t rawMedian = samples[count / 2];
    const int64_t reference = static_cast<int64_t>(rawMedian) + static_cast<int64_t>(knownStageMm);
    if (reference < INT32_MIN || reference > INT32_MAX) {
        sendTextReply(requester, channel, "CAL STAGE failed: calibration value is outside supported range");
        return false;
    }

    cfg.stageReferenceMm = static_cast<int32_t>(reference);
    cfg.flags |= FLAG_STAGE_CALIBRATED;
    latestReading.distanceMm = rawMedian;
    latestReading.status = DistanceReadStatus::OK;
    latestStageMm = knownStageMm;
    latestStageValid = true;
    saveConfig();

    char reply[210] = {};
    snprintf(reply, sizeof(reply), "CAL STAGE saved\nRaw sensor distance: %.3f ft (%lumm)\nKnown stage: %.3f ft\nReference: %.3f ft",
             rawMedian / 304.8f, static_cast<unsigned long>(rawMedian), knownStageMm / 304.8f, cfg.stageReferenceMm / 304.8f);
    sendTextReply(requester, channel, reply);
    return true;
}

void DistanceSensorModule::replySensor(uint32_t requester, uint8_t channel)
{
    char reply[200] = {};
    snprintf(reply, sizeof(reply), "SENSOR\nConfigured: %s\nActive: %s\nInterface: %s\nLast: %s%s",
             distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)), distanceSensorTypeName(activeType),
             activeDriver ? activeDriver->interfaceName() : "NONE", distanceReadStatusName(latestReading.status),
             latestReading.valid() ? " (use READ for distance)" : "");
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replyCount(uint32_t requester, uint8_t channel)
{
    const bool timeSynced = getValidTime(RTCQualityDevice) != 0;
    char reply[200] = {};
    snprintf(reply, sizeof(reply), "TRAIL COUNT\nDaily: %lu\nLifetime: %lu\nState: %s\nObstruction: %s\nClock: %s",
             static_cast<unsigned long>(cfg.dailyCount), static_cast<unsigned long>(cfg.lifetimeCount),
             trailBlocked ? "BLOCKED" : "CLEAR", obstruction ? "YES" : "NO", timeSynced ? "SYNCED" : "UNSYNCED");
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replyRead(uint32_t requester, uint8_t channel, bool rawOnly)
{
    const DistanceReading r = readDistance();
    if (!r.valid()) {
        char reply[120] = {};
        snprintf(reply, sizeof(reply), "READ failed: %s", distanceReadStatusName(r.status));
        sendTextReply(requester, channel, reply);
        return;
    }

    char reply[210] = {};
    if (rawOnly) {
        snprintf(reply, sizeof(reply), "RAW: %lumm = %.2f cm = %.3f ft", static_cast<unsigned long>(r.distanceMm),
                 r.distanceMm / 10.0f, r.distanceMm / 304.8f);
    } else if (static_cast<Mode>(cfg.mode) == Mode::WATER) {
        if (latestStageValid) {
            snprintf(reply, sizeof(reply), "WATER READ\nRaw distance: %.3f ft (%lumm)\nStage: %.3f ft (%.1f cm)", r.distanceMm / 304.8f,
                     static_cast<unsigned long>(r.distanceMm), latestStageMm / 304.8f, latestStageMm / 10.0f);
        } else {
            snprintf(reply, sizeof(reply), "WATER READ\nRaw distance: %.3f ft (%lumm)\nStage: NOT CALIBRATED\nUse CAL STAGE <value>",
                     r.distanceMm / 304.8f, static_cast<unsigned long>(r.distanceMm));
        }
    } else if (static_cast<Mode>(cfg.mode) == Mode::TRAIL) {
        snprintf(reply, sizeof(reply), "TRAIL READ\nRaw distance: %.3f ft (%lumm)\nBaseline: %.3f ft\nDaily/Lifetime: %lu/%lu",
                 r.distanceMm / 304.8f, static_cast<unsigned long>(r.distanceMm), cfg.trailBaselineMm / 304.8f,
                 static_cast<unsigned long>(cfg.dailyCount), static_cast<unsigned long>(cfg.lifetimeCount));
    } else {
        snprintf(reply, sizeof(reply), "READ\nRaw distance: %.3f ft (%lumm)\nMode: IDLE", r.distanceMm / 304.8f,
                 static_cast<unsigned long>(r.distanceMm));
    }
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replyStatus(uint32_t requester, uint8_t channel)
{
    const Mode mode = static_cast<Mode>(cfg.mode);
    const bool calibrated = mode == Mode::WATER ? (cfg.flags & FLAG_STAGE_CALIBRATED) != 0
                                                : mode == Mode::TRAIL ? (cfg.flags & FLAG_TRAIL_CALIBRATED) != 0 : false;
    char reply[230] = {};
    snprintf(reply, sizeof(reply), "DISTANCE NODE %s\nMode:%s Sensor:%s\nCalibration:%s Interval:%lus\nLast:%s %lumm\nReads OK/ERR:%lu/%lu",
             platformName(), modeName(mode), distanceSensorTypeName(activeType), calibrated ? "VALID" : "NOT SET",
             static_cast<unsigned long>(cfg.reportIntervalSec), distanceReadStatusName(latestReading.status),
             static_cast<unsigned long>(latestReading.distanceMm), static_cast<unsigned long>(sensorReadSuccesses),
             static_cast<unsigned long>(sensorReadErrors));
    sendTextReply(requester, channel, reply);
}

bool DistanceSensorModule::normalizeCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize)
{
    if (bytes == nullptr || out == nullptr || outSize < 2 || size == 0)
        return false;

    size_t n = size;
    if (n >= outSize)
        n = outSize - 1;
    memcpy(out, bytes, n);
    out[n] = '\0';

    char *start = out;
    while (*start && std::isspace(static_cast<unsigned char>(*start)))
        ++start;
    if (*start == '/')
        ++start;

    if (start != out)
        memmove(out, start, strlen(start) + 1);

    size_t len = strlen(out);
    while (len > 0 && std::isspace(static_cast<unsigned char>(out[len - 1])))
        out[--len] = '\0';

    for (size_t i = 0; i < len; ++i)
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
    return len > 0;
}

bool DistanceSensorModule::parseDistanceMm(const char *text, int32_t &valueMm)
{
    if (text == nullptr)
        return false;

    while (*text == ' ' || *text == '\t')
        ++text;
    char *end = nullptr;
    const float value = strtof(text, &end);
    if (end == text || !std::isfinite(value))
        return false;

    while (*end == ' ' || *end == '\t')
        ++end;

    float multiplier = 0.0f;
    if (strcmp(end, "MM") == 0)
        multiplier = 1.0f;
    else if (strcmp(end, "CM") == 0)
        multiplier = 10.0f;
    else if (strcmp(end, "M") == 0)
        multiplier = 1000.0f;
    else if (strcmp(end, "IN") == 0)
        multiplier = 25.4f;
    else if (strcmp(end, "FT") == 0)
        multiplier = 304.8f;
    else
        return false;

    const double mm = static_cast<double>(value) * multiplier;
    if (mm < static_cast<double>(INT32_MIN) || mm > static_cast<double>(INT32_MAX))
        return false;
    valueMm = static_cast<int32_t>(lround(mm));
    return true;
}

bool DistanceSensorModule::parseIntervalSeconds(const char *text, uint32_t &seconds)
{
    if (text == nullptr)
        return false;
    while (*text == ' ' || *text == '\t')
        ++text;

    char *end = nullptr;
    const float value = strtof(text, &end);
    if (end == text || !std::isfinite(value) || value <= 0)
        return false;
    while (*end == ' ' || *end == '\t')
        ++end;

    float multiplier = 0.0f;
    if (strcmp(end, "S") == 0 || strcmp(end, "SEC") == 0 || strcmp(end, "SECS") == 0)
        multiplier = 1.0f;
    else if (strcmp(end, "M") == 0 || strcmp(end, "MIN") == 0 || strcmp(end, "MINS") == 0)
        multiplier = 60.0f;
    else if (strcmp(end, "H") == 0 || strcmp(end, "HR") == 0 || strcmp(end, "HRS") == 0)
        multiplier = 3600.0f;
    else
        return false;

    const double total = static_cast<double>(value) * multiplier;
    if (total < 5.0 || total > 86400.0)
        return false;
    seconds = static_cast<uint32_t>(lround(total));
    return true;
}

bool DistanceSensorModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage DistanceSensorModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (nodeDB == nullptr || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    char command[100] = {};
    if (!normalizeCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, sizeof(command)))
        return ProcessMessage::CONTINUE;

    if (strcmp(command, "HELP") == 0) {
        sendTextReply(mp.from, mp.channel,
                      "DIST CMDS: STATUS SENSOR [AUTO|SEN0590|SEN0311|SEN0313] MODE [WATER|TRAIL|IDLE] READ RAW CAL STAGE <v> CAL CLEAR CAL RESET TRIGGER <v> CLEAR <v> INTERVAL <v> COUNT RESET DAILY RESET COUNT TELEMETRY NOW | POWER WATCHDOG RECOVER");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "STATUS") == 0 || strcmp(command, "HEALTH") == 0) {
        replyStatus(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "SENSOR") == 0) {
        replySensor(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "SENSOR ", 7) == 0) {
        const char *arg = command + 7;
        DistanceSensorType requested = DistanceSensorType::NONE;
        if (strcmp(arg, "AUTO") == 0)
            requested = DistanceSensorType::AUTO;
        else if (strcmp(arg, "SEN0590") == 0)
            requested = DistanceSensorType::SEN0590;
        else if (strcmp(arg, "SEN0311") == 0 || strcmp(arg, "A02YYUW") == 0)
            requested = DistanceSensorType::SEN0311_A02YYUW;
        else if (strcmp(arg, "SEN0313") == 0 || strcmp(arg, "A01NYUB") == 0)
            requested = DistanceSensorType::SEN0313_A01NYUB;

        if (requested == DistanceSensorType::NONE) {
            sendTextReply(mp.from, mp.channel, "SENSOR options: AUTO, SEN0590, SEN0311/A02YYUW, SEN0313/A01NYUB");
        } else {
            const bool online = configureSensor(requested, true);
            char reply[150] = {};
            snprintf(reply, sizeof(reply), "SENSOR saved: %s\nActive now: %s", distanceSensorTypeName(requested),
                     online ? distanceSensorTypeName(activeType) : "NOT DETECTED (will retry)");
            sendTextReply(mp.from, mp.channel, reply);
        }
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "MODE ", 5) == 0) {
        const char *arg = command + 5;
        Mode newMode = Mode::IDLE;
        bool recognized = true;
        if (strcmp(arg, "WATER") == 0)
            newMode = Mode::WATER;
        else if (strcmp(arg, "TRAIL") == 0)
            newMode = Mode::TRAIL;
        else if (strcmp(arg, "IDLE") != 0 && strcmp(arg, "OFF") != 0)
            recognized = false;

        if (!recognized) {
            sendTextReply(mp.from, mp.channel, "MODE options: WATER, TRAIL, IDLE");
        } else {
            cfg.mode = static_cast<uint8_t>(newMode);
            trailBlocked = false;
            obstruction = false;
            blockedSamples = clearSamples = 0;
            saveConfig();
            char reply[100] = {};
            snprintf(reply, sizeof(reply), "MODE saved: %s", modeName(newMode));
            sendTextReply(mp.from, mp.channel, reply);
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "READ") == 0) {
        replyRead(mp.from, mp.channel, false);
        return ProcessMessage::CONTINUE;
    }
    if (strcmp(command, "RAW") == 0) {
        replyRead(mp.from, mp.channel, true);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "CAL STAGE ", 10) == 0) {
        int32_t mm = 0;
        if (!parseDistanceMm(command + 10, mm))
            sendTextReply(mp.from, mp.channel, "Use CAL STAGE with units, e.g. CAL STAGE 1.42FT or CAL STAGE 43CM");
        else
            calibrateStage(mm, mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL CLEAR") == 0) {
        calibrateTrail(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL RESET") == 0) {
        cfg.flags &= static_cast<uint8_t>(~(FLAG_STAGE_CALIBRATED | FLAG_TRAIL_CALIBRATED));
        cfg.stageReferenceMm = 0;
        cfg.trailBaselineMm = 0;
        latestStageValid = false;
        trailBlocked = false;
        obstruction = false;
        saveConfig();
        sendTextReply(mp.from, mp.channel, "CAL RESET: water and trail calibration cleared; counts preserved");
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "TRIGGER ", 8) == 0 || strncmp(command, "CLEAR ", 6) == 0) {
        const bool isTrigger = strncmp(command, "TRIGGER ", 8) == 0;
        const char *arg = command + (isTrigger ? 8 : 6);
        int32_t mm = 0;
        if (!parseDistanceMm(arg, mm) || mm < 20 || mm > 5000) {
            sendTextReply(mp.from, mp.channel, "Distance must include units, e.g. TRIGGER 12IN or CLEAR 6IN");
            return ProcessMessage::CONTINUE;
        }

        if (isTrigger) {
            if (mm <= cfg.clearDeltaMm) {
                sendTextReply(mp.from, mp.channel, "TRIGGER must be larger than CLEAR hysteresis distance");
                return ProcessMessage::CONTINUE;
            }
            cfg.triggerDeltaMm = static_cast<uint16_t>(mm);
        } else {
            if (mm >= cfg.triggerDeltaMm) {
                sendTextReply(mp.from, mp.channel, "CLEAR must be smaller than TRIGGER so hysteresis can work");
                return ProcessMessage::CONTINUE;
            }
            cfg.clearDeltaMm = static_cast<uint16_t>(mm);
        }
        saveConfig();
        char reply[110] = {};
        snprintf(reply, sizeof(reply), "%s saved: %.1f in (%dmm)", isTrigger ? "TRIGGER" : "CLEAR", mm / 25.4f, mm);
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "INTERVAL ", 9) == 0) {
        uint32_t seconds = 0;
        if (!parseIntervalSeconds(command + 9, seconds)) {
            sendTextReply(mp.from, mp.channel, "INTERVAL examples: 30SEC, 5MIN, 1HR (allowed 5 sec to 24 hr)");
        } else {
            cfg.reportIntervalSec = seconds;
            saveConfig();
            char reply[100] = {};
            snprintf(reply, sizeof(reply), "INTERVAL saved: %lu sec", static_cast<unsigned long>(seconds));
            sendTextReply(mp.from, mp.channel, reply);
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "COUNT") == 0) {
        replyCount(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "RESET DAILY") == 0) {
        cfg.dailyCount = 0;
        const uint32_t validTime = getValidTime(RTCQualityDevice);
        cfg.dayKey = validTime ? validTime / SEC_PER_DAY : 0;
        unsavedCountEvents = 0;
        saveConfig();
        sendTextReply(mp.from, mp.channel, "RESET DAILY: daily trail count set to 0; lifetime count preserved");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "RESET COUNT") == 0) {
        cfg.dailyCount = 0;
        cfg.lifetimeCount = 0;
        unsavedCountEvents = 0;
        saveConfig();
        sendTextReply(mp.from, mp.channel, "RESET COUNT: daily and lifetime trail counts set to 0");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "TELEMETRY NOW") == 0) {
        readDistance();
        const bool sent = sendDistanceTelemetry(false);
        sendTextReply(mp.from, mp.channel, sent ? "TELEMETRY NOW: packet queued" : "TELEMETRY NOW: packet allocation failed");
        return ProcessMessage::CONTINUE;
    }

    return ProcessMessage::CONTINUE;
}

int32_t DistanceSensorModule::runOnce()
{
    const uint32_t now = millis();
    if (now < BOOT_SETTLE_MS)
        return 1000;

    if (!moduleInitialized) {
        setDefaults();
        loadConfig();
        configureSensor(static_cast<DistanceSensorType>(cfg.sensorType), false);
        updateDailyRollover();
        lastReportMs = now;
        lastWaterReadMs = 0;
        moduleInitialized = true;
        LOG_INFO("DistanceSensor: ready platform=%s mode=%s configured=%s active=%s", platformName(),
                 modeName(static_cast<Mode>(cfg.mode)), distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)),
                 distanceSensorTypeName(activeType));
    }

    if (activeDriver == nullptr && (now - lastSensorAttemptMs) >= SENSOR_RETRY_MS)
        configureSensor(static_cast<DistanceSensorType>(cfg.sensorType), false);

    const Mode mode = static_cast<Mode>(cfg.mode);
    if (mode == Mode::IDLE)
        return 1000;

    if (mode == Mode::WATER) {
        const uint32_t intervalMs = cfg.reportIntervalSec * 1000UL;
        if (lastWaterReadMs == 0 || (now - lastWaterReadMs) >= intervalMs) {
            lastWaterReadMs = now;
            readDistance();
            sendDistanceTelemetry(false);
        }
        return 1000;
    }

    // TRAIL: sample locally at high rate, but transmit only a compact summary
    // at the configured interval.  This prevents raw 10 Hz samples from
    // flooding the LoRa mesh.
    const DistanceReading r = readDistance();
    updateTrailState(r);

    const uint32_t reportMs = cfg.reportIntervalSec * 1000UL;
    if ((now - lastReportMs) >= reportMs) {
        lastReportMs = now;
        sendDistanceTelemetry(false);
        if (unsavedCountEvents > 0) {
            saveConfig();
            unsavedCountEvents = 0;
        }
    }

    return TRAIL_SAMPLE_MS;
}

#endif
