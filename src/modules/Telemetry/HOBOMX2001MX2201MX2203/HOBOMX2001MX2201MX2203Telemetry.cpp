#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "HOBOMX2001MX2201MX2203Telemetry.h"

#include "../../../mesh/generated/meshtastic/telemetry.pb.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "SPILock.h"
#include "main.h"
#include "pb_encode.h"

#include <bluefruit.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{

static const uint8_t HOBO_SERVICE_UUID[16] = {
    0xCF, 0xCB, 0xE6, 0xBC,
    0xCC, 0x83,
    0x49, 0xAC,
    0x41, 0x46,
    0x4E, 0xED,
    0x4F, 0x6E,
    0xE1, 0x65
};

static const uint8_t HOBO_CHAR_UUID[16] = {
    0xCF, 0xCB, 0xE6, 0xBC,
    0xCC, 0x83,
    0x49, 0xAC,
    0x41, 0x46,
    0x4E, 0xED,
    0x4F, 0x6F,
    0xE1, 0x65
};

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

static const uint8_t CMD_INIT[] = {
    0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00
};

static const uint8_t CMD_NEWREAD64[] = {
    0x01, 0x01, 0x08, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t CMD_STATUS[] = {
    0x01, 0x01, 0x08, 0x04, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t CMD_MX2001_META0[] = {
    0x01, 0x01, 0x0A, 0x0A, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08
};

static const uint8_t CMD_MX2001_META8[] = {
    0x01, 0x01, 0x0A, 0x0A, 0x01,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x08
};

static const uint8_t CMD_MX2201_META0[] = {
    0x01, 0x01, 0x0A, 0x0A, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00
};

static const uint8_t CMD_MX2201_META8[] = {
    0x01, 0x01, 0x0A, 0x0A, 0x01,
    0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x08, 0x00
};

enum class LoggerType : uint8_t
{
    UNKNOWN = 0,
    MX2001,
    MX2201,
    MX2203,
    UNSUPPORTED
};

enum class MetaProfile : uint8_t
{
    NONE = 0,
    MX2001,
    MX2201
};

enum class ReadPurpose : uint8_t
{
    PROBE = 0,
    AUTOMATIC,
    ON_DEMAND
};

enum class UniversalState : uint8_t
{
    IDLE = 0,
    SEND_INIT,
    WAIT_INIT,
    SEND_META0,
    WAIT_META0,
    SEND_META8,
    WAIT_META8,
    SEND_READ,
    WAIT_READ,
    SEND_STATUS,
    WAIT_STATUS,
    READY
};

bool initialized = false;
bool connecting = false;
bool connected = false;
uint16_t connectionHandle = BLE_CONN_HANDLE_INVALID;

LoggerType loggerType = LoggerType::UNKNOWN;
MetaProfile activeMetaProfile = MetaProfile::NONE;
ReadPurpose readPurpose = ReadPurpose::PROBE;
UniversalState universalState = UniversalState::IDLE;
uint32_t stateDueMs = 0;

uint8_t probeAttempt = 0;
bool candidateLikelyMX2001 = false;
bool candidateLikelyMX2203 = false;

uint8_t loggerMac[6] = {};
uint8_t candidateAddrRaw[6] = {};
int8_t loggerBleRssi = 0;

uint8_t rejectedAddrRaw[6] = {};
bool haveRejectedAddr = false;
uint32_t rejectedUntilMs = 0;

bool loggerLockEnabled = false;
uint8_t lockedAddrRaw[6] = {};

bool directReadActive = false;
bool measurementReady = false;
bool measurementHasStage = false;
float latestTemperatureF = NAN;
float latestTemperatureC = NAN;
float latestStageMeters = NAN;
float latestStageFeet = NAN;
uint32_t latestTemperatureRaw = 0;

uint8_t mx2001Fragment1[20] = {};
uint8_t mx2001Fragment2[20] = {};
uint16_t mx2001Fragment1Length = 0;
uint16_t mx2001Fragment2Length = 0;
bool gotMX2001Fragment1 = false;
bool gotMX2001Fragment2 = false;

bool statusReady = false;
uint32_t currentWritePointer = 0;
uint32_t lastWritePointer = 0;
uint32_t pendingWritePointer = 0;
uint16_t loggerIntervalSeconds = 0;
bool haveStatusBaseline = false;
bool statusTrackingAvailable = true;
bool intervalPhaseLocked = false;
uint8_t consecutiveStatusTimeouts = 0;
uint32_t nextStatusCheckMs = 0;
uint32_t pendingPointerDetectedMs = 0;
uint32_t lastAutomaticTxMs = 0;
uint32_t automaticTxCount = 0;
uint16_t measurementSequence = 0;

bool readRequestPending = false;
bool readRequestInProgress = false;
bool readFailureReplyPending = false;
uint32_t readRequester = 0;
uint8_t readChannel = 0;

static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t READ_TIMEOUT_MS = 3000;
static constexpr uint32_t STATUS_TIMEOUT_MS = 3000;
static constexpr uint32_t POINTER_FINE_POLL_MS = 500;
static constexpr uint32_t POINTER_INITIAL_SYNC_POLL_MS = 1000;
static constexpr uint32_t STATUS_RECOVERY_RETRY_MS = 5000;
static constexpr uint8_t STATUS_TIMEOUT_LIMIT = 3;
static constexpr uint32_t REJECT_RETRY_MS = 60000;
static constexpr uint32_t TRANSIENT_RETRY_MS = 5000;
static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;
static constexpr char LOCK_FILE_PATH[] = "/prefs/hobo_lock.bin";

static constexpr uint32_t MX2201_MIN_RAW = 400;
static constexpr uint32_t MX2201_MAX_RAW = 2400;
static constexpr float MX2201_RAW_TO_F_SLOPE = 0.0771942720f;
static constexpr float MX2201_RAW_TO_F_INTERCEPT = -52.2825573f;

static constexpr float MX2203_CONST_A = 175.72f;
static constexpr float MX2203_FULL_RAW = 16384.0f;
static constexpr float MX2203_CONST_C = 46.85f;
static constexpr uint32_t MX2203_MAX_RAW = 16383;

bool reached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

uint32_t readBE32(const uint8_t *p)
{
    return
        (static_cast<uint32_t>(p[0]) << 24) |
        (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) |
        static_cast<uint32_t>(p[3]);
}

float readBEFloat(const uint8_t *p)
{
    const uint32_t bits = readBE32(p);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void makeHumanMac(const uint8_t in[6], uint8_t out[6])
{
    for (uint8_t i = 0; i < 6; ++i)
        out[i] = in[5 - i];
}

void logMac(const char *prefix, const uint8_t mac[6])
{
    LOG_INFO(
        "%s %02X:%02X:%02X:%02X:%02X:%02X",
        prefix,
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]);
}

uint8_t lockChecksum(const uint8_t *data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; ++i)
        checksum ^= data[i];
    return checksum;
}

void logLockTarget(const char *prefix)
{
    if (!loggerLockEnabled)
        return;

    uint8_t human[6] = {};
    makeHumanMac(lockedAddrRaw, human);
    logMac(prefix, human);
}

bool saveLoggerLock()
{
    uint8_t record[12] = {
        'H', 'B', 'L', '1', 1,
        0, 0, 0, 0, 0, 0,
        0
    };
    memcpy(&record[5], lockedAddrRaw, 6);
    record[11] = lockChecksum(record, 11);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(LOCK_FILE_PATH, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("HOBO universal: failed to open logger lock file for write");
        return false;
    }

    const size_t written = file.write(record, sizeof(record));
    file.flush();
    file.close();

    if (written != sizeof(record)) {
        LOG_WARN("HOBO universal: logger lock file short write");
        return false;
    }

    return true;
}

void clearLoggerLock()
{
    loggerLockEnabled = false;
    memset(lockedAddrRaw, 0, sizeof(lockedAddrRaw));

    concurrency::LockGuard g(spiLock);
    FSCom.remove(LOCK_FILE_PATH);
}

void loadLoggerLock()
{
    loggerLockEnabled = false;
    memset(lockedAddrRaw, 0, sizeof(lockedAddrRaw));

    uint8_t record[12] = {};
    size_t readLength = 0;

    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(LOCK_FILE_PATH, FILE_O_READ);
        if (!file)
            return;

        readLength = file.read(record, sizeof(record));
        file.close();
    }

    if (readLength != sizeof(record) ||
        record[0] != 'H' || record[1] != 'B' ||
        record[2] != 'L' || record[3] != '1' ||
        record[4] != 1 ||
        record[11] != lockChecksum(record, 11)) {
        LOG_WARN("HOBO universal: ignoring invalid logger lock file");
        return;
    }

    memcpy(lockedAddrRaw, &record[5], 6);
    loggerLockEnabled = true;
    logLockTarget("HOBO universal: restored logger lock:");
}

