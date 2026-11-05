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
#include "mesh/Throttle.h"
#include "pb_encode.h"

#include <ble_gap.h>
#include <bluefruit.h>
#include <cctype>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace
{

static const uint8_t HOBO_SERVICE_UUID[16] = {0xCF, 0xCB, 0xE6, 0xBC, 0xCC, 0x83, 0x49, 0xAC,
                                              0x41, 0x46, 0x4E, 0xED, 0x4F, 0x6E, 0xE1, 0x65};

static const uint8_t HOBO_CHAR_UUID[16] = {0xCF, 0xCB, 0xE6, 0xBC, 0xCC, 0x83, 0x49, 0xAC,
                                           0x41, 0x46, 0x4E, 0xED, 0x4F, 0x6F, 0xE1, 0x65};

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

static const uint8_t CMD_INIT[] = {0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00};

static const uint8_t CMD_NEWREAD64[] = {0x01, 0x01, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t CMD_STATUS[] = {0x01, 0x01, 0x08, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t CMD_MX2001_META0[] = {0x01, 0x01, 0x0A, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08};

static const uint8_t CMD_MX2001_META8[] = {0x01, 0x01, 0x0A, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08};

static const uint8_t CMD_MX2201_META0[] = {0x01, 0x01, 0x0A, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00};

static const uint8_t CMD_MX2201_META8[] = {0x01, 0x01, 0x0A, 0x0A, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00};

enum class LoggerType : uint8_t { UNKNOWN = 0, MX2001, MX2201, MX2203, UNSUPPORTED };

enum class MetaProfile : uint8_t { NONE = 0, MX2001, MX2201 };

enum class ReadPurpose : uint8_t { PROBE = 0, AUTOMATIC, ON_DEMAND };

enum class UniversalState : uint8_t {
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
uint32_t connectAttemptStartedMs = 0;
uint8_t bleRecoveryCycles = 0;
// V5: monotonic, boot-local BLE evidence. Never erase on reconnect.
std::atomic<bool> manualBleRestartPending{false};
// Only the HOBO worker performs resets. DMs modify the interval atomically.
std::atomic<uint16_t> automaticBleResetMinutes{120};
uint32_t bleHealthTimerStartMs = 0;
uint32_t lastAutomaticBleResetMs = 0;
uint32_t automaticBleResetCount = 0;
uint32_t totalBleOnlyResetCount = 0;
uint32_t autoBleResetDeferredSinceMs = 0;
bool automaticBleResetPreviouslyAttempted = false;
bool hardwareResetScheduled = false;
uint32_t allAdvertisementCount = 0;
uint32_t matchingAddressCount = 0;
uint32_t hoboCandidateCount = 0;
uint32_t connectionAttemptCount = 0;
uint32_t connectionReadyCount = 0;
uint32_t scannerRestartCount = 0;
uint32_t disconnectedSinceMs = 0;
uint32_t lastScanRefreshMs = 0;
uint32_t scanBurstUntilMs = 0;
uint32_t lastTargetAdvertisementMs = 0;
uint32_t lastGoodStatusMs = 0;
uint32_t totalAutomaticTxCount = 0;
uint32_t lastAnyAutomaticTxMs = 0;
bool recoveryScanBurst = false;
std::atomic<bool> scanBurstRequested{false};
char lastBleFailure[48] = "none";
uint8_t consecutiveReadTimeouts = 0;

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
// Field v3: check the logger write pointer at a fixed 30-second cadence.
// This does not change the logger's own recording interval or radio TX interval.
static constexpr uint32_t HOBO_STATUS_POLL_MS = 30000UL;
static constexpr uint32_t POINTER_FINE_POLL_MS = HOBO_STATUS_POLL_MS;
static constexpr uint32_t POINTER_INITIAL_SYNC_POLL_MS = HOBO_STATUS_POLL_MS;
static constexpr uint32_t STATUS_RECOVERY_RETRY_MS = 5000;
static constexpr uint8_t STATUS_TIMEOUT_LIMIT = 3;
static constexpr uint8_t READ_TIMEOUT_LIMIT = 3;
static constexpr uint32_t CONNECT_ATTEMPT_TIMEOUT_MS = 30000UL;
static constexpr uint32_t REJECT_RETRY_MS = 60000;
static constexpr uint32_t TRANSIENT_RETRY_MS = 5000;
// V5: bounded high-duty scan only while the assigned logger is missing.
static constexpr uint32_t SCAN_REFRESH_MS = 5UL * 60UL * 1000UL;
static constexpr uint32_t HIGH_DUTY_SCAN_MS = 120UL * 1000UL;
// First deep reset after 15 minutes with no valid HOBO STATUS, then every
// configured interval (default 120 min) while still unhealthy.
static constexpr uint32_t FIRST_DEEP_BLE_RECOVERY_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t HARDWARE_RESET_GRACE_MS = 5UL * 60UL * 1000UL;
static constexpr uint32_t DEFER_BLE_RESET_WHILE_READ_MS = 10UL * 1000UL;
static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;
static constexpr char LOCK_FILE_PATH[] = "/prefs/hobo_lock.bin";
static constexpr char BLE_RECOVERY_FILE_PATH[] = "/prefs/hobo_ble_recovery.bin";

struct HoboHardwareResetGuard {
    uint32_t magic;
    uint32_t validation;
    uint32_t usedSinceVerifiedStatus;
};

static constexpr uint32_t HOBO_HARD_RESET_GUARD_MAGIC = 0x48425247UL; // HBRG
static constexpr uint32_t HOBO_HARD_RESET_GUARD_XOR = 0x5CD0E7A1UL;
__attribute__((section(".noinit"))) HoboHardwareResetGuard hoboHardwareResetGuard;

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

void noteBleFailure(const char *reason)
{
    snprintf(lastBleFailure, sizeof(lastBleFailure), "%s", reason ? reason : "unknown");
}

void requestHighDutyScanBurst()
{
    // The HOBO worker owns actual Bluefruit scanner calls. Callbacks only ask
    // it to replace the low-duty scan with a short recovery acquisition burst.
    scanBurstRequested = true;
}

// Only this HOBO state machine modifies the scanner. Do not interfere with
// an active link or an in-flight connect, including the SoftDevice link state.
void refreshDisconnectedScanner(bool fast)
{
    if (connected || connecting || Bluefruit.Central.connected())
        return;

    Bluefruit.Scanner.stop();
    Bluefruit.Scanner.setInterval(fast ? 160 : 320, fast ? 80 : 32); // 50% / 10% duty
    // HOBO identifiers can live in a scan response. Active scanning only
    // occurs during bounded recovery/boot acquisition windows.
    Bluefruit.Scanner.useActiveScan(fast);
    const bool started = Bluefruit.Scanner.start(0);
    scannerRestartCount++;
    lastScanRefreshMs = millis();
    recoveryScanBurst = fast;
    if (!started) {
        noteBleFailure("scanner start failed");
        LOG_WARN("HOBO V5: scanner start failed after refresh");
    }
}

void serviceDisconnectedScanner(uint32_t now)
{
    if (disconnectedSinceMs == 0)
        disconnectedSinceMs = now;

    if (scanBurstRequested.exchange(false)) {
        refreshDisconnectedScanner(true);
        scanBurstUntilMs = now + HIGH_DUTY_SCAN_MS;
        return;
    }

    if (recoveryScanBurst && reached(now, scanBurstUntilMs)) {
        refreshDisconnectedScanner(false);
        return;
    }

    if (!recoveryScanBurst && (now - disconnectedSinceMs) >= SCAN_REFRESH_MS &&
        (now - lastScanRefreshMs) >= SCAN_REFRESH_MS) {
        refreshDisconnectedScanner(true);
        scanBurstUntilMs = now + HIGH_DUTY_SCAN_MS;
        return;
    }

    if (!Bluefruit.Scanner.isRunning() && (now - lastScanRefreshMs) >= 5000UL)
        refreshDisconnectedScanner(false);
}

uint32_t readBE32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) |
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
    LOG_INFO("%s %02X:%02X:%02X:%02X:%02X:%02X", prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint8_t lockChecksum(const uint8_t *data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; ++i)
        checksum ^= data[i];
    return checksum;
}

uint32_t hoboHardwareResetValidation(uint32_t usedSinceVerifiedStatus)
{
    return HOBO_HARD_RESET_GUARD_MAGIC ^ HOBO_HARD_RESET_GUARD_XOR ^ usedSinceVerifiedStatus;
}

void initializeHoboHardwareResetGuard()
{
    if (hoboHardwareResetGuard.magic == HOBO_HARD_RESET_GUARD_MAGIC &&
        hoboHardwareResetGuard.usedSinceVerifiedStatus <= 1 &&
        hoboHardwareResetGuard.validation == hoboHardwareResetValidation(hoboHardwareResetGuard.usedSinceVerifiedStatus))
        return;

    hoboHardwareResetGuard.magic = HOBO_HARD_RESET_GUARD_MAGIC;
    hoboHardwareResetGuard.usedSinceVerifiedStatus = 0;
    hoboHardwareResetGuard.validation = hoboHardwareResetValidation(0);
}

bool hoboHardwareResetAlreadyUsed()
{
    initializeHoboHardwareResetGuard();
    return hoboHardwareResetGuard.usedSinceVerifiedStatus != 0;
}

void markHoboHardwareResetUsed()
{
    initializeHoboHardwareResetGuard();
    hoboHardwareResetGuard.usedSinceVerifiedStatus = 1;
    hoboHardwareResetGuard.validation = hoboHardwareResetValidation(1);
}

void clearHoboHardwareResetGuard()
{
    initializeHoboHardwareResetGuard();
    hoboHardwareResetGuard.usedSinceVerifiedStatus = 0;
    hoboHardwareResetGuard.validation = hoboHardwareResetValidation(0);
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
    uint8_t record[12] = {'H', 'B', 'L', '1', 1, 0, 0, 0, 0, 0, 0, 0};
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

    if (readLength != sizeof(record) || record[0] != 'H' || record[1] != 'B' || record[2] != 'L' || record[3] != '1' ||
        record[4] != 1 || record[11] != lockChecksum(record, 11)) {
        LOG_WARN("HOBO universal: ignoring invalid logger lock file");
        return;
    }

    memcpy(lockedAddrRaw, &record[5], 6);
    loggerLockEnabled = true;
    logLockTarget("HOBO universal: restored logger lock:");
}

// Separate CRC-checked configuration file; never touch the persistent
// logger lock, Meshtastic preferences or HOBO recording interval.
bool saveAutomaticBleResetMinutes(uint16_t minutes)
{
    uint8_t record[8] = {'H', 'B', 'R', '1', 1,
                         static_cast<uint8_t>(minutes & 0xff),
                         static_cast<uint8_t>((minutes >> 8) & 0xff), 0};
    record[7] = lockChecksum(record, 7);

    concurrency::LockGuard g(spiLock);
    // LittleFS FILE_O_WRITE appends on nRF52. Replace the fixed record rather
    // than leaving an older interval at the front of the file.
    FSCom.remove(BLE_RECOVERY_FILE_PATH);
    File file = FSCom.open(BLE_RECOVERY_FILE_PATH, FILE_O_WRITE);
    if (!file)
        return false;
    const size_t written = file.write(record, sizeof(record));
    file.flush();
    file.close();
    return written == sizeof(record);
}

void loadAutomaticBleResetMinutes()
{
    uint8_t record[8] = {};
    size_t length = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(BLE_RECOVERY_FILE_PATH, FILE_O_READ);
        if (file) {
            length = file.read(record, sizeof(record));
            file.close();
        }
    }
    if (length != sizeof(record) || memcmp(record, "HBR1", 4) != 0 || record[4] != 1 ||
        record[7] != lockChecksum(record, 7))
        return; // Default: 120 minutes.

    const uint16_t minutes = static_cast<uint16_t>(record[5]) |
                             (static_cast<uint16_t>(record[6]) << 8);
    if (minutes == 0 || (minutes >= 15 && minutes <= 720))
        automaticBleResetMinutes.store(minutes);
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
    // Do not switch to the old near-record 500 ms/1 s burst schedule.
    // All healthy connected STATUS checks run on the 30-second cadence.
    return HOBO_STATUS_POLL_MS;
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

struct CandidateInfo {
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

        if (type == 0xFF && payloadLength >= 2 && payload[0] == 0xC5 && payload[1] == 0x00) {
            result.onsetManufacturer = true;

            if (payloadLength == 22)
                result.likelyMX2001 = true;

            if (payloadLength >= 10 && payload[6] == 0x01 && payload[7] == 0x03 && payload[8] == 0x22 && payload[9] == 0x02) {
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

    result.candidate = result.onsetManufacturer || result.hoboService || result.hoboName;

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
        if (std::toupper(static_cast<unsigned char>(p[i])) != std::toupper(static_cast<unsigned char>(expected[i]))) {
            return false;
        }
    }

    p += expectedLength;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    return *p == '\0';
}

// Recognize BLE RESET AUTO <minutes|OFF> without matching unrelated messages.
// The caller validates the interval and commits it only after a successful write.
bool parseBleResetAutoCommand(const uint8_t *bytes, size_t size, char outArg[32])
{
    if (!bytes || size == 0 || size >= 64)
        return false;
    char text[64] = {};
    memcpy(text, bytes, size);
    char *cmd = text;
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n')
        ++cmd;
    if (*cmd == '/')
        ++cmd;
    for (char *p = cmd; *p; ++p)
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));

    static constexpr char PREFIX[] = "BLE RESET AUTO";
    const size_t prefixLen = sizeof(PREFIX) - 1;
    if (strncmp(cmd, PREFIX, prefixLen) != 0)
        return false;
    char *arg = cmd + prefixLen;
    if (*arg != '\0' && *arg != ' ' && *arg != '\t')
        return false;
    while (*arg == ' ' || *arg == '\t')
        ++arg;
    const size_t argLen = strlen(arg);
    if (argLen >= 32)
        return false;
    memcpy(outArg, arg, argLen + 1);
    size_t remaining = argLen;
    while (remaining > 0 && (outArg[remaining - 1] == ' ' ||
                             outArg[remaining - 1] == '\r' ||
                             outArg[remaining - 1] == '\n' ||
                             outArg[remaining - 1] == '\t'))
        outArg[--remaining] = '\0';
    return true;
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
        LOG_WARN("HOBO universal: %s short write requested=%u written=%u", name, length, written);
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
        static_cast<uint16_t>((static_cast<uint16_t>(mx2001Fragment1[17]) << 8) | static_cast<uint16_t>(mx2001Fragment1[18]));

    latestTemperatureRaw = raw;
    latestTemperatureF = -0.1805f * static_cast<float>(raw) + 169.64f;
    latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;
    latestStageMeters = readBEFloat(&mx2001Fragment2[3]);
    latestStageFeet = latestStageMeters * 3.280839895f;

    if (!isfinite(latestTemperatureF) || !isfinite(latestStageFeet) || latestTemperatureF < -50.0f ||
        latestTemperatureF > 180.0f || latestStageFeet < -100.0f || latestStageFeet > 1000.0f) {
        LOG_WARN("HOBO universal: implausible MX2001 live measurement");
        directReadActive = false;
        return;
    }

    loggerType = LoggerType::MX2001;
    measurementHasStage = true;
    measurementReady = true;
    directReadActive = false;
}

void notifyCallback(BLEClientCharacteristic *characteristic, uint8_t *data, uint16_t len)
{
    (void)characteristic;

    if (data == nullptr || len == 0)
        return;

    if (len >= 14 && data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x04 && data[3] == 0x05) {

        currentWritePointer = (static_cast<uint32_t>(data[8]) << 24) | (static_cast<uint32_t>(data[9]) << 16) |
                              (static_cast<uint32_t>(data[10]) << 8) | static_cast<uint32_t>(data[11]);

        loggerIntervalSeconds = static_cast<uint16_t>((static_cast<uint16_t>(data[12]) << 8) | static_cast<uint16_t>(data[13]));

        statusReady = true;

        LOG_DEBUG("HOBO universal STATUS model=%s pointer=0x%08lX interval=%u", loggerTypeName(loggerType),
                  static_cast<unsigned long>(currentWritePointer), loggerIntervalSeconds);
        return;
    }

    if (!directReadActive)
        return;

    if (len >= 12 && data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x0B && data[3] == 0x04 && data[4] == 0x04 &&
        data[5] == 0x00 && data[6] == 0x04 && data[7] == 0x04) {

        const uint32_t raw = readBE32(&data[8]);
        if (raw > MX2203_MAX_RAW) {
            LOG_WARN("HOBO universal: MX2203 raw value invalid %lu", static_cast<unsigned long>(raw));
            return;
        }

        latestTemperatureRaw = raw;
        latestTemperatureC = static_cast<float>(raw) * MX2203_CONST_A / MX2203_FULL_RAW - MX2203_CONST_C;
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

    if (len >= 12 && data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x07 && data[3] == 0x04 && data[4] == 0x04 &&
        data[5] == 0x00 && data[6] == 0x04 && data[7] == 0x04) {

        const uint32_t raw = readBE32(&data[8]);
        if (raw < MX2201_MIN_RAW || raw > MX2201_MAX_RAW) {
            LOG_WARN("HOBO universal: MX2201 raw value implausible %lu", static_cast<unsigned long>(raw));
            return;
        }

        latestTemperatureRaw = raw;
        latestTemperatureF = MX2201_RAW_TO_F_SLOPE * static_cast<float>(raw) + MX2201_RAW_TO_F_INTERCEPT;
        latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;

        loggerType = LoggerType::MX2201;
        measurementHasStage = false;
        measurementReady = true;
        directReadActive = false;
        return;
    }

    if (len >= 20 && data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x04 && data[3] == 0x04) {

        mx2001Fragment1Length = (len > sizeof(mx2001Fragment1)) ? sizeof(mx2001Fragment1) : len;
        memcpy(mx2001Fragment1, data, mx2001Fragment1Length);
        gotMX2001Fragment1 = true;
        loggerType = LoggerType::MX2001;
    } else if (len >= 7 && data[0] == 0x02) {
        mx2001Fragment2Length = (len > sizeof(mx2001Fragment2)) ? sizeof(mx2001Fragment2) : len;
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

    allAdvertisementCount++;
    if (loggerLockEnabled && memcmp(report->peer_addr.addr, lockedAddrRaw, 6) != 0) {
        Bluefruit.Scanner.resume();
        return;
    }

    matchingAddressCount++;
    lastTargetAdvertisementMs = millis();

    if (isRejectedCandidate(report->peer_addr.addr)) {
        Bluefruit.Scanner.resume();
        return;
    }

    const CandidateInfo info = inspectAdvertisement(report);
    if (!info.candidate) {
        Bluefruit.Scanner.resume();
        return;
    }
    hoboCandidateCount++;

    memcpy(candidateAddrRaw, report->peer_addr.addr, 6);
    makeHumanMac(report->peer_addr.addr, loggerMac);
    loggerBleRssi = report->rssi;
    candidateLikelyMX2001 = info.likelyMX2001;
    candidateLikelyMX2203 = info.likelyMX2203;

    LOG_INFO("HOBO universal: candidate %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d%s", loggerMac[0], loggerMac[1], loggerMac[2],
             loggerMac[3], loggerMac[4], loggerMac[5], loggerBleRssi, loggerLockEnabled ? " LOCKED-TARGET" : "");

    connecting = true;
    connectionAttemptCount++;
    connectAttemptStartedMs = millis();
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        connectAttemptStartedMs = 0;
        noteBleFailure("BLE connect request failed");
        LOG_WARN("HOBO universal: BLE connection request failed");
        requestHighDutyScanBurst();
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connectAttemptStartedMs = 0;
    connected = true;
    connectionHandle = connHandle;
    disconnectedSinceMs = 0;

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
        noteBleFailure("service discovery failed");
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
        noteBleFailure("characteristic discovery failed");
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
        noteBleFailure("notification enable failed");
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

    connectionReadyCount++;
    LOG_INFO("HOBO universal: BLE command channel ready");
    logMac("HOBO universal connected logger:", loggerMac);
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("HOBO universal: disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    disconnectedSinceMs = millis();
    requestHighDutyScanBurst();
    connectAttemptStartedMs = 0;
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

void triggerBleRecovery(const char *reason)
{
    bleRecoveryCycles++;
    noteBleFailure(reason);
    LOG_ERROR("HOBO universal: BLE recovery cycle %u: %s", bleRecoveryCycles, reason ? reason : "unspecified");

    if (connected && connectionHandle != BLE_CONN_HANDLE_INVALID) {
        Bluefruit.disconnect(connectionHandle);
    } else if (connecting) {
        (void)sd_ble_gap_connect_cancel();
        connecting = false;
        connectAttemptStartedMs = 0;
    }

    universalState = UniversalState::IDLE;
    statusTrackingAvailable = false;
    requestHighDutyScanBurst();

    // Do not turn a burst of ordinary connection/discovery failures into an
    // immediate reboot loop. runOnce() escalates only after a STATUS-proven
    // stale period, a BLE-only recovery, and a five-minute grace period.
}

void initializeClient()
{
    LOG_INFO("HOBO universal bridge: MX2001 + MX2201 + MX2203");

    loadLoggerLock();
    loadAutomaticBleResetMinutes();
    initializeHoboHardwareResetGuard();
    bleHealthTimerStartMs = millis();

    hoboService.begin();
    hoboCharacteristic.setNotifyCallback(notifyCallback);
    hoboCharacteristic.begin(&hoboService);

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(false);

    initialized = true;

    const uint32_t now = millis();
    lastScanRefreshMs = now;
    disconnectedSinceMs = now;
    refreshDisconnectedScanner(true);
    scanBurstUntilMs = now + HIGH_DUTY_SCAN_MS;
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
    : SinglePortModule("HOBOMX2001MX2201MX2203", meshtastic_PortNum_PRIVATE_APP), concurrency::OSThread("HOBOMX2001MX2201MX2203")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool HOBOMX2001MX2201MX2203TelemetryModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p == nullptr)
        return false;

    return p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP || p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOMX2001MX2201MX2203TelemetryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    if (nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "BLE RESET STATUS")) {
        char reply[180] = {};
        const uint32_t now = millis();
        snprintf(reply, sizeof(reply),
                 "BLE RESET: %um (OFF=0); STATUS:%u age:%lus soft:%lu hard:%s",
                 static_cast<unsigned int>(automaticBleResetMinutes.load()),
                 lastGoodStatusMs ? 1U : 0U,
                 static_cast<unsigned long>(lastGoodStatusMs ? (now - lastGoodStatusMs) / 1000UL : 0UL),
                 static_cast<unsigned long>(automaticBleResetCount),
                 hoboHardwareResetAlreadyUsed() ? "used" : "available");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    char recoveryArgument[32] = {};
    if (parseBleResetAutoCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, recoveryArgument)) {
        const uint16_t current = automaticBleResetMinutes.load();
        if (recoveryArgument[0] == '\0') {
            char reply[130] = {};
            snprintf(reply, sizeof(reply), "BLE RESET AUTO: %um; first stale reset at 15m; 0=OFF; range 15-720m",
                     static_cast<unsigned int>(current));
            sendTextReply(mp.from, mp.channel, reply);
            return ProcessMessage::CONTINUE;
        }

        uint16_t requested = 0;
        bool valid = !strcmp(recoveryArgument, "OFF") || !strcmp(recoveryArgument, "0");
        if (!valid) {
            char *end = nullptr;
            const long parsed = strtol(recoveryArgument, &end, 10);
            valid = end && *end == '\0' && parsed >= 15 && parsed <= 720;
            if (valid)
                requested = static_cast<uint16_t>(parsed);
        }
        if (!valid) {
            sendTextReply(mp.from, mp.channel, "BLE RESET AUTO: use OFF or 15-720 minutes (example: BLE RESET AUTO 120)");
            return ProcessMessage::CONTINUE;
        }
        if (saveAutomaticBleResetMinutes(requested)) {
            automaticBleResetMinutes.store(requested);
            char reply[130] = {};
            if (requested)
                snprintf(reply, sizeof(reply), "BLE RESET AUTO saved: %u minutes; logger lock unchanged", static_cast<unsigned int>(requested));
            else
                snprintf(reply, sizeof(reply), "BLE RESET AUTO OFF saved; logger lock unchanged");
            sendTextReply(mp.from, mp.channel, reply);
        } else {
            sendTextReply(mp.from, mp.channel, "BLE RESET AUTO: flash write failed; prior setting retained");
        }
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "HOBO RESTART")) {
        manualBleRestartPending.store(true);
        sendTextReply(mp.from, mp.channel, "HOBO RESTART queued: BLE only; mesh and saved logger lock preserved");
        setIntervalFromNow(10);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "BLE DIAG") ||
        isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "HOBO STATUS")) {
        const uint32_t now = millis();
        char out[215] = {};
        snprintf(out, sizeof(out),
                 "BLE 1/2 link:%u scan:%u fast:%u lock:%u\nadv:%lu target:%lu hobo:%lu\nconnect:%lu ready:%lu",
                 static_cast<unsigned int>(Bluefruit.Central.connected()),
                 Bluefruit.Scanner.isRunning() ? 1U : 0U, recoveryScanBurst ? 1U : 0U,
                 loggerLockEnabled ? 1U : 0U,
                 static_cast<unsigned long>(allAdvertisementCount),
                 static_cast<unsigned long>(matchingAddressCount),
                 static_cast<unsigned long>(hoboCandidateCount),
                 static_cast<unsigned long>(connectionAttemptCount),
                 static_cast<unsigned long>(connectionReadyCount));
        sendTextReply(mp.from, mp.channel, out);
        snprintf(out, sizeof(out),
                 "BLE 2/2 status:%u age:%lus target:%u age:%lus\nscan:%lu recover:%u auto:%lu int:%um hard:%s last:%s",
                 lastGoodStatusMs ? 1U : 0U,
                 static_cast<unsigned long>(lastGoodStatusMs ? (now - lastGoodStatusMs) / 1000UL : 0),
                 lastTargetAdvertisementMs ? 1U : 0U,
                 static_cast<unsigned long>(lastTargetAdvertisementMs ? (now - lastTargetAdvertisementMs) / 1000UL : 0),
                 static_cast<unsigned long>(scannerRestartCount),
                 static_cast<unsigned int>(bleRecoveryCycles),
                 static_cast<unsigned long>(automaticBleResetCount),
                 static_cast<unsigned int>(automaticBleResetMinutes.load()),
                 hoboHardwareResetAlreadyUsed() ? "used" : "available",
                 lastBleFailure);
        sendTextReply(mp.from, mp.channel, out);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "AUTO STATUS")) {
        const uint32_t now = millis();
        char out[170] = {};
        snprintf(out, sizeof(out),
                 "AUTO tracking:%u baseline:%u pointer:%08lX\nTX boot:%lu last_seen:%u age:%lus interval:%us",
                 statusTrackingAvailable ? 1U : 0U, haveStatusBaseline ? 1U : 0U,
                 static_cast<unsigned long>(lastWritePointer),
                 static_cast<unsigned long>(totalAutomaticTxCount), lastAnyAutomaticTxMs ? 1U : 0U,
                 static_cast<unsigned long>(lastAnyAutomaticTxMs ? (now - lastAnyAutomaticTxMs) / 1000UL : 0),
                 static_cast<unsigned int>(loggerIntervalSeconds));
        sendTextReply(mp.from, mp.channel, out);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "LOGGER")) {
        char reply[220] = {};
        uint8_t targetHuman[6] = {};
        if (loggerLockEnabled)
            makeHumanMac(lockedAddrRaw, targetHuman);

        if (!connected) {
            if (loggerLockEnabled) {
                snprintf(reply, sizeof(reply),
                         "HOBO NOT CONNECTED\nLock: ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X\nWaiting for target",
                         targetHuman[0], targetHuman[1], targetHuman[2], targetHuman[3], targetHuman[4], targetHuman[5]);
            } else {
                snprintf(reply, sizeof(reply), "HOBO NOT CONNECTED\nLock: OFF");
            }
        } else if (loggerIntervalSeconds > 0) {
            if (loggerLockEnabled) {
                snprintf(reply, sizeof(reply),
                         "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: %u sec\nLock: "
                         "ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X",
                         loggerTypeName(loggerType), loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4],
                         loggerMac[5], loggerBleRssi, loggerIntervalSeconds, targetHuman[0], targetHuman[1], targetHuman[2],
                         targetHuman[3], targetHuman[4], targetHuman[5]);
            } else {
                snprintf(
                    reply, sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: %u sec\nLock: OFF",
                    loggerTypeName(loggerType), loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4],
                    loggerMac[5], loggerBleRssi, loggerIntervalSeconds);
            }
        } else {
            if (loggerLockEnabled) {
                snprintf(reply, sizeof(reply),
                         "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: detecting\nLock: "
                         "ON\nTarget: %02X:%02X:%02X:%02X:%02X:%02X",
                         loggerTypeName(loggerType), loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4],
                         loggerMac[5], loggerBleRssi, targetHuman[0], targetHuman[1], targetHuman[2], targetHuman[3],
                         targetHuman[4], targetHuman[5]);
            } else {
                snprintf(
                    reply, sizeof(reply),
                    "HOBO CONNECTED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nInterval: detecting\nLock: OFF",
                    loggerTypeName(loggerType), loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4],
                    loggerMac[5], loggerBleRssi);
            }
        }

        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "LOCK")) {
        if (!connected ||
            (loggerType != LoggerType::MX2001 && loggerType != LoggerType::MX2201 && loggerType != LoggerType::MX2203)) {
            sendTextReply(mp.from, mp.channel, "LOCK failed: connect to an identified MX2001/MX2201/MX2203 first");
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
        snprintf(reply, sizeof(reply), "LOGGER LOCKED\nModel: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nPersists after reboot",
                 loggerTypeName(loggerType), loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5]);
        sendTextReply(mp.from, mp.channel, reply);
        logLockTarget("HOBO universal: saved logger lock:");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "UNLOCK")) {
        clearLoggerLock();
        sendTextReply(mp.from, mp.channel, "LOGGER UNLOCKED\nScanning any supported HOBO\nCurrent BLE link will be released");

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

bool HOBOMX2001MX2201MX2203TelemetryModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
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

    // Device/battery telemetry already uses standard TELEMETRY_APP. Send all
    // automatic HOBO measurements on that same normal path so a station cannot
    // look alive only because its separate PRIVATE_APP packet was filtered.
    // DeviceMetrics and EnvironmentMetrics are separate protobuf oneof
    // messages, but both share the standard telemetry port and gateway path.
    auto sendHoboTelemetry = [&](bool includeStage) -> bool {
        meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
        telemetry.time = getTime();
        telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
        telemetry.variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;
        telemetry.variant.environment_metrics.has_temperature = true;
        telemetry.variant.environment_metrics.temperature = latestTemperatureC;
        if (includeStage) {
            // EnvironmentMetrics defines distance in millimetres. For an
            // MX2001 this carries its calibrated water stage (ft × 304.8).
            telemetry.variant.environment_metrics.has_distance = true;
            telemetry.variant.environment_metrics.distance = latestStageFeet * 304.8f;
        }

        if (nodeDB != nullptr) {
            nodeDB->updateTelemetry(nodeDB->getNodeNum(), telemetry, RX_SRC_LOCAL);
        }

        meshtastic_MeshPacket *packet = allocDataPacket();
        if (packet == nullptr) {
            LOG_WARN("HOBO universal: telemetry packet allocation failed");
            return false;
        }

        const size_t encoded = pb_encode_to_bytes(packet->decoded.payload.bytes, sizeof(packet->decoded.payload.bytes),
                                                  &meshtastic_Telemetry_msg, &telemetry);

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

        if (includeStage) {
            LOG_INFO("HOBO universal TELEMETRY TX model=MX2001 stage=%.2f ft distance=%.0f mm temp=%.2f C / %.2f F "
                     "logger=%02X:%02X:%02X:%02X:%02X:%02X",
                     latestStageFeet, telemetry.variant.environment_metrics.distance, latestTemperatureC, latestTemperatureF,
                     loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5]);
        } else {
            LOG_INFO("HOBO universal TELEMETRY TX model=%s temp=%.2f C / %.2f F logger=%02X:%02X:%02X:%02X:%02X:%02X",
                     loggerTypeName(loggerType), latestTemperatureC, latestTemperatureF, loggerMac[0], loggerMac[1],
                     loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5]);
        }

        service->sendToMesh(packet, RX_SRC_LOCAL, true);
        return true;
    };

    auto sendAutomaticMeasurement = [&]() -> bool {
        if (loggerType == LoggerType::MX2001 && measurementHasStage)
            return sendHoboTelemetry(true);

        if (loggerType == LoggerType::MX2201 || loggerType == LoggerType::MX2203)
            return sendHoboTelemetry(false);

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

    // A good STATUS reply is the health signal, not scanner=SCANNING or
    // an intact LoRa link. Never disturb an actively healthy logger.
    const uint16_t resetMinutes = automaticBleResetMinutes.load();
    const bool unhealthyFor15Min =
        bleHealthTimerStartMs != 0 && !Throttle::isWithinTimespanMs(bleHealthTimerStartMs, FIRST_DEEP_BLE_RECOVERY_MS);
    const bool timedRecoveryDue = resetMinutes != 0 && unhealthyFor15Min &&
                                  (!automaticBleResetPreviouslyAttempted ||
                                   !Throttle::isWithinTimespanMs(
                                       lastAutomaticBleResetMs, static_cast<uint32_t>(resetMinutes) * 60000UL));
    bool autoRecoveryNow = false;
    if (timedRecoveryDue) {
        if (autoBleResetDeferredSinceMs == 0)
            autoBleResetDeferredSinceMs = now;
        const bool readBusy = readRequestInProgress || readRequestPending ||
                              universalState == UniversalState::SEND_READ ||
                              universalState == UniversalState::WAIT_READ;
        autoRecoveryNow = !readBusy ||
                          !Throttle::isWithinTimespanMs(autoBleResetDeferredSinceMs, DEFER_BLE_RESET_WHILE_READ_MS);
    } else {
        autoBleResetDeferredSinceMs = 0;
    }

    // Both DM and automatic resets are executed by this single HOBO worker.
    const bool manualRecoveryNow = manualBleRestartPending.exchange(false);
    if (manualRecoveryNow || autoRecoveryNow) {
        if (autoRecoveryNow && !manualRecoveryNow) {
            lastAutomaticBleResetMs = now;
            automaticBleResetPreviouslyAttempted = true;
            automaticBleResetCount++;
            nrf52FieldDiagEvent(2, 4, static_cast<uint16_t>(automaticBleResetCount));
            LOG_WARN("HOBO V5: BLE-only recovery %lu after %lu seconds without STATUS",
                     static_cast<unsigned long>(automaticBleResetCount),
                     static_cast<unsigned long>((now - bleHealthTimerStartMs) / 1000UL));
        } else {
            bleHealthTimerStartMs = now; // Grant 15 minutes after an operator intervention.
            automaticBleResetPreviouslyAttempted = false;
            lastAutomaticBleResetMs = 0;
        }
        autoBleResetDeferredSinceMs = 0;
        totalBleOnlyResetCount++;
        bleRecoveryCycles = 0; // Planned recovery must not trip the failure watchdog.
        noteBleFailure(manualRecoveryNow ? "manual HOBO RESTART" : "15m stale STATUS");
        disconnectedSinceMs = now;
        requestHighDutyScanBurst();
        haveRejectedAddr = false;
        if (connected && connectionHandle != BLE_CONN_HANDLE_INVALID) {
            Bluefruit.disconnect(connectionHandle); // BLE callback clears GATT state.
        } else if (connecting) {
            (void)sd_ble_gap_connect_cancel();
            connecting = false;
            connectAttemptStartedMs = 0;
            universalState = UniversalState::IDLE;
            statusReady = false;
        } else {
            universalState = UniversalState::IDLE;
            statusReady = false;
            // No SoftDevice teardown; LoRa stays up. Immediately use the
            // bounded active acquisition scan after an explicit BLE recovery.
            scanBurstRequested = false;
            refreshDisconnectedScanner(true);
            scanBurstUntilMs = now + HIGH_DUTY_SCAN_MS;
        }
        return 500;
    }

    // A central-only reset can clear an ordinary scanner/GATT dead end. If a
    // real STATUS still has not returned five minutes later, rebuild the
    // complete nRF52/SoftDevice/radio process once. The retained guard is
    // cleared only by STATUS, so a dead logger cannot reboot a working relay
    // over and over again.
    if (automaticBleResetPreviouslyAttempted && lastAutomaticBleResetMs != 0 && !hardwareResetScheduled &&
        !hoboHardwareResetAlreadyUsed() &&
        !Throttle::isWithinTimespanMs(lastAutomaticBleResetMs, HARDWARE_RESET_GRACE_MS)) {
        if (nrf52ScheduleWatchdogReset(3000UL)) {
            markHoboHardwareResetUsed();
            hardwareResetScheduled = true;
            nrf52FieldDiagEvent(2, 5, bleRecoveryCycles);
            LOG_ERROR("HOBO V5: stale STATUS survived BLE recovery; watchdog reset armed (settings retained)");
            return 500;
        }
        LOG_WARN("HOBO V5: whole-node recovery already scheduled elsewhere");
    }

    if (!connected) {
        if (connecting) {
            if (connectAttemptStartedMs != 0 && reached(now, connectAttemptStartedMs + CONNECT_ATTEMPT_TIMEOUT_MS)) {
                noteBleFailure("connect timed out");
                LOG_ERROR("HOBO universal: BLE connect attempt timed out");
                triggerBleRecovery("connect timeout");
                if (!Bluefruit.Scanner.isRunning())
                    Bluefruit.Scanner.start(0);
            }
            return 500;
        }

        serviceDisconnectedScanner(now);
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
        const char *name = activeMetaProfile == MetaProfile::MX2001 ? "MX2001 META0" : "MX2201 META0";

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
        const char *name = activeMetaProfile == MetaProfile::MX2001 ? "MX2001 META8" : "MX2201 META8";

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
            consecutiveReadTimeouts = 0;
            measurementReady = false;

            LOG_INFO("HOBO universal: READ complete model=%s logger=%02X:%02X:%02X:%02X:%02X:%02X", loggerTypeName(loggerType),
                     loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5]);

            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                char reply[190] = {};

                if (loggerType == LoggerType::MX2001 && measurementHasStage) {
                    snprintf(reply, sizeof(reply),
                             "MX2001\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nLevel: %.2f ft\nTemp: %.1f F",
                             loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5], loggerBleRssi,
                             latestStageFeet, latestTemperatureF);
                } else if (loggerType == LoggerType::MX2201) {
                    snprintf(reply, sizeof(reply), "MX2201\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nTemp: %.1f F",
                             loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5], loggerBleRssi,
                             latestTemperatureF);
                } else if (loggerType == LoggerType::MX2203) {
                    snprintf(reply, sizeof(reply),
                             "MX2203\nLogger: %02X:%02X:%02X:%02X:%02X:%02X\nBLE: %d dBm\nTemp: %.2f F / %.2f C", loggerMac[0],
                             loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5], loggerBleRssi,
                             latestTemperatureF, latestTemperatureC);
                } else {
                    snprintf(reply, sizeof(reply), "HOBO read completed; model unknown\nLogger: %02X:%02X:%02X:%02X:%02X:%02X",
                             loggerMac[0], loggerMac[1], loggerMac[2], loggerMac[3], loggerMac[4], loggerMac[5]);
                }

                sendTextReply(readRequester, readChannel, reply);
                readRequestInProgress = false;
                readRequester = 0;
                readPurpose = ReadPurpose::AUTOMATIC;
                universalState = UniversalState::READY;
                break;
            }

            if (readPurpose == ReadPurpose::PROBE) {
                LOG_INFO("HOBO universal: probe identified model=%s; automatic TX waits for logger STATUS pointer advance",
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
                    LOG_WARN("HOBO universal: suppressing automatic TX without a confirmed new logger pointer");
                    nextStatusCheckMs = now + POINTER_FINE_POLL_MS;
                    universalState = UniversalState::READY;
                    break;
                }

                const bool sent = sendAutomaticMeasurement();

                if (sent) {
                    const uint32_t previousTxMs = lastAutomaticTxMs;
                    const uint32_t cadenceMs = previousTxMs == 0 ? 0 : now - previousTxMs;
                    const uint32_t pointerToTxMs = pendingPointerDetectedMs == 0 ? 0 : now - pendingPointerDetectedMs;

                    automaticTxCount++;
                    totalAutomaticTxCount++;
                    lastAnyAutomaticTxMs = now;
                    lastAutomaticTxMs = now;
                    lastWritePointer = pendingWritePointer;
                    intervalPhaseLocked = true;

                    LOG_INFO("HOBO universal AUTO TX confirmed count=%lu pointer=0x%08lX interval=%u sec pointer_to_tx=%lu ms "
                             "cadence=%lu ms",
                             static_cast<unsigned long>(automaticTxCount), static_cast<unsigned long>(lastWritePointer),
                             loggerIntervalSeconds, static_cast<unsigned long>(pointerToTxMs),
                             static_cast<unsigned long>(cadenceMs));

                    if (previousTxMs != 0 && loggerIntervalSeconds > 0) {
                        const uint32_t expectedMs = static_cast<uint32_t>(loggerIntervalSeconds) * 1000UL;
                        const uint32_t driftMs = cadenceMs > expectedMs ? cadenceMs - expectedMs : expectedMs - cadenceMs;

                        if (driftMs > 2000) {
                            LOG_WARN("HOBO universal AUTO TX cadence differs from logger interval by %lu ms (pointer-gated; no "
                                     "duplicate generated)",
                                     static_cast<unsigned long>(driftMs));
                        }
                    }

                    nextStatusCheckMs = now + nextRecordPrecheckDelayMs();
                } else {
                    LOG_WARN("HOBO universal: automatic mesh enqueue failed; retaining pointer for retry");
                    nextStatusCheckMs = now + 1000;
                }

                pendingPointerDetectedMs = 0;
                universalState = UniversalState::READY;
                break;
            }
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;

            if (readPurpose == ReadPurpose::PROBE && loggerType == LoggerType::UNKNOWN && prepareFallbackProbe()) {
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

            consecutiveReadTimeouts++;
            LOG_WARN("HOBO universal: automatic NEWREAD64 timeout %u/%u", consecutiveReadTimeouts, READ_TIMEOUT_LIMIT);
            if (consecutiveReadTimeouts >= READ_TIMEOUT_LIMIT) {
                triggerBleRecovery("NEWREAD64 timeouts");
            } else {
                readPurpose = ReadPurpose::AUTOMATIC;
                nextStatusCheckMs = now + 1000;
                universalState = UniversalState::READY;
            }
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
                LOG_ERROR("HOBO universal: STATUS writes failed repeatedly; rebuilding BLE link");
                triggerBleRecovery("STATUS write failures");
            } else {
                nextStatusCheckMs = now + 1000;
                universalState = UniversalState::READY;
            }
        }
        break;

    case UniversalState::WAIT_STATUS:
        if (statusReady) {
            statusReady = false;
            lastGoodStatusMs = now;
            bleHealthTimerStartMs = now;
            automaticBleResetPreviouslyAttempted = false;
            lastAutomaticBleResetMs = 0;
            autoBleResetDeferredSinceMs = 0;
            hardwareResetScheduled = false;
            clearHoboHardwareResetGuard();
            consecutiveStatusTimeouts = 0;
            consecutiveReadTimeouts = 0;
            bleRecoveryCycles = 0;
            statusTrackingAvailable = true;

            if (!haveStatusBaseline) {
                haveStatusBaseline = true;
                lastWritePointer = currentWritePointer;
                intervalPhaseLocked = false;

                LOG_INFO("HOBO universal: status baseline model=%s pointer=0x%08lX interval=%u sec; syncing to next true record "
                         "boundary",
                         loggerTypeName(loggerType), static_cast<unsigned long>(lastWritePointer), loggerIntervalSeconds);

                nextStatusCheckMs = now + POINTER_INITIAL_SYNC_POLL_MS;
                universalState = UniversalState::READY;
                break;
            }

            if (currentWritePointer != lastWritePointer) {
                // Reject implausibly rapid repeated pointer advances, including
                // noisy STATUS responses that previously flooded the mesh.
                // A true new record remains pending and is read at a later poll.
                // Half the logger interval tolerates the 30 s observation jitter.
                if (lastAutomaticTxMs != 0 && loggerIntervalSeconds > 0 &&
                    (now - lastAutomaticTxMs) < (static_cast<uint32_t>(loggerIntervalSeconds) * 500UL)) {
                    LOG_WARN("HOBO universal: early pointer advance suppressed; waiting for next 30s STATUS");
                    nextStatusCheckMs = now + HOBO_STATUS_POLL_MS;
                    universalState = UniversalState::READY;
                    break;
                }
                pendingWritePointer = currentWritePointer;
                pendingPointerDetectedMs = now;
                readPurpose = ReadPurpose::AUTOMATIC;

                LOG_INFO("HOBO universal: new logger record model=%s old=0x%08lX new=0x%08lX interval=%u sec phase=%s",
                         loggerTypeName(loggerType), static_cast<unsigned long>(lastWritePointer),
                         static_cast<unsigned long>(currentWritePointer), loggerIntervalSeconds,
                         intervalPhaseLocked ? "LOCKED" : "SYNCING");

                universalState = UniversalState::SEND_READ;
                stateDueMs = now;
            } else {
                nextStatusCheckMs = now + (intervalPhaseLocked ? POINTER_FINE_POLL_MS : POINTER_INITIAL_SYNC_POLL_MS);
                universalState = UniversalState::READY;
            }
            break;
        }

        if (reached(now, stateDueMs)) {
            consecutiveStatusTimeouts++;
            LOG_WARN("HOBO universal: STATUS timeout %u/%u", consecutiveStatusTimeouts, STATUS_TIMEOUT_LIMIT);

            if (consecutiveStatusTimeouts >= STATUS_TIMEOUT_LIMIT) {
                LOG_ERROR("HOBO universal: STATUS responses stale; rebuilding BLE link");
                triggerBleRecovery("STATUS response timeouts");
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
