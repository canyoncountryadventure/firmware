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
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
static constexpr uint32_t CONFIG_MAGIC = 0x32525457UL; // "WTR2" little-endian
static constexpr uint8_t CONFIG_VERSION = 2;
static constexpr char CONFIG_PATH_A[] = "/prefs/water_distance_a.bin";
static constexpr char CONFIG_PATH_B[] = "/prefs/water_distance_b.bin";
static constexpr char LEGACY_CONFIG_PATH[] = "/prefs/distance_sensor.bin";
static constexpr uint32_t LEGACY_CONFIG_MAGIC = 0x31545344UL; // "DST1"
static constexpr uint8_t LEGACY_CONFIG_VERSION = 1;

static constexpr uint32_t BOOT_SETTLE_MS = 8000UL;
static constexpr uint32_t SENSOR_RETRY_MS = 30000UL;
static constexpr uint32_t DEFAULT_REPORT_INTERVAL_SEC = 3600UL;
static constexpr uint8_t DISTANCE_PACKET_VERSION = 1;
static constexpr uint8_t WATER_MODE_VALUE = 1;

struct LegacyPersistentConfig
{
    uint32_t magic;
    uint8_t version;
    uint8_t sensorType;
    uint8_t mode;
    uint8_t flags;
    int32_t stageReferenceMm;
    uint32_t trailBaselineMm;
    uint16_t triggerDeltaMm;
    uint16_t clearDeltaMm;
    uint32_t reportIntervalSec;
    uint32_t dailyCount;
    uint32_t lifetimeCount;
    uint32_t dayKey;
    uint8_t checksum;
} __attribute__((packed));

uint8_t legacyChecksum(const LegacyPersistentConfig &record)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(LegacyPersistentConfig) - 1; ++i)
        checksum ^= bytes[i];
    return checksum;
}

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

bool sequenceNewer(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) > 0;
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

void DistanceSensorModule::setDefaults()
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    cfg.sensorType = static_cast<uint8_t>(DistanceSensorType::SEN0313_A01NYUB);
    cfg.reportIntervalSec = DEFAULT_REPORT_INTERVAL_SEC;
}

uint32_t DistanceSensorModule::configCrc32(const PersistentConfig &record) const
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < offsetof(PersistentConfig, crc32); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
    return ~crc;
}

bool DistanceSensorModule::configValid(const PersistentConfig &record) const
{
    if (record.magic != CONFIG_MAGIC || record.version != CONFIG_VERSION)
        return false;
    if (record.sensorType > static_cast<uint8_t>(DistanceSensorType::UART_GENERIC))
        return false;
    if ((record.flags & static_cast<uint8_t>(~(FLAG_CALIBRATED | FLAG_CAL_LOCKED))) != 0)
        return false;
    if (record.reportIntervalSec < 5UL || record.reportIntervalSec > 86400UL)
        return false;
    return record.crc32 == configCrc32(record);
}

bool DistanceSensorModule::readConfigSlot(const char *path, PersistentConfig &out) const
{
    if (path == nullptr)
        return false;

    size_t readLength = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(path, FILE_O_READ);
        if (!file)
            return false;
        if (file.size() != sizeof(out)) {
            file.close();
            return false;
        }
        readLength = file.read(reinterpret_cast<uint8_t *>(&out), sizeof(out));
        file.close();
    }
    return readLength == sizeof(out) && configValid(out);
}