const char *loggerTypeName(LoggerType type)
{
    switch (type) {
    case LoggerType::MX2001:
        return "MX2001";
    case LoggerType::MX2201:
        return "MX2201";
    case LoggerType::MX2203:
        return "MX2203";
    case LoggerType::UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}

uint32_t nextRecordPrecheckDelayMs()
{
    if (loggerIntervalSeconds == 0)
        return POINTER_INITIAL_SYNC_POLL_MS;

    const uint32_t intervalMs =
        static_cast<uint32_t>(loggerIntervalSeconds) * 1000UL;

    if (intervalMs > 3000)
        return intervalMs - 2000;
    if (intervalMs > 1000)
        return intervalMs / 2;

    return 500;
}

bool containsAsciiIgnoreCase(const uint8_t *data, uint16_t length, const char *needle)
{
    if (data == nullptr || needle == nullptr)
        return false;

    const size_t needleLength = strlen(needle);
    if (needleLength == 0 || needleLength > length)
        return false;

    for (uint16_t i = 0; i + needleLength <= length; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLength; ++j) {
            const unsigned char a = static_cast<unsigned char>(data[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            if (std::toupper(a) != std::toupper(b)) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }

    return false;
}

struct CandidateInfo
{
    bool candidate;
    bool onsetManufacturer;
    bool hoboService;
    bool hoboName;
    bool likelyMX2001;
    bool likelyMX2203;
};

CandidateInfo inspectAdvertisement(const ble_gap_evt_adv_report_t *report)
{
    CandidateInfo result = {};

    if (report == nullptr || report->data.p_data == nullptr)
        return result;

    const uint8_t *data = report->data.p_data;
    const uint16_t len = report->data.len;
    uint16_t pos = 0;

    while (pos < len) {
        const uint8_t fieldLength = data[pos];
        if (fieldLength == 0)
            break;

        const uint16_t fieldEnd = static_cast<uint16_t>(pos + 1 + fieldLength);
        if (fieldEnd > len || fieldLength < 1)
            break;

        const uint8_t type = data[pos + 1];
        const uint8_t *payload = &data[pos + 2];
        const uint16_t payloadLength = static_cast<uint16_t>(fieldLength - 1);

        if (type == 0xFF && payloadLength >= 2 &&
            payload[0] == 0xC5 && payload[1] == 0x00) {
            result.onsetManufacturer = true;

            if (payloadLength == 22)
                result.likelyMX2001 = true;

            if (payloadLength >= 10 &&
                payload[6] == 0x01 && payload[7] == 0x03 &&
                payload[8] == 0x22 && payload[9] == 0x02) {
                result.likelyMX2203 = true;
            }
        }

        if ((type == 0x06 || type == 0x07) && payloadLength >= 16) {
            for (uint16_t offset = 0; offset + 16 <= payloadLength; offset += 16) {
                if (memcmp(payload + offset, HOBO_SERVICE_UUID, 16) == 0) {
                    result.hoboService = true;
                    break;
                }
            }
        }

        if ((type == 0x08 || type == 0x09) && payloadLength > 0) {
            if (containsAsciiIgnoreCase(payload, payloadLength, "HOBO") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2001") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2201") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2203")) {
                result.hoboName = true;
            }
        }

        pos = fieldEnd;
    }

    result.candidate =
        result.onsetManufacturer ||
        result.hoboService ||
        result.hoboName;

    return result;
}

bool isRejectedCandidate(const uint8_t rawAddr[6])
{
    if (!haveRejectedAddr)
        return false;

    if (reached(millis(), rejectedUntilMs)) {
        haveRejectedAddr = false;
        return false;
    }

    return memcmp(rawAddr, rejectedAddrRaw, 6) == 0;
}

void rejectCurrentCandidate(uint32_t retryMs = REJECT_RETRY_MS)
{
    memcpy(rejectedAddrRaw, candidateAddrRaw, 6);
    haveRejectedAddr = true;
    rejectedUntilMs = millis() + retryMs;
}

bool isCommand(const uint8_t *bytes, size_t size, const char *expected)
{
    if (bytes == nullptr || size == 0 || expected == nullptr)
        return false;

    char command[32] = {};
    size_t n = size;
    if (n > sizeof(command) - 1)
        n = sizeof(command) - 1;
    memcpy(command, bytes, n);

    char *p = command;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p == '/')
        ++p;

    const size_t expectedLength = strlen(expected);
    for (size_t i = 0; i < expectedLength; ++i) {
        if (p[i] == '\0')
            return false;
        if (std::toupper(static_cast<unsigned char>(p[i])) !=
            std::toupper(static_cast<unsigned char>(expected[i]))) {
            return false;
        }
    }

    p += expectedLength;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    return *p == '\0';
}

void resetMeasurementCapture()
{
    directReadActive = false;
    measurementReady = false;
    measurementHasStage = false;
    latestTemperatureF = NAN;
    latestTemperatureC = NAN;
    latestStageMeters = NAN;
    latestStageFeet = NAN;
    latestTemperatureRaw = 0;

    memset(mx2001Fragment1, 0, sizeof(mx2001Fragment1));
    memset(mx2001Fragment2, 0, sizeof(mx2001Fragment2));
    mx2001Fragment1Length = 0;
    mx2001Fragment2Length = 0;
    gotMX2001Fragment1 = false;
    gotMX2001Fragment2 = false;
}

bool sendCommand(const uint8_t *command, uint16_t length, const char *name)
{
    if (!connected)
        return false;

    const uint16_t written = hoboCharacteristic.write(command, length);
    if (written != length) {
        LOG_WARN(
            "HOBO universal: %s short write requested=%u written=%u",
            name,
            length,
            written);
        return false;
    }

    return true;
}

void finishMX2001Measurement()
{
    if (!gotMX2001Fragment1 || !gotMX2001Fragment2)
        return;

    if (mx2001Fragment1Length < 19 || mx2001Fragment2Length < 7) {
        LOG_WARN("HOBO universal: MX2001 fragments too short");
        directReadActive = false;
        return;
    }

    const uint16_t raw =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(mx2001Fragment1[17]) << 8) |
            static_cast<uint16_t>(mx2001Fragment1[18]));

    latestTemperatureRaw = raw;
    latestTemperatureF = -0.1805f * static_cast<float>(raw) + 169.64f;
    latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;
    latestStageMeters = readBEFloat(&mx2001Fragment2[3]);
    latestStageFeet = latestStageMeters * 3.280839895f;

    if (!isfinite(latestTemperatureF) || !isfinite(latestStageFeet) ||
        latestTemperatureF < -50.0f || latestTemperatureF > 180.0f ||
        latestStageFeet < -100.0f || latestStageFeet > 1000.0f) {
        LOG_WARN("HOBO universal: implausible MX2001 live measurement");
        directReadActive = false;
        return;
    }

    loggerType = LoggerType::MX2001;
    measurementHasStage = true;
    measurementReady = true;
    directReadActive = false;
}

void notifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;

    if (data == nullptr || len == 0)
        return;

    if (len >= 14 &&
        data[0] == 0x01 && data[1] == 0x02 &&
        data[2] == 0x04 && data[3] == 0x05) {

        currentWritePointer =
            (static_cast<uint32_t>(data[8]) << 24) |
            (static_cast<uint32_t>(data[9]) << 16) |
            (static_cast<uint32_t>(data[10]) << 8) |
            static_cast<uint32_t>(data[11]);

        loggerIntervalSeconds =
            static_cast<uint16_t>(
                (static_cast<uint16_t>(data[12]) << 8) |
                static_cast<uint16_t>(data[13]));

        statusReady = true;

        LOG_DEBUG(
            "HOBO universal STATUS model=%s pointer=0x%08lX interval=%u",
            loggerTypeName(loggerType),
            static_cast<unsigned long>(currentWritePointer),
            loggerIntervalSeconds);
        return;
    }

    if (!directReadActive)
        return;

    if (len >= 12 &&
        data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x0B &&
        data[3] == 0x04 && data[4] == 0x04 && data[5] == 0x00 &&
        data[6] == 0x04 && data[7] == 0x04) {

        const uint32_t raw = readBE32(&data[8]);
        if (raw > MX2203_MAX_RAW) {
            LOG_WARN(
                "HOBO universal: MX2203 raw value invalid %lu",
                static_cast<unsigned long>(raw));
            return;
        }

        latestTemperatureRaw = raw;
        latestTemperatureC =
            static_cast<float>(raw) * MX2203_CONST_A / MX2203_FULL_RAW -
            MX2203_CONST_C;
        latestTemperatureF = latestTemperatureC * (9.0f / 5.0f) + 32.0f;

        if (!isfinite(latestTemperatureC) || !isfinite(latestTemperatureF)) {
            LOG_WARN("HOBO universal: MX2203 temperature conversion invalid");
            return;
        }

        loggerType = LoggerType::MX2203;
        measurementHasStage = false;
        measurementReady = true;
        directReadActive = false;
        return;
    }

    if (len >= 12 &&
        data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x07 &&
        data[3] == 0x04 && data[4] == 0x04 && data[5] == 0x00 &&
        data[6] == 0x04 && data[7] == 0x04) {

        const uint32_t raw = readBE32(&data[8]);
        if (raw < MX2201_MIN_RAW || raw > MX2201_MAX_RAW) {
            LOG_WARN(
                "HOBO universal: MX2201 raw value implausible %lu",
                static_cast<unsigned long>(raw));
            return;
        }

        latestTemperatureRaw = raw;
        latestTemperatureF =
            MX2201_RAW_TO_F_SLOPE * static_cast<float>(raw) +
            MX2201_RAW_TO_F_INTERCEPT;
        latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;

        loggerType = LoggerType::MX2201;
        measurementHasStage = false;
        measurementReady = true;
        directReadActive = false;
        return;
    }

    if (len >= 20 &&
        data[0] == 0x01 && data[1] == 0x02 &&
        data[2] == 0x04 && data[3] == 0x04) {

        mx2001Fragment1Length =
            (len > sizeof(mx2001Fragment1)) ? sizeof(mx2001Fragment1) : len;
        memcpy(mx2001Fragment1, data, mx2001Fragment1Length);
        gotMX2001Fragment1 = true;
        loggerType = LoggerType::MX2001;
    } else if (len >= 7 && data[0] == 0x02) {
        mx2001Fragment2Length =
            (len > sizeof(mx2001Fragment2)) ? sizeof(mx2001Fragment2) : len;
        memcpy(mx2001Fragment2, data, mx2001Fragment2Length);
        gotMX2001Fragment2 = true;
    }

    if (gotMX2001Fragment1 && gotMX2001Fragment2)
        finishMX2001Measurement();
}