bool DistanceSensorModule::saveConfig()
{
    PersistentConfig candidate = cfg;
    candidate.magic = CONFIG_MAGIC;
    candidate.version = CONFIG_VERSION;
    candidate.reserved0 = 0;
    candidate.sequence = cfg.sequence + 1U;
    candidate.crc32 = 0;
    candidate.crc32 = configCrc32(candidate);

    const int8_t targetSlot = activeConfigSlot == 0 ? 1 : 0;
    const char *targetPath = targetSlot == 0 ? CONFIG_PATH_A : CONFIG_PATH_B;

    size_t written = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(targetPath, FILE_O_WRITE);
        if (!file) {
            LOG_WARN("WaterDistance: failed to open config slot %c", targetSlot == 0 ? 'A' : 'B');
            return false;
        }

        // Adafruit LittleFS FILE_O_WRITE opens at EOF rather than truncating.
        // This is the inactive A/B slot, so reset it to exactly one record before writing.
        // The currently active slot remains untouched until this new record verifies.
        if (!file.seek(0) || !file.truncate()) {
            LOG_WARN("WaterDistance: failed to truncate config slot %c", targetSlot == 0 ? 'A' : 'B');
            file.close();
            return false;
        }

        written = file.write(reinterpret_cast<const uint8_t *>(&candidate), sizeof(candidate));
        file.flush();
        file.close();
    }

    if (written != sizeof(candidate)) {
        LOG_WARN("WaterDistance: short config write to slot %c", targetSlot == 0 ? 'A' : 'B');
        return false;
    }

    PersistentConfig verified = {};
    if (!readConfigSlot(targetPath, verified) || verified.sequence != candidate.sequence) {
        LOG_WARN("WaterDistance: config verification failed for slot %c", targetSlot == 0 ? 'A' : 'B');
        return false;
    }

    cfg = verified;
    activeConfigSlot = targetSlot;
    LOG_INFO("WaterDistance: config saved slot=%c seq=%lu", activeConfigSlot == 0 ? 'A' : 'B',
             static_cast<unsigned long>(cfg.sequence));
    return true;
}

bool DistanceSensorModule::migrateLegacyConfig()
{
    LegacyPersistentConfig legacy = {};
    size_t readLength = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(LEGACY_CONFIG_PATH, FILE_O_READ);
        if (!file)
            return false;
        readLength = file.read(reinterpret_cast<uint8_t *>(&legacy), sizeof(legacy));
        file.close();
    }

    if (readLength != sizeof(legacy) || legacy.magic != LEGACY_CONFIG_MAGIC || legacy.version != LEGACY_CONFIG_VERSION ||
        legacy.sensorType > static_cast<uint8_t>(DistanceSensorType::UART_GENERIC) ||
        legacy.reportIntervalSec < 5UL || legacy.reportIntervalSec > 86400UL || legacy.checksum != legacyChecksum(legacy))
        return false;

    setDefaults();
    cfg.sensorType = legacy.sensorType;
    cfg.reportIntervalSec = legacy.reportIntervalSec;
    if ((legacy.flags & 0x01U) != 0) {
        cfg.flags = FLAG_CALIBRATED | FLAG_CAL_LOCKED;
        cfg.stageReferenceMm = legacy.stageReferenceMm;
    }

    if (!saveConfig())
        return false;

    LOG_INFO("WaterDistance: migrated valid legacy config; calibration=%s",
             (cfg.flags & FLAG_CALIBRATED) ? "YES" : "NO");
    return true;
}

bool DistanceSensorModule::loadConfig()
{
    PersistentConfig slotA = {};
    PersistentConfig slotB = {};
    const bool validA = readConfigSlot(CONFIG_PATH_A, slotA);
    const bool validB = readConfigSlot(CONFIG_PATH_B, slotB);

    if (validA || validB) {
        if (validA && validB) {
            if (sequenceNewer(slotB.sequence, slotA.sequence)) {
                cfg = slotB;
                activeConfigSlot = 1;
            } else {
                cfg = slotA;
                activeConfigSlot = 0;
            }
        } else if (validA) {
            cfg = slotA;
            activeConfigSlot = 0;
        } else {
            cfg = slotB;
            activeConfigSlot = 1;
        }

        LOG_INFO("WaterDistance: restored slot=%c seq=%lu sensor=%s interval=%lus calibrated=%s locked=%s",
                 activeConfigSlot == 0 ? 'A' : 'B', static_cast<unsigned long>(cfg.sequence),
                 distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)),
                 static_cast<unsigned long>(cfg.reportIntervalSec), (cfg.flags & FLAG_CALIBRATED) ? "YES" : "NO",
                 (cfg.flags & FLAG_CAL_LOCKED) ? "YES" : "NO");
        return true;
    }

    setDefaults();
    activeConfigSlot = -1;
    if (migrateLegacyConfig())
        return true;

    LOG_INFO("WaterDistance: no valid config; creating defaults (A01NYUB, 1 hour, uncalibrated)");
    saveConfig();
    return false;
}