void scanCallback(ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr)
        return;

    if (connecting || connected) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (loggerLockEnabled &&
        memcmp(report->peer_addr.addr, lockedAddrRaw, 6) != 0) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (isRejectedCandidate(report->peer_addr.addr)) {
        Bluefruit.Scanner.resume();
        return;
    }

    const CandidateInfo info = inspectAdvertisement(report);
    if (!info.candidate) {
        Bluefruit.Scanner.resume();
        return;
    }

    memcpy(candidateAddrRaw, report->peer_addr.addr, 6);
    makeHumanMac(report->peer_addr.addr, loggerMac);
    loggerBleRssi = report->rssi;
    candidateLikelyMX2001 = info.likelyMX2001;
    candidateLikelyMX2203 = info.likelyMX2203;

    LOG_INFO(
        "HOBO universal: candidate %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d%s",
        loggerMac[0], loggerMac[1], loggerMac[2],
        loggerMac[3], loggerMac[4], loggerMac[5],
        loggerBleRssi,
        loggerLockEnabled ? " LOCKED-TARGET" : "");

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        LOG_WARN("HOBO universal: BLE connection request failed");
        Bluefruit.Scanner.start(0);
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connected = true;
    connectionHandle = connHandle;

    delay(SERVICE_SETTLE_MS);

    bool serviceFound = false;
    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        if (hoboService.discover(connHandle)) {
            serviceFound = true;
            break;
        }
        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!serviceFound) {
        LOG_WARN("HOBO universal: service discovery failed");
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        Bluefruit.disconnect(connHandle);
        return;
    }

    bool characteristicFound = false;
    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        if (hoboCharacteristic.discover()) {
            characteristicFound = true;
            break;
        }
        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!characteristicFound) {
        LOG_WARN("HOBO universal: characteristic discovery failed");
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        Bluefruit.disconnect(connHandle);
        return;
    }

    bool notifyEnabled = false;
    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        if (hoboCharacteristic.enableNotify()) {
            notifyEnabled = true;
            break;
        }
        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!notifyEnabled) {
        LOG_WARN("HOBO universal: notification enable failed");
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        Bluefruit.disconnect(connHandle);
        return;
    }

    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    readPurpose = ReadPurpose::PROBE;
    probeAttempt = 0;
    resetMeasurementCapture();

    statusReady = false;
    currentWritePointer = 0;
    lastWritePointer = 0;
    pendingWritePointer = 0;
    loggerIntervalSeconds = 0;
    haveStatusBaseline = false;
    statusTrackingAvailable = true;
    intervalPhaseLocked = false;
    consecutiveStatusTimeouts = 0;
    nextStatusCheckMs = 0;
    pendingPointerDetectedMs = 0;
    lastAutomaticTxMs = 0;
    automaticTxCount = 0;

    universalState = UniversalState::SEND_INIT;
    stateDueMs = millis() + 500;

    LOG_INFO("HOBO universal: BLE command channel ready");
    logMac("HOBO universal connected logger:", loggerMac);
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("HOBO universal: disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    connectionHandle = BLE_CONN_HANDLE_INVALID;
    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    readPurpose = ReadPurpose::PROBE;
    universalState = UniversalState::IDLE;
    resetMeasurementCapture();

    statusReady = false;
    currentWritePointer = 0;
    lastWritePointer = 0;
    pendingWritePointer = 0;
    loggerIntervalSeconds = 0;
    haveStatusBaseline = false;
    statusTrackingAvailable = true;
    intervalPhaseLocked = false;
    consecutiveStatusTimeouts = 0;
    nextStatusCheckMs = 0;
    pendingPointerDetectedMs = 0;
    lastAutomaticTxMs = 0;
    automaticTxCount = 0;

    if (readRequestInProgress || readRequestPending) {
        readRequestInProgress = false;
        readRequestPending = false;
        readFailureReplyPending = (readRequester != 0);
    }
}

void initializeClient()
{
    LOG_INFO("HOBO universal bridge: MX2001 + MX2201 + MX2203");

    loadLoggerLock();

    hoboService.begin();
    hoboCharacteristic.setNotifyCallback(notifyCallback);
    hoboCharacteristic.begin(&hoboService);

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(false);
    Bluefruit.Scanner.setInterval(160, 80);
    Bluefruit.Scanner.useActiveScan(false);

    initialized = true;

    if (!Bluefruit.Scanner.start(0))
        LOG_WARN("HOBO universal: scanner failed to start");
}

MetaProfile preferredMetaProfile()
{
    return candidateLikelyMX2001 ? MetaProfile::MX2001 : MetaProfile::MX2201;
}

MetaProfile alternateMetaProfile(MetaProfile profile)
{
    return profile == MetaProfile::MX2001 ? MetaProfile::MX2201 : MetaProfile::MX2001;
}

bool prepareFallbackProbe()
{
    if (candidateLikelyMX2203)
        return false;

    if (probeAttempt == 0) {
        probeAttempt = 1;
        activeMetaProfile = preferredMetaProfile();
        return true;
    }

    if (probeAttempt == 1) {
        probeAttempt = 2;
        activeMetaProfile = alternateMetaProfile(preferredMetaProfile());
        return true;
    }

    return false;
}

const uint8_t *meta0Command(size_t &length)
{
    if (activeMetaProfile == MetaProfile::MX2001) {
        length = sizeof(CMD_MX2001_META0);
        return CMD_MX2001_META0;
    }

    length = sizeof(CMD_MX2201_META0);
    return CMD_MX2201_META0;
}

const uint8_t *meta8Command(size_t &length)
{
    if (activeMetaProfile == MetaProfile::MX2001) {
        length = sizeof(CMD_MX2001_META8);
        return CMD_MX2001_META8;
    }

    length = sizeof(CMD_MX2201_META8);
    return CMD_MX2201_META8;
}

} // namespace

HOBOMX2001MX2201MX2203TelemetryModule::HOBOMX2001MX2201MX2203TelemetryModule()
    : SinglePortModule(
          "HOBOMX2001MX2201MX2203",
          meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread(
          "HOBOMX2001MX2201MX2203")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool HOBOMX2001MX2201MX2203TelemetryModule::wantPacket(
    const meshtastic_MeshPacket *p)
{
    if (p == nullptr)
        return false;

    return
        p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP ||
        p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOMX2001MX2201MX2203TelemetryModule::handleReceived(
    const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    if (nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "LOGGER")) {
        char reply[220] = {};
        uint8_t targetHuman[6] = {};
        if (loggerLockEnabled)
            makeHumanMac(lockedAddrRaw, targetHuman);

        if (!connected) {
            if (loggerLockEnabled) {
                snprintf(
                    reply,
                    sizeof(reply),
                    "HOBO NOT CONNECTED\nLock: ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X\nWaiting for target",
                    targetHuman[0], targetHuman[1], targetHuman[2],
                    targetHuman[3], targetHuman[4], targetHuman[5]);
            } else {
                snprintf(reply, sizeof(reply), "HOBO NOT CONNECTED\nLock: OFF");
            }
        } else if (loggerIntervalSeconds > 0) {
            if (loggerLockEnabled) {
                snprintf(
                    reply,
                    sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: %u sec\nLock: ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X",
                    loggerTypeName(loggerType),
                    loggerMac[0], loggerMac[1], loggerMac[2],
                    loggerMac[3], loggerMac[4], loggerMac[5],
                    loggerBleRssi,
                    loggerIntervalSeconds,
                    targetHuman[0], targetHuman[1], targetHuman[2],
                    targetHuman[3], targetHuman[4], targetHuman[5]);
            } else {
                snprintf(
                    reply,
                    sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: %u sec\nLock: OFF",
                    loggerTypeName(loggerType),
                    loggerMac[0], loggerMac[1], loggerMac[2],
                    loggerMac[3], loggerMac[4], loggerMac[5],
                    loggerBleRssi,
                    loggerIntervalSeconds);
            }
        } else {
            if (loggerLockEnabled) {
                snprintf(
                    reply,
                    sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: detecting\nLock: ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X",
                    loggerTypeName(loggerType),
                    loggerMac[0], loggerMac[1], loggerMac[2],
                    loggerMac[3], loggerMac[4], loggerMac[5],
                    loggerBleRssi,
                    targetHuman[0], targetHuman[1], targetHuman[2],
                    targetHuman[3], targetHuman[4], targetHuman[5]);
            } else {
                snprintf(
                    reply,
                    sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: detecting\nLock: OFF",
                    loggerTypeName(loggerType),
                    loggerMac[0], loggerMac[1], loggerMac[2],
                    loggerMac[3], loggerMac[4], loggerMac[5],
                    loggerBleRssi);
            }
        }

        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "LOCK")) {
        if (!connected ||
            (loggerType != LoggerType::MX2001 &&
             loggerType != LoggerType::MX2201 &&
             loggerType != LoggerType::MX2203)) {
            sendTextReply(
                mp.from,
                mp.channel,
                "LOCK failed: connect to an identified MX2001/MX2201/MX2203 first");
            return ProcessMessage::CONTINUE;
        }

        memcpy(lockedAddrRaw, candidateAddrRaw, 6);
        loggerLockEnabled = true;

        if (!saveLoggerLock()) {
            loggerLockEnabled = false;
            memset(lockedAddrRaw, 0, sizeof(lockedAddrRaw));
            sendTextReply(mp.from, mp.channel, "LOCK failed: could not save to flash");
            return ProcessMessage::CONTINUE;
        }

        char reply[150] = {};
        snprintf(
            reply,
            sizeof(reply),
            "LOGGER LOCKED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nPersists after reboot",
            loggerTypeName(loggerType),
            loggerMac[0], loggerMac[1], loggerMac[2],
            loggerMac[3], loggerMac[4], loggerMac[5]);
        sendTextReply(mp.from, mp.channel, reply);
        logLockTarget("HOBO universal: saved logger lock:");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "UNLOCK")) {
        clearLoggerLock();
        sendTextReply(
            mp.from,
            mp.channel,
            "LOGGER UNLOCKED\nScanning any supported HOBO\nCurrent BLE link will be released");

        if (connected && connectionHandle != BLE_CONN_HANDLE_INVALID)
            Bluefruit.disconnect(connectionHandle);

        return ProcessMessage::CONTINUE;
    }

    if (!isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "READ"))
        return ProcessMessage::CONTINUE;

    if (!connected) {
        sendTextReply(mp.from, mp.channel, "HOBO unavailable");
        return ProcessMessage::CONTINUE;
    }

    if (readRequestPending || readRequestInProgress) {
        sendTextReply(mp.from, mp.channel, "HOBO read already in progress");
        return ProcessMessage::CONTINUE;
    }

    readRequester = mp.from;
    readChannel = mp.channel;
    readRequestPending = true;
    setIntervalFromNow(10);

    return ProcessMessage::CONTINUE;
}