bool DistanceSensorModule::autoDetectSensor()
{
    activeDriver = nullptr;
    activeType = DistanceSensorType::NONE;

    if (sen0590.probe() && sen0590.begin()) {
        activeDriver = &sen0590;
        activeType = DistanceSensorType::SEN0590;
        LOG_INFO("WaterDistance: AUTO detected SEN0590");
        return true;
    }

    uartDriver.setType(DistanceSensorType::UART_GENERIC);
    uartDriver.begin();
    const DistanceReading probe = uartDriver.read();
    if (probe.valid()) {
        activeDriver = &uartDriver;
        activeType = DistanceSensorType::UART_GENERIC;
        latestReading = probe;
        LOG_INFO("WaterDistance: AUTO detected compatible UART ultrasonic");
        return true;
    }

    LOG_WARN("WaterDistance: AUTO found no supported sensor");
    return false;
}

bool DistanceSensorModule::configureSensor(DistanceSensorType requested, bool persistSelection)
{
    lastSensorAttemptMs = millis();
    activeDriver = nullptr;
    activeType = DistanceSensorType::NONE;

    if (persistSelection) {
        const uint8_t previous = cfg.sensorType;
        cfg.sensorType = static_cast<uint8_t>(requested);
        if (!saveConfig()) {
            cfg.sensorType = previous;
            return false;
        }
    }

    if (requested == DistanceSensorType::AUTO)
        return autoDetectSensor();

    if (requested == DistanceSensorType::SEN0590) {
        if (!sen0590.begin())
            return false;
        activeDriver = &sen0590;
        activeType = requested;
        return true;
    }

    if (requested == DistanceSensorType::SEN0311_A02YYUW || requested == DistanceSensorType::SEN0313_A01NYUB ||
        requested == DistanceSensorType::UART_GENERIC) {
        uartDriver.setType(requested);
        if (!uartDriver.begin())
            return false;
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
        latestStageValid = false;
        sensorReadErrors++;
        consecutiveReadErrors++;
        return missing;
    }

    latestReading = activeDriver->read();
    if (latestReading.valid()) {
        sensorReadSuccesses++;
        consecutiveReadErrors = 0;
    } else {
        sensorReadErrors++;
        consecutiveReadErrors++;
    }

    latestStageValid = false;
    if (latestReading.valid() && (cfg.flags & FLAG_CALIBRATED) != 0) {
        latestStageMm = cfg.stageReferenceMm - static_cast<int32_t>(latestReading.distanceMm);
        latestStageValid = true;
    }

    if (!latestReading.valid() && consecutiveReadErrors >= 3U && (millis() - lastSensorAttemptMs) >= 1000UL) {
        LOG_WARN("WaterDistance: 3 consecutive sensor errors; reinitializing %s",
                 distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)));
        configureSensor(static_cast<DistanceSensorType>(cfg.sensorType), false);
        consecutiveReadErrors = 0;
    }

    return latestReading;
}

bool DistanceSensorModule::takeMedianSamples(uint32_t *samples, size_t capacity, size_t required, size_t maxAttempts,
                                             uint32_t &medianMm, uint32_t &minMm, uint32_t &maxMm)
{
    if (samples == nullptr || required == 0 || required > capacity)
        return false;

    size_t count = 0;
    for (size_t attempt = 0; attempt < maxAttempts && count < required; ++attempt) {
        const DistanceReading r = readDistance();
        if (r.valid())
            samples[count++] = r.distanceMm;
        delay(80);
    }

    if (count < required)
        return false;

    std::sort(samples, samples + count);
    minMm = samples[0];
    maxMm = samples[count - 1];
    medianMm = samples[count / 2];
    return true;
}

bool DistanceSensorModule::sendDistanceTelemetry()
{
    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("WaterDistance: telemetry packet allocation failed");
        return false;
    }

    telemetrySequence++;
    uint8_t *payload = packet->decoded.payload.bytes;
    memset(payload, 0, 24);
    payload[0] = 'D';
    payload[1] = 'S';
    payload[2] = DISTANCE_PACKET_VERSION;
    payload[3] = WATER_MODE_VALUE;
    payload[4] = static_cast<uint8_t>(activeType);

    const uint32_t validTime = getValidTime(RTCQualityDevice);
    uint8_t flags = 0;
    if (latestReading.valid())
        flags |= 0x01;
    if ((cfg.flags & FLAG_CALIBRATED) != 0)
        flags |= 0x02;
    if (validTime != 0)
        flags |= 0x08;
    payload[5] = flags;

    putLE16(&payload[6], telemetrySequence);
    putLE32(&payload[8], latestReading.valid() ? latestReading.distanceMm : 0U);
    putLE32(&payload[12], static_cast<uint32_t>(latestStageValid ? latestStageMm : INT32_MIN));
    putLE32(&payload[16], static_cast<uint32_t>((cfg.flags & FLAG_CALIBRATED) ? cfg.stageReferenceMm : 0));
    putLE32(&payload[20], validTime);

    packet->decoded.payload.size = 24;
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);

    LOG_INFO("WaterDistance TX sensor=%s raw=%lumm stage=%ldmm valid=%s seq=%u",
             distanceSensorTypeName(activeType), static_cast<unsigned long>(latestReading.distanceMm),
             static_cast<long>(latestStageMm), latestStageValid ? "YES" : "NO", telemetrySequence);
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
    packet->want_ack = false;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

bool DistanceSensorModule::calibrateStage(int32_t knownStageMm, uint32_t requester, uint8_t channel)
{
    if ((cfg.flags & FLAG_CAL_LOCKED) != 0) {
        sendTextReply(requester, channel, "CAL LOCKED. To change it: CAL UNLOCK CONFIRM");
        return false;
    }
    if (activeDriver == nullptr) {
        sendTextReply(requester, channel, "CAL failed: no active sensor");
        return false;
    }

    uint32_t samples[9] = {};
    uint32_t median = 0;
    uint32_t minMm = 0;
    uint32_t maxMm = 0;
    if (!takeMedianSamples(samples, 9, 7, 14, median, minMm, maxMm)) {
        sendTextReply(requester, channel, "CAL failed: fewer than 7 valid fresh readings. Check sensor/wiring.");
        return false;
    }

    const int64_t reference = static_cast<int64_t>(median) + static_cast<int64_t>(knownStageMm);
    if (reference < INT32_MIN || reference > INT32_MAX) {
        sendTextReply(requester, channel, "CAL failed: reference is outside supported range");
        return false;
    }

    const PersistentConfig previous = cfg;
    cfg.stageReferenceMm = static_cast<int32_t>(reference);
    cfg.calibrationRawMm = median;
    cfg.calibrationStageMm = knownStageMm;
    cfg.calibrationUnix = getValidTime(RTCQualityDevice);
    cfg.flags |= FLAG_CALIBRATED | FLAG_CAL_LOCKED;

    if (!saveConfig()) {
        cfg = previous;
        sendTextReply(requester, channel, "CAL failed: could not verify persistent save");
        return false;
    }

    latestReading.status = DistanceReadStatus::OK;
    latestReading.distanceMm = median;
    latestReading.measuredAtMs = millis();
    latestStageMm = knownStageMm;
    latestStageValid = true;

    char reply[230] = {};
    snprintf(reply, sizeof(reply),
             "CAL SAVED + LOCKED\nRaw median: %.3f ft (%lumm)\nKnown stage: %.3f ft\nReference: %.3f ft\nSample spread: %lumm",
             median / 304.8f, static_cast<unsigned long>(median), knownStageMm / 304.8f,
             cfg.stageReferenceMm / 304.8f, static_cast<unsigned long>(maxMm - minMm));
    sendTextReply(requester, channel, reply);
    return true;
}