bool HOBOMX2001MX2201MX2203TelemetryModule::sendTextReply(
    uint32_t destination,
    uint8_t channel,
    const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("HOBO universal: text packet allocation failed");
        return false;
    }

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

int32_t HOBOMX2001MX2201MX2203TelemetryModule::runOnce()
{
    const uint32_t now = millis();

    auto sendTemperatureTelemetry = [&](float temperatureC) -> bool {
        meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
        telemetry.time = getTime();
        telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
        telemetry.variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;
        telemetry.variant.environment_metrics.has_temperature = true;
        telemetry.variant.environment_metrics.temperature = temperatureC;

        if (nodeDB != nullptr) {
            nodeDB->updateTelemetry(
                nodeDB->getNodeNum(),
                telemetry,
                RX_SRC_LOCAL);
        }

        meshtastic_MeshPacket *packet = allocDataPacket();
        if (packet == nullptr) {
            LOG_WARN("HOBO universal: telemetry packet allocation failed");
            return false;
        }

        const size_t encoded = pb_encode_to_bytes(
            packet->decoded.payload.bytes,
            sizeof(packet->decoded.payload.bytes),
            &meshtastic_Telemetry_msg,
            &telemetry);

        if (encoded == 0) {
            LOG_WARN("HOBO universal: telemetry protobuf encode failed");
            return false;
        }

        packet->decoded.payload.size = encoded;
        packet->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
        packet->decoded.want_response = false;
        packet->to = NODENUM_BROADCAST;
        packet->channel = 0;
        packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

        LOG_INFO(
            "HOBO universal TELEMETRY TX model=%s temp=%.2f C / %.2f F logger=%02X:%02X:%02X:%02X:%02X:%02X",
            loggerTypeName(loggerType),
            temperatureC,
            latestTemperatureF,
            loggerMac[0], loggerMac[1], loggerMac[2],
            loggerMac[3], loggerMac[4], loggerMac[5]);

        service->sendToMesh(packet, RX_SRC_LOCAL, true);
        return true;
    };

    auto sendMX2001MeasurementPacket = [&]() -> bool {
        meshtastic_MeshPacket *packet = allocDataPacket();
        if (packet == nullptr) {
            LOG_WARN("HOBO universal: MX2001 packet allocation failed");
            return false;
        }

        int32_t stageScaled = static_cast<int32_t>(lroundf(latestStageFeet * 10.0f));
        int32_t tempScaled = static_cast<int32_t>(lroundf(latestTemperatureF * 10.0f));

        if (stageScaled < -32768)
            stageScaled = -32768;
        if (stageScaled > 32767)
            stageScaled = 32767;
        if (tempScaled < -32768)
            tempScaled = -32768;
        if (tempScaled > 32767)
            tempScaled = 32767;

        const int16_t stageTenths = static_cast<int16_t>(stageScaled);
        const int16_t tempTenths = static_cast<int16_t>(tempScaled);
        const uint16_t stageBits = static_cast<uint16_t>(stageTenths);
        const uint16_t tempBits = static_cast<uint16_t>(tempTenths);
        const uint16_t raw16 = static_cast<uint16_t>(latestTemperatureRaw & 0xFFFFU);

        measurementSequence++;
        uint8_t *payload = packet->decoded.payload.bytes;

        payload[0] = 'M';
        payload[1] = 'X';
        payload[2] = 1;
        payload[3] = 0x03;
        payload[4] = static_cast<uint8_t>(measurementSequence & 0xFF);
        payload[5] = static_cast<uint8_t>((measurementSequence >> 8) & 0xFF);
        payload[6] = static_cast<uint8_t>(stageBits & 0xFF);
        payload[7] = static_cast<uint8_t>((stageBits >> 8) & 0xFF);
        payload[8] = static_cast<uint8_t>(tempBits & 0xFF);
        payload[9] = static_cast<uint8_t>((tempBits >> 8) & 0xFF);
        payload[10] = static_cast<uint8_t>(raw16 & 0xFF);
        payload[11] = static_cast<uint8_t>((raw16 >> 8) & 0xFF);
        memcpy(&payload[12], loggerMac, 6);
        payload[18] = static_cast<uint8_t>(loggerBleRssi);

        packet->decoded.payload.size = 19;
        packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
        packet->decoded.want_response = false;
        packet->to = NODENUM_BROADCAST;
        packet->channel = 0;
        packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

        LOG_INFO(
            "HOBO universal MX2001 TX sequence=%u level=%.1f ft temp=%.1f F logger=%02X:%02X:%02X:%02X:%02X:%02X",
            measurementSequence,
            stageTenths / 10.0f,
            tempTenths / 10.0f,
            loggerMac[0], loggerMac[1], loggerMac[2],
            loggerMac[3], loggerMac[4], loggerMac[5]);

        service->sendToMesh(packet, RX_SRC_LOCAL, true);
        return true;
    };

    auto sendAutomaticMeasurement = [&]() -> bool {
        if (loggerType == LoggerType::MX2001 && measurementHasStage)
            return sendMX2001MeasurementPacket();

        if (loggerType == LoggerType::MX2201 || loggerType == LoggerType::MX2203)
            return sendTemperatureTelemetry(latestTemperatureC);

        return false;
    };

    if (now < 15000)
        return 500;

    if (!initialized) {
        initializeClient();
        return 500;
    }

    if (readFailureReplyPending && readRequester != 0) {
        sendTextReply(readRequester, readChannel, "HOBO READ failed");
        readFailureReplyPending = false;
        readRequester = 0;
    }

    if (!connected) {
        if (connecting)
            return 500;

        if (!Bluefruit.Scanner.isRunning())
            Bluefruit.Scanner.start(0);

        return 500;
    }

    if (readRequestPending && universalState == UniversalState::READY) {
        readRequestPending = false;
        readRequestInProgress = true;
        readPurpose = ReadPurpose::ON_DEMAND;
        universalState = UniversalState::SEND_READ;
        stateDueMs = now;
    }

    switch (universalState) {
    case UniversalState::SEND_INIT:
        if (!reached(now, stateDueMs))
            break;

        if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
            universalState = UniversalState::WAIT_INIT;
            stateDueMs = now + COMMAND_DELAY_MS;
        } else {
            stateDueMs = now + 1000;
        }
        break;

    case UniversalState::WAIT_INIT:
        if (reached(now, stateDueMs)) {
            readPurpose = ReadPurpose::PROBE;
            universalState = UniversalState::SEND_READ;
        }
        break;

    case UniversalState::SEND_META0: {
        size_t length = 0;
        const uint8_t *command = meta0Command(length);
        const char *name =
            activeMetaProfile == MetaProfile::MX2001 ?
                "MX2001 META0" : "MX2201 META0";

        if (sendCommand(command, static_cast<uint16_t>(length), name)) {
            universalState = UniversalState::WAIT_META0;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case UniversalState::WAIT_META0:
        if (reached(now, stateDueMs))
            universalState = UniversalState::SEND_META8;
        break;

    case UniversalState::SEND_META8: {
        size_t length = 0;
        const uint8_t *command = meta8Command(length);
        const char *name =
            activeMetaProfile == MetaProfile::MX2001 ?
                "MX2001 META8" : "MX2201 META8";

        if (sendCommand(command, static_cast<uint16_t>(length), name)) {
            universalState = UniversalState::WAIT_META8;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case UniversalState::WAIT_META8:
        if (reached(now, stateDueMs)) {
            readPurpose = ReadPurpose::PROBE;
            universalState = UniversalState::SEND_READ;
        }
        break;

    case UniversalState::SEND_READ:
        resetMeasurementCapture();
        directReadActive = true;

        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            universalState = UniversalState::WAIT_READ;
            stateDueMs = now + READ_TIMEOUT_MS;
        } else {
            directReadActive = false;

            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                universalState = UniversalState::READY;
            } else {
                universalState = UniversalState::READY;
                nextStatusCheckMs = now + 1000;
            }
        }
        break;

    case UniversalState::WAIT_READ:
        if (measurementReady) {
            measurementReady = false;

            LOG_INFO(
                "HOBO universal: READ complete model=%s logger=%02X:%02X:%02X:%02X:%02X:%02X",
                loggerTypeName(loggerType),
                loggerMac[0], loggerMac[1], loggerMac[2],
                loggerMac[3], loggerMac[4], loggerMac[5]);

            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                char reply[190] = {};

                if (loggerType == LoggerType::MX2001 && measurementHasStage) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2001\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nLevel: %.2f ft\nTemp: %.1f F",
                        loggerMac[0], loggerMac[1], loggerMac[2],
                        loggerMac[3], loggerMac[4], loggerMac[5],
                        loggerBleRssi,
                        latestStageFeet,
                        latestTemperatureF);
                } else if (loggerType == LoggerType::MX2201) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2201\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nTemp: %.1f F",
                        loggerMac[0], loggerMac[1], loggerMac[2],
                        loggerMac[3], loggerMac[4], loggerMac[5],
                        loggerBleRssi,
                        latestTemperatureF);
                } else if (loggerType == LoggerType::MX2203) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2203\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nTemp: %.2f F / %.2f C",
                        loggerMac[0], loggerMac[1], loggerMac[2],
                        loggerMac[3], loggerMac[4], loggerMac[5],
                        loggerBleRssi,
                        latestTemperatureF,
                        latestTemperatureC);
                } else {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "HOBO read completed; model unknown\nLogger: %02X:%02X:%02X:%02X:%02X:%02X",
                        loggerMac[0], loggerMac[1], loggerMac[2],
                        loggerMac[3], loggerMac[4], loggerMac[5]);
                }

                sendTextReply(readRequester, readChannel, reply);
                readRequestInProgress = false;
                readRequester = 0;
                readPurpose = ReadPurpose::AUTOMATIC;
                universalState = UniversalState::READY;
                break;
            }

            if (readPurpose == ReadPurpose::PROBE) {
                LOG_INFO(
                    "HOBO universal: probe identified model=%s; automatic TX waits for logger STATUS pointer advance",
                    loggerTypeName(loggerType));

                readPurpose = ReadPurpose::AUTOMATIC;
                consecutiveStatusTimeouts = 0;
                statusTrackingAvailable = true;
                intervalPhaseLocked = false;
                universalState = UniversalState::SEND_STATUS;
                stateDueMs = now + 200;
                break;
            }

            if (readPurpose == ReadPurpose::AUTOMATIC) {
                if (!haveStatusBaseline || pendingWritePointer == lastWritePointer) {
                    LOG_WARN(
                        "HOBO universal: suppressing automatic TX without a confirmed new logger pointer");
                    nextStatusCheckMs = now + POINTER_FINE_POLL_MS;
                    universalState = UniversalState::READY;
                    break;
                }

                const bool sent = sendAutomaticMeasurement();

                if (sent) {
                    const uint32_t previousTxMs = lastAutomaticTxMs;
                    const uint32_t cadenceMs = previousTxMs == 0 ? 0 : now - previousTxMs;
                    const uint32_t pointerToTxMs =
                        pendingPointerDetectedMs == 0 ? 0 : now - pendingPointerDetectedMs;

                    automaticTxCount++;
                    lastAutomaticTxMs = now;
                    lastWritePointer = pendingWritePointer;
                    intervalPhaseLocked = true;

                    LOG_INFO(
                        "HOBO universal AUTO TX confirmed count=%lu pointer=0x%08lX interval=%u sec pointer_to_tx=%lu ms cadence=%lu ms",
                        static_cast<unsigned long>(automaticTxCount),
                        static_cast<unsigned long>(lastWritePointer),
                        loggerIntervalSeconds,
                        static_cast<unsigned long>(pointerToTxMs),
                        static_cast<unsigned long>(cadenceMs));

                    if (previousTxMs != 0 && loggerIntervalSeconds > 0) {
                        const uint32_t expectedMs =
                            static_cast<uint32_t>(loggerIntervalSeconds) * 1000UL;
                        const uint32_t driftMs =
                            cadenceMs > expectedMs ? cadenceMs - expectedMs : expectedMs - cadenceMs;

                        if (driftMs > 2000) {
                            LOG_WARN(
                                "HOBO universal AUTO TX cadence differs from logger interval by %lu ms (pointer-gated; no duplicate generated)",
                                static_cast<unsigned long>(driftMs));
                        }
                    }

                    nextStatusCheckMs = now + nextRecordPrecheckDelayMs();
                } else {
                    LOG_WARN(
                        "HOBO universal: automatic mesh enqueue failed; retaining pointer for retry");
                    nextStatusCheckMs = now + 1000;
                }

                pendingPointerDetectedMs = 0;
                universalState = UniversalState::READY;
                break;
            }
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;

            if (readPurpose == ReadPurpose::PROBE &&
                loggerType == LoggerType::UNKNOWN &&
                prepareFallbackProbe()) {
                universalState = UniversalState::SEND_META0;
                stateDueMs = now + 200;
                break;
            }

            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                readPurpose = ReadPurpose::AUTOMATIC;
                universalState = UniversalState::READY;
                break;
            }

            if (readPurpose == ReadPurpose::PROBE && loggerType == LoggerType::UNKNOWN) {
                loggerType = LoggerType::UNSUPPORTED;
                LOG_WARN("HOBO universal: unsupported NEWREAD64 response");
                rejectCurrentCandidate();
                Bluefruit.disconnect(connectionHandle);
                break;
            }

            LOG_WARN("HOBO universal: automatic NEWREAD64 timeout");
            readPurpose = ReadPurpose::AUTOMATIC;
            nextStatusCheckMs = now + 1000;
            universalState = UniversalState::READY;
        }
        break;

    case UniversalState::SEND_STATUS:
        statusReady = false;

        if (sendCommand(CMD_STATUS, sizeof(CMD_STATUS), "STATUS")) {
            universalState = UniversalState::WAIT_STATUS;
            stateDueMs = now + STATUS_TIMEOUT_MS;
        } else {
            consecutiveStatusTimeouts++;
            if (consecutiveStatusTimeouts >= STATUS_TIMEOUT_LIMIT) {
                statusTrackingAvailable = false;
                nextStatusCheckMs = now + STATUS_RECOVERY_RETRY_MS;
                LOG_WARN(
                    "HOBO universal: STATUS command unavailable; automatic TX PAUSED until pointer tracking recovers");
            } else {
                nextStatusCheckMs = now + 1000;
            }
            universalState = UniversalState::READY;
        }
        break;

    case UniversalState::WAIT_STATUS:
        if (statusReady) {
            statusReady = false;
            consecutiveStatusTimeouts = 0;
            statusTrackingAvailable = true;

            if (!haveStatusBaseline) {
                haveStatusBaseline = true;
                lastWritePointer = currentWritePointer;
                intervalPhaseLocked = false;

                LOG_INFO(
                    "HOBO universal: status baseline model=%s pointer=0x%08lX interval=%u sec; syncing to next true record boundary",
                    loggerTypeName(loggerType),
                    static_cast<unsigned long>(lastWritePointer),
                    loggerIntervalSeconds);

                nextStatusCheckMs = now + POINTER_INITIAL_SYNC_POLL_MS;
                universalState = UniversalState::READY;
                break;
            }

            if (currentWritePointer != lastWritePointer) {
                pendingWritePointer = currentWritePointer;
                pendingPointerDetectedMs = now;
                readPurpose = ReadPurpose::AUTOMATIC;

                LOG_INFO(
                    "HOBO universal: new logger record model=%s old=0x%08lX new=0x%08lX interval=%u sec phase=%s",
                    loggerTypeName(loggerType),
                    static_cast<unsigned long>(lastWritePointer),
                    static_cast<unsigned long>(currentWritePointer),
                    loggerIntervalSeconds,
                    intervalPhaseLocked ? "LOCKED" : "SYNCING");

                universalState = UniversalState::SEND_READ;
                stateDueMs = now;
            } else {
                nextStatusCheckMs = now +
                    (intervalPhaseLocked ? POINTER_FINE_POLL_MS : POINTER_INITIAL_SYNC_POLL_MS);
                universalState = UniversalState::READY;
            }
            break;
        }

        if (reached(now, stateDueMs)) {
            consecutiveStatusTimeouts++;
            LOG_WARN(
                "HOBO universal: STATUS timeout %u/%u",
                consecutiveStatusTimeouts,
                STATUS_TIMEOUT_LIMIT);

            if (consecutiveStatusTimeouts >= STATUS_TIMEOUT_LIMIT) {
                statusTrackingAvailable = false;
                nextStatusCheckMs = now + STATUS_RECOVERY_RETRY_MS;

                LOG_WARN(
                    "HOBO universal: write-pointer tracking unavailable; automatic TX PAUSED until STATUS recovers");

                universalState = UniversalState::READY;
            } else {
                universalState = UniversalState::SEND_STATUS;
                stateDueMs = now + 1000;
            }
        }
        break;

    case UniversalState::READY:
        if (readRequestPending)
            break;

        if (nextStatusCheckMs == 0 || reached(now, nextStatusCheckMs)) {
            universalState = UniversalState::SEND_STATUS;
            stateDueMs = now;
        }
        break;

    case UniversalState::IDLE:
    default:
        break;
    }

    return 100;
}

#endif