void DistanceSensorModule::replyStatus(uint32_t requester, uint8_t channel, bool refreshReading)
{
    if (refreshReading)
        readDistance();

    const bool calibrated = (cfg.flags & FLAG_CALIBRATED) != 0;
    const bool locked = (cfg.flags & FLAG_CAL_LOCKED) != 0;
    const bool ready = calibrated && locked && latestReading.valid() && activeDriver != nullptr;

    char reply[230] = {};
    if (latestStageValid) {
        snprintf(reply, sizeof(reply),
                 "WATER NODE %s\nReady:%s Sensor:%s\nCal:%s %s Interval:%lus\nLast:%s %lumm Stage:%.3fft\nReads OK/ERR:%lu/%lu",
                 platformName(), ready ? "YES" : "NO", distanceSensorTypeName(activeType),
                 calibrated ? "VALID" : "NOT SET", locked ? "LOCKED" : "UNLOCKED",
                 static_cast<unsigned long>(cfg.reportIntervalSec), distanceReadStatusName(latestReading.status),
                 static_cast<unsigned long>(latestReading.distanceMm), latestStageMm / 304.8f,
                 static_cast<unsigned long>(sensorReadSuccesses), static_cast<unsigned long>(sensorReadErrors));
    } else {
        snprintf(reply, sizeof(reply),
                 "WATER NODE %s\nReady:%s Sensor:%s\nCal:%s %s Interval:%lus\nLast:%s %lumm Stage:N/A\nReads OK/ERR:%lu/%lu",
                 platformName(), ready ? "YES" : "NO", distanceSensorTypeName(activeType),
                 calibrated ? "VALID" : "NOT SET", locked ? "LOCKED" : "UNLOCKED",
                 static_cast<unsigned long>(cfg.reportIntervalSec), distanceReadStatusName(latestReading.status),
                 static_cast<unsigned long>(latestReading.distanceMm), static_cast<unsigned long>(sensorReadSuccesses),
                 static_cast<unsigned long>(sensorReadErrors));
    }
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
    } else if (latestStageValid) {
        snprintf(reply, sizeof(reply), "WATER READ\nRaw: %.3f ft (%lumm)\nStage: %.3f ft (%.1f cm)",
                 r.distanceMm / 304.8f, static_cast<unsigned long>(r.distanceMm), latestStageMm / 304.8f,
                 latestStageMm / 10.0f);
    } else {
        snprintf(reply, sizeof(reply), "WATER READ\nRaw: %.3f ft (%lumm)\nStage: NOT CALIBRATED\nUse CAL STAGE <value>",
                 r.distanceMm / 304.8f, static_cast<unsigned long>(r.distanceMm));
    }
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replySensor(uint32_t requester, uint8_t channel)
{
    char reply[200] = {};
    snprintf(reply, sizeof(reply), "SENSOR\nConfigured:%s\nActive:%s\nInterface:%s\nLast:%s",
             distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)), distanceSensorTypeName(activeType),
             activeDriver ? activeDriver->interfaceName() : "NONE", distanceReadStatusName(latestReading.status));
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replyCalibration(uint32_t requester, uint8_t channel)
{
    const bool calibrated = (cfg.flags & FLAG_CALIBRATED) != 0;
    const bool locked = (cfg.flags & FLAG_CAL_LOCKED) != 0;
    char reply[220] = {};

    if (!calibrated) {
        snprintf(reply, sizeof(reply), "CAL STATUS\nCalibration:NOT SET\nLock:%s\nUse CAL STAGE <value><unit>",
                 locked ? "LOCKED" : "UNLOCKED");
    } else if (cfg.calibrationRawMm != 0) {
        snprintf(reply, sizeof(reply),
                 "CAL STATUS\nCalibration:VALID %s\nReference:%.3fft\nAt cal: raw %.3fft, stage %.3fft\nSaved seq:%lu",
                 locked ? "LOCKED" : "UNLOCKED", cfg.stageReferenceMm / 304.8f, cfg.calibrationRawMm / 304.8f,
                 cfg.calibrationStageMm / 304.8f, static_cast<unsigned long>(cfg.sequence));
    } else {
        snprintf(reply, sizeof(reply),
                 "CAL STATUS\nCalibration:VALID %s\nReference:%.3fft\nMigrated legacy calibration\nSaved seq:%lu",
                 locked ? "LOCKED" : "UNLOCKED", cfg.stageReferenceMm / 304.8f,
                 static_cast<unsigned long>(cfg.sequence));
    }
    sendTextReply(requester, channel, reply);
}

void DistanceSensorModule::replyVerify(uint32_t requester, uint8_t channel)
{
    uint32_t samples[7] = {};
    uint32_t median = 0;
    uint32_t minMm = 0;
    uint32_t maxMm = 0;
    if (!takeMedianSamples(samples, 7, 5, 10, median, minMm, maxMm)) {
        sendTextReply(requester, channel, "VERIFY failed: fewer than 5 valid fresh readings");
        return;
    }

    latestReading.status = DistanceReadStatus::OK;
    latestReading.distanceMm = median;
    latestReading.measuredAtMs = millis();
    latestStageValid = (cfg.flags & FLAG_CALIBRATED) != 0;
    if (latestStageValid)
        latestStageMm = cfg.stageReferenceMm - static_cast<int32_t>(median);

    char reply[220] = {};
    if (latestStageValid) {
        snprintf(reply, sizeof(reply), "VERIFY OK\nMedian:%lumm (%.3fft)\nRange:%lu-%lumm Spread:%lumm\nStage:%.3fft",
                 static_cast<unsigned long>(median), median / 304.8f, static_cast<unsigned long>(minMm),
                 static_cast<unsigned long>(maxMm), static_cast<unsigned long>(maxMm - minMm), latestStageMm / 304.8f);
    } else {
        snprintf(reply, sizeof(reply), "VERIFY OK\nMedian:%lumm (%.3fft)\nRange:%lu-%lumm Spread:%lumm\nStage:NOT CALIBRATED",
                 static_cast<unsigned long>(median), median / 304.8f, static_cast<unsigned long>(minMm),
                 static_cast<unsigned long>(maxMm), static_cast<unsigned long>(maxMm - minMm));
    }
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

    // Combined Water + HOBO firmware: only explicitly prefixed WATER commands
    // belong to this module. Bare READ/STATUS/etc. are reserved for HOBO and
    // other command modules, preventing command collisions and reply loops.
    if (strncmp(out, "WATER ", 6) == 0) {
        memmove(out, out + 6, strlen(out + 6) + 1);
    } else if (strcmp(out, "WATER") == 0) {
        strcpy(out, "HELP");
    } else {
        return false;
    }

    return out[0] != '\0';
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

    char command[110] = {};
    if (!normalizeCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, sizeof(command)))
        return ProcessMessage::CONTINUE;

    if (strcmp(command, "HELP") == 0) {
        sendTextReply(mp.from, mp.channel,
                      "WATER CMDS: STATUS CHECK SENSOR [A01NYUB|A02YYUW|SEN0590|AUTO] READ RAW VERIFY INTERVAL <v> CAL STAGE <v> CAL STATUS CAL LOCK CAL UNLOCK CONFIRM CAL RESET CONFIRM RESET WATER CONFIRM TELEMETRY NOW | POWER WATCHDOG REBOOT");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "STATUS") == 0 || strcmp(command, "CHECK") == 0 || strcmp(command, "INSTALL") == 0) {
        replyStatus(mp.from, mp.channel, true);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "SENSOR") == 0) {
        replySensor(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "SENSOR ", 7) == 0) {
        DistanceSensorType requested = DistanceSensorType::NONE;
        const char *arg = command + 7;
        if (strcmp(arg, "AUTO") == 0)
            requested = DistanceSensorType::AUTO;
        else if (strcmp(arg, "SEN0590") == 0)
            requested = DistanceSensorType::SEN0590;
        else if (strcmp(arg, "SEN0311") == 0 || strcmp(arg, "A02YYUW") == 0)
            requested = DistanceSensorType::SEN0311_A02YYUW;
        else if (strcmp(arg, "SEN0313") == 0 || strcmp(arg, "A01NYUB") == 0)
            requested = DistanceSensorType::SEN0313_A01NYUB;

        if (requested == DistanceSensorType::NONE) {
            sendTextReply(mp.from, mp.channel, "SENSOR options: A01NYUB, A02YYUW, SEN0590, AUTO");
            return ProcessMessage::CONTINUE;
        }

        const DistanceSensorType current = static_cast<DistanceSensorType>(cfg.sensorType);
        if (requested != current && (cfg.flags & FLAG_CAL_LOCKED) != 0) {
            sendTextReply(mp.from, mp.channel, "SENSOR change blocked: calibration is LOCKED. Use CAL UNLOCK CONFIRM or CAL RESET CONFIRM.");
            return ProcessMessage::CONTINUE;
        }

        if (requested != current && (cfg.flags & FLAG_CALIBRATED) != 0) {
            cfg.flags &= static_cast<uint8_t>(~FLAG_CALIBRATED);
            cfg.stageReferenceMm = 0;
            cfg.calibrationRawMm = 0;
            cfg.calibrationStageMm = 0;
            cfg.calibrationUnix = 0;
        }

        const bool online = configureSensor(requested, true);
        char reply[150] = {};
        snprintf(reply, sizeof(reply), "SENSOR saved:%s\nActive:%s", distanceSensorTypeName(requested),
                 online ? distanceSensorTypeName(activeType) : "NOT DETECTED (will retry)");
        sendTextReply(mp.from, mp.channel, reply);
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
    if (strcmp(command, "VERIFY") == 0) {
        replyVerify(mp.from, mp.channel);
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "INTERVAL ", 9) == 0) {
        uint32_t seconds = 0;
        if (!parseIntervalSeconds(command + 9, seconds)) {
            sendTextReply(mp.from, mp.channel, "INTERVAL examples: 15MIN, 1HR, 6H (allowed 5 sec to 24 hr)");
        } else {
            const uint32_t previous = cfg.reportIntervalSec;
            cfg.reportIntervalSec = seconds;
            if (!saveConfig()) {
                cfg.reportIntervalSec = previous;
                sendTextReply(mp.from, mp.channel, "INTERVAL failed: persistent save verification failed");
            } else {
                lastReportMs = millis();
                char reply[100] = {};
                snprintf(reply, sizeof(reply), "INTERVAL saved:%lu sec", static_cast<unsigned long>(seconds));
                sendTextReply(mp.from, mp.channel, reply);
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL STATUS") == 0) {
        replyCalibration(mp.from, mp.channel);
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

    if (strcmp(command, "CAL LOCK") == 0) {
        if ((cfg.flags & FLAG_CALIBRATED) == 0) {
            sendTextReply(mp.from, mp.channel, "CAL LOCK failed: no calibration exists");
        } else {
            const uint8_t previous = cfg.flags;
            cfg.flags |= FLAG_CAL_LOCKED;
            if (!saveConfig()) {
                cfg.flags = previous;
                sendTextReply(mp.from, mp.channel, "CAL LOCK failed: persistent save verification failed");
            } else {
                sendTextReply(mp.from, mp.channel, "CAL LOCKED");
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL UNLOCK CONFIRM") == 0) {
        const uint8_t previous = cfg.flags;
        cfg.flags &= static_cast<uint8_t>(~FLAG_CAL_LOCKED);
        if (!saveConfig()) {
            cfg.flags = previous;
            sendTextReply(mp.from, mp.channel, "CAL UNLOCK failed: persistent save verification failed");
        } else {
            sendTextReply(mp.from, mp.channel, "CAL UNLOCKED. Existing calibration remains active until replaced/reset.");
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL UNLOCK") == 0) {
        sendTextReply(mp.from, mp.channel, "Safety check: send CAL UNLOCK CONFIRM");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL RESET CONFIRM") == 0) {
        const PersistentConfig previous = cfg;
        cfg.flags &= static_cast<uint8_t>(~(FLAG_CALIBRATED | FLAG_CAL_LOCKED));
        cfg.stageReferenceMm = 0;
        cfg.calibrationRawMm = 0;
        cfg.calibrationStageMm = 0;
        cfg.calibrationUnix = 0;
        latestStageValid = false;
        if (!saveConfig()) {
            cfg = previous;
            sendTextReply(mp.from, mp.channel, "CAL RESET failed: persistent save verification failed");
        } else {
            sendTextReply(mp.from, mp.channel, "CAL RESET complete. Sensor/interval preserved; calibration cleared.");
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "CAL RESET") == 0) {
        sendTextReply(mp.from, mp.channel, "Safety check: send CAL RESET CONFIRM");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "RESET WATER CONFIRM") == 0) {
        const PersistentConfig previous = cfg;
        const uint32_t previousSequence = cfg.sequence;
        setDefaults();
        cfg.sequence = previousSequence;
        if (!saveConfig()) {
            cfg = previous;
            sendTextReply(mp.from, mp.channel, "RESET WATER failed: persistent save verification failed");
        } else {
            configureSensor(static_cast<DistanceSensorType>(cfg.sensorType), false);
            latestStageValid = false;
            lastReportMs = millis();
            sendTextReply(mp.from, mp.channel,
                          "WATER RESET complete: A01NYUB, 1HR, uncalibrated. Meshtastic identity/channels/keys untouched.");
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "RESET WATER") == 0) {
        sendTextReply(mp.from, mp.channel, "Safety check: send RESET WATER CONFIRM");
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "TELEMETRY NOW") == 0) {
        readDistance();
        const bool sent = sendDistanceTelemetry();
        sendTextReply(mp.from, mp.channel, sent ? "TELEMETRY NOW: packet queued" : "TELEMETRY NOW: allocation failed");
        return ProcessMessage::CONTINUE;
    }

    if (strncmp(command, "MODE", 4) == 0) {
        sendTextReply(mp.from, mp.channel, "This firmware is WATER-ONLY. WATER mode is permanent; no MODE command is needed.");
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
        const uint32_t intervalMs = cfg.reportIntervalSec * 1000UL;
        lastReportMs = now - intervalMs; // force one health/telemetry sample after boot
        moduleInitialized = true;
        LOG_INFO("WaterDistance: ready platform=%s sensor=%s interval=%lus calibrated=%s locked=%s",
                 platformName(), distanceSensorTypeName(static_cast<DistanceSensorType>(cfg.sensorType)),
                 static_cast<unsigned long>(cfg.reportIntervalSec), (cfg.flags & FLAG_CALIBRATED) ? "YES" : "NO",
                 (cfg.flags & FLAG_CAL_LOCKED) ? "YES" : "NO");
    }

    if (activeDriver == nullptr && (now - lastSensorAttemptMs) >= SENSOR_RETRY_MS)
        configureSensor(static_cast<DistanceSensorType>(cfg.sensorType), false);

    const uint32_t intervalMs = cfg.reportIntervalSec * 1000UL;
    if ((now - lastReportMs) >= intervalMs) {
        lastReportMs = now;
        readDistance();
        sendDistanceTelemetry();
    }

    return 1000;
}

#endif
