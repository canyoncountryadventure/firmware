#include "HoboBleSensorModule.h"

#if defined(ARCH_ESP32) && defined(HELTEC_V4) && !MESHTASTIC_EXCLUDE_BLUETOOTH

#include "HoboHttpGatewayModule.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "SPILock.h"
#include "main.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include "pb_encode.h"

#include <NimBLEDevice.h>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace
{

static const NimBLEUUID HOBO_SERVICE_UUID("65e16e4f-ed4e-4641-ac49-83ccbce6cbcf");
static const NimBLEUUID HOBO_CHAR_UUID("65e16f4f-ed4e-4641-ac49-83ccbce6cbcf");

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
enum class HoboState : uint8_t {
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

struct SeenHobo {
    char mac[18];
    uint8_t addressType;
    int8_t rssi;
    uint32_t lastSeenMs;
    bool likelyMX2001;
    bool likelyMX2203;
};

struct NotificationFrame {
    uint8_t data[64];
    uint8_t length;
};

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
static constexpr uint32_t UNLOCKED_SCAN_WINDOW_MS = 5000;
static constexpr uint32_t SEEN_FRESH_MS = 20000;
static constexpr char LOCK_FILE_PATH[] = "/prefs/heltec_hobo_lock.bin";

static constexpr uint32_t MX2201_MIN_RAW = 400;
static constexpr uint32_t MX2201_MAX_RAW = 2400;
static constexpr float MX2201_RAW_TO_F_SLOPE = 0.0771942720f;
static constexpr float MX2201_RAW_TO_F_INTERCEPT = -52.2825573f;
static constexpr float MX2203_CONST_A = 175.72f;
static constexpr float MX2203_FULL_RAW = 16384.0f;
static constexpr float MX2203_CONST_C = 46.85f;
static constexpr uint32_t MX2203_MAX_RAW = 16383;

bool initialized = false;
bool connecting = false;
bool connected = false;
bool serviceReady = false;
NimBLEClient *hoboClient = nullptr;
NimBLERemoteCharacteristic *hoboCharacteristic = nullptr;

LoggerType loggerType = LoggerType::UNKNOWN;
MetaProfile activeMetaProfile = MetaProfile::NONE;
ReadPurpose readPurpose = ReadPurpose::PROBE;
HoboState state = HoboState::IDLE;
uint32_t stateDueMs = 0;
uint8_t probeAttempt = 0;
bool candidateLikelyMX2001 = false;
bool candidateLikelyMX2203 = false;

char loggerMac[18] = {};
uint8_t loggerAddressType = 0;
int8_t loggerBleRssi = 0;

bool loggerLockEnabled = false;
char lockedMac[18] = {};
uint8_t lockedAddressType = 0;

char rejectedMac[18] = {};
uint32_t rejectedUntilMs = 0;

SeenHobo seenHobos[4] = {};
uint8_t seenCount = 0;
uint32_t scanStartedMs = 0;

std::mutex bleEventMutex;
std::atomic<bool> clientConnectedEvent{false};
std::atomic<bool> clientDisconnectedEvent{false};
std::atomic<bool> clientConnectFailedEvent{false};
std::atomic<int> clientDisconnectReason{0};

bool candidatePending = false;
char pendingCandidateMac[18] = {};
uint8_t pendingCandidateType = 0;
int8_t pendingCandidateRssi = 0;
bool pendingCandidateLikelyMX2001 = false;
bool pendingCandidateLikelyMX2203 = false;

std::mutex notificationMutex;
NotificationFrame notificationQueue[8] = {};
uint8_t notificationCount = 0;

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

bool reached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

uint32_t readBE32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
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

const char *loggerTypeName(LoggerType type)
{
    switch (type) {
    case LoggerType::MX2001: return "MX2001";
    case LoggerType::MX2201: return "MX2201";
    case LoggerType::MX2203: return "MX2203";
    case LoggerType::UNSUPPORTED: return "UNSUPPORTED";
    default: return "UNKNOWN";
    }
}

void uppercaseMac(char *mac)
{
    if (!mac)
        return;
    for (size_t i = 0; mac[i]; ++i)
        mac[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(mac[i])));
}

bool validMac(const char *mac)
{
    if (!mac || strlen(mac) != 17)
        return false;
    unsigned int b[6] = {};
    char tail = 0;
    return sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x%c",
                  &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &tail) == 6;
}

void macToBytes(const char *mac, uint8_t out[6])
{
    unsigned int b[6] = {};
    if (mac && sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x",
                      &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (uint8_t i = 0; i < 6; ++i)
            out[i] = static_cast<uint8_t>(b[i]);
    }
}

uint8_t checksum(const uint8_t *data, size_t length)
{
    uint8_t value = 0;
    for (size_t i = 0; i < length; ++i)
        value ^= data[i];
    return value;
}

bool saveLoggerLock()
{
    uint8_t record[24] = {'H', 'B', 'L', '2'};
    record[4] = lockedAddressType;
    memcpy(&record[5], lockedMac, 17);
    record[22] = 0;
    record[23] = checksum(record, 23);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(LOCK_FILE_PATH, FILE_O_WRITE);
    if (!file)
        return false;
    const size_t written = file.write(record, sizeof(record));
    file.flush();
    file.close();
    return written == sizeof(record);
}

void clearLoggerLock()
{
    loggerLockEnabled = false;
    lockedMac[0] = '\0';
    lockedAddressType = 0;
    concurrency::LockGuard g(spiLock);
    FSCom.remove(LOCK_FILE_PATH);
}

void loadLoggerLock()
{
    uint8_t record[24] = {};
    size_t count = 0;
    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(LOCK_FILE_PATH, FILE_O_READ);
        if (!file)
            return;
        count = file.read(record, sizeof(record));
        file.close();
    }

    if (count != sizeof(record) || memcmp(record, "HBL2", 4) != 0 || record[23] != checksum(record, 23)) {
        LOG_WARN("CCA HOBO: ignoring invalid saved logger lock");
        return;
    }

    memcpy(lockedMac, &record[5], 17);
    lockedMac[17] = '\0';
    uppercaseMac(lockedMac);
    if (!validMac(lockedMac)) {
        lockedMac[0] = '\0';
        return;
    }
    lockedAddressType = record[4];
    loggerLockEnabled = true;
    LOG_INFO("CCA HOBO: restored lock %s type=%u", lockedMac, lockedAddressType);
}

uint32_t nextRecordPrecheckDelayMs()
{
    if (loggerIntervalSeconds == 0)
        return POINTER_INITIAL_SYNC_POLL_MS;
    const uint32_t intervalMs = static_cast<uint32_t>(loggerIntervalSeconds) * 1000UL;
    if (intervalMs > 3000)
        return intervalMs - 2000;
    if (intervalMs > 1000)
        return intervalMs / 2;
    return 500;
}

bool containsIgnoreCase(const std::string &haystack, const char *needle)
{
    if (!needle || !*needle)
        return false;
    std::string upper = haystack;
    for (char &c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    std::string target(needle);
    for (char &c : target)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return upper.find(target) != std::string::npos;
}

struct CandidateInfo {
    bool candidate = false;
    bool likelyMX2001 = false;
    bool likelyMX2203 = false;
};

CandidateInfo inspectAdvertisement(NimBLEAdvertisedDevice *device)
{
    CandidateInfo result;
    if (!device)
        return result;

    if (device->haveServiceUUID() && device->isAdvertisingService(HOBO_SERVICE_UUID))
        result.candidate = true;

    if (device->haveName()) {
        const std::string name = device->getName();
        if (containsIgnoreCase(name, "HOBO") || containsIgnoreCase(name, "MX2001") ||
            containsIgnoreCase(name, "MX2201") || containsIgnoreCase(name, "MX2203")) {
            result.candidate = true;
        }
    }

    if (device->haveManufacturerData()) {
        const std::string data = device->getManufacturerData();
        if (data.size() >= 2 && static_cast<uint8_t>(data[0]) == 0xC5 && static_cast<uint8_t>(data[1]) == 0x00) {
            result.candidate = true;
            if (data.size() == 22)
                result.likelyMX2001 = true;
            if (data.size() >= 10 && static_cast<uint8_t>(data[6]) == 0x01 &&
                static_cast<uint8_t>(data[7]) == 0x03 && static_cast<uint8_t>(data[8]) == 0x22 &&
                static_cast<uint8_t>(data[9]) == 0x02) {
                result.likelyMX2203 = true;
            }
        }
    }

    return result;
}

void updateSeenLocked(const char *mac, uint8_t addressType, int8_t rssi, const CandidateInfo &info)
{
    const uint32_t now = millis();
    for (uint8_t i = 0; i < seenCount; ++i) {
        if (strcmp(seenHobos[i].mac, mac) == 0) {
            seenHobos[i].addressType = addressType;
            seenHobos[i].rssi = rssi;
            seenHobos[i].lastSeenMs = now;
            seenHobos[i].likelyMX2001 = info.likelyMX2001;
            seenHobos[i].likelyMX2203 = info.likelyMX2203;
            return;
        }
    }

    uint8_t index = seenCount < 4 ? seenCount++ : 0;
    if (seenCount >= 4) {
        uint32_t oldest = seenHobos[0].lastSeenMs;
        for (uint8_t i = 1; i < 4; ++i) {
            if (seenHobos[i].lastSeenMs < oldest) {
                oldest = seenHobos[i].lastSeenMs;
                index = i;
            }
        }
    }

    snprintf(seenHobos[index].mac, sizeof(seenHobos[index].mac), "%s", mac);
    seenHobos[index].addressType = addressType;
    seenHobos[index].rssi = rssi;
    seenHobos[index].lastSeenMs = now;
    seenHobos[index].likelyMX2001 = info.likelyMX2001;
    seenHobos[index].likelyMX2203 = info.likelyMX2203;
}

bool isRejectedLocked(const char *mac)
{
    if (rejectedMac[0] == '\0')
        return false;
    if (reached(millis(), rejectedUntilMs)) {
        rejectedMac[0] = '\0';
        return false;
    }
    return strcmp(rejectedMac, mac) == 0;
}

void rejectCurrentCandidate(uint32_t retryMs = REJECT_RETRY_MS)
{
    std::lock_guard<std::mutex> guard(bleEventMutex);
    snprintf(rejectedMac, sizeof(rejectedMac), "%s", loggerMac);
    rejectedUntilMs = millis() + retryMs;
}

#ifdef NIMBLE_TWO
class HoboScanCallbacks : public NimBLEScanCallbacks
{
  public:
    void onResult(const NimBLEAdvertisedDevice *device) override
#else
class HoboScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
  public:
    void onResult(NimBLEAdvertisedDevice *device) override
#endif
    {
        const CandidateInfo info = inspectAdvertisement(device);
        if (!info.candidate)
            return;

        char mac[18] = {};
        snprintf(mac, sizeof(mac), "%s", device->getAddress().toString().c_str());
        uppercaseMac(mac);
        const uint8_t addressType = device->getAddressType();
        const int8_t rssi = device->getRSSI();

        std::lock_guard<std::mutex> guard(bleEventMutex);
        if (isRejectedLocked(mac))
            return;
        updateSeenLocked(mac, addressType, rssi, info);

        if (loggerLockEnabled && strcmp(mac, lockedMac) == 0 && !candidatePending && !connecting && !connected) {
            snprintf(pendingCandidateMac, sizeof(pendingCandidateMac), "%s", mac);
            pendingCandidateType = addressType;
            pendingCandidateRssi = rssi;
            pendingCandidateLikelyMX2001 = info.likelyMX2001;
            pendingCandidateLikelyMX2203 = info.likelyMX2203;
            candidatePending = true;
            NimBLEDevice::getScan()->stop();
        }
    }
};

class HoboClientCallbacks : public NimBLEClientCallbacks
{
  public:
    void onConnect(NimBLEClient *client) override
    {
        (void)client;
        clientConnectedEvent = true;
    }
#ifdef NIMBLE_TWO
    void onDisconnect(NimBLEClient *client, int reason) override
    {
        (void)client;
        clientDisconnectReason = reason;
        clientDisconnectedEvent = true;
    }
    void onConnectFail(NimBLEClient *client, int reason) override
    {
        (void)client;
        clientDisconnectReason = reason;
        clientConnectFailedEvent = true;
    }
#else
    void onDisconnect(NimBLEClient *client) override
    {
        (void)client;
        clientDisconnectedEvent = true;
    }
#endif
};

HoboScanCallbacks scanCallbacks;
HoboClientCallbacks clientCallbacks;

void notificationCallback(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
{
    (void)characteristic;
    (void)isNotify;
    if (!data || length == 0)
        return;

    std::lock_guard<std::mutex> guard(notificationMutex);
    if (notificationCount >= 8) {
        LOG_WARN("CCA HOBO: notification queue full");
        return;
    }

    NotificationFrame &frame = notificationQueue[notificationCount++];
    frame.length = static_cast<uint8_t>(length > sizeof(frame.data) ? sizeof(frame.data) : length);
    memcpy(frame.data, data, frame.length);
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

void finishMX2001Measurement()
{
    if (!gotMX2001Fragment1 || !gotMX2001Fragment2)
        return;
    if (mx2001Fragment1Length < 19 || mx2001Fragment2Length < 7) {
        directReadActive = false;
        return;
    }

    const uint16_t raw = static_cast<uint16_t>((static_cast<uint16_t>(mx2001Fragment1[17]) << 8) | mx2001Fragment1[18]);
    latestTemperatureRaw = raw;
    latestTemperatureF = -0.1805f * static_cast<float>(raw) + 169.64f;
    latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;
    latestStageMeters = readBEFloat(&mx2001Fragment2[3]);
    latestStageFeet = latestStageMeters * 3.280839895f;

    if (!isfinite(latestTemperatureF) || !isfinite(latestStageFeet) || latestTemperatureF < -50.0f ||
        latestTemperatureF > 180.0f || latestStageFeet < -100.0f || latestStageFeet > 1000.0f) {
        directReadActive = false;
        return;
    }

    loggerType = LoggerType::MX2001;
    measurementHasStage = true;
    measurementReady = true;
    directReadActive = false;
}

void processNotification(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return;

    if (len >= 14 && data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x04 && data[3] == 0x05) {
        currentWritePointer = (static_cast<uint32_t>(data[8]) << 24) | (static_cast<uint32_t>(data[9]) << 16) |
                              (static_cast<uint32_t>(data[10]) << 8) | static_cast<uint32_t>(data[11]);
        loggerIntervalSeconds = static_cast<uint16_t>((static_cast<uint16_t>(data[12]) << 8) | data[13]);
        statusReady = true;
        return;
    }

    if (!directReadActive)
        return;

    if (len >= 12 && data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x0B && data[3] == 0x04 &&
        data[4] == 0x04 && data[5] == 0x00 && data[6] == 0x04 && data[7] == 0x04) {
        const uint32_t raw = readBE32(&data[8]);
        if (raw <= MX2203_MAX_RAW) {
            latestTemperatureRaw = raw;
            latestTemperatureC = static_cast<float>(raw) * MX2203_CONST_A / MX2203_FULL_RAW - MX2203_CONST_C;
            latestTemperatureF = latestTemperatureC * (9.0f / 5.0f) + 32.0f;
            loggerType = LoggerType::MX2203;
            measurementReady = isfinite(latestTemperatureC);
            directReadActive = !measurementReady;
        }
        return;
    }

    if (len >= 12 && data[0] == 0x01 && data[1] == 0x01 && data[2] == 0x07 && data[3] == 0x04 &&
        data[4] == 0x04 && data[5] == 0x00 && data[6] == 0x04 && data[7] == 0x04) {
        const uint32_t raw = readBE32(&data[8]);
        if (raw >= MX2201_MIN_RAW && raw <= MX2201_MAX_RAW) {
            latestTemperatureRaw = raw;
            latestTemperatureF = MX2201_RAW_TO_F_SLOPE * static_cast<float>(raw) + MX2201_RAW_TO_F_INTERCEPT;
            latestTemperatureC = (latestTemperatureF - 32.0f) * 5.0f / 9.0f;
            loggerType = LoggerType::MX2201;
            measurementReady = true;
            directReadActive = false;
        }
        return;
    }

    if (len >= 20 && data[0] == 0x01 && data[1] == 0x02 && data[2] == 0x04 && data[3] == 0x04) {
        mx2001Fragment1Length = static_cast<uint16_t>(len > sizeof(mx2001Fragment1) ? sizeof(mx2001Fragment1) : len);
        memcpy(mx2001Fragment1, data, mx2001Fragment1Length);
        gotMX2001Fragment1 = true;
        loggerType = LoggerType::MX2001;
    } else if (len >= 7 && data[0] == 0x02) {
        mx2001Fragment2Length = static_cast<uint16_t>(len > sizeof(mx2001Fragment2) ? sizeof(mx2001Fragment2) : len);
        memcpy(mx2001Fragment2, data, mx2001Fragment2Length);
        gotMX2001Fragment2 = true;
    }

    if (gotMX2001Fragment1 && gotMX2001Fragment2)
        finishMX2001Measurement();
}

void drainNotifications()
{
    NotificationFrame local[8] = {};
    uint8_t count = 0;
    {
        std::lock_guard<std::mutex> guard(notificationMutex);
        count = notificationCount;
        for (uint8_t i = 0; i < count; ++i)
            local[i] = notificationQueue[i];
        notificationCount = 0;
    }
    for (uint8_t i = 0; i < count; ++i)
        processNotification(local[i].data, local[i].length);
}

void resetConnectionState()
{
    connected = false;
    connecting = false;
    serviceReady = false;
    hoboCharacteristic = nullptr;
    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    readPurpose = ReadPurpose::PROBE;
    state = HoboState::IDLE;
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
        readFailureReplyPending = readRequester != 0;
    }
}

void startScan()
{
    NimBLEScan *scan = NimBLEDevice::getScan();
#ifdef NIMBLE_TWO
    scan->setScanCallbacks(&scanCallbacks, false);
#else
    scan->setAdvertisedDeviceCallbacks(&scanCallbacks, false);
#endif
    scan->setActiveScan(false);
    scan->setInterval(160);
    scan->setWindow(80);
#ifdef NIMBLE_TWO
    scan->setMaxResults(0);
    scan->start(0, false, true);
#else
    scan->start(0, nullptr, false);
#endif
    scanStartedMs = millis();
    LOG_INFO("CCA HOBO: scanning%s", loggerLockEnabled ? " for locked logger" : " for supported loggers");
}

bool chooseUnlockedCandidate()
{
    std::lock_guard<std::mutex> guard(bleEventMutex);
    if (candidatePending || seenCount == 0)
        return candidatePending;

    const uint32_t now = millis();
    int best = -1;
    int bestRssi = -128;
    for (uint8_t i = 0; i < seenCount; ++i) {
        if (now - seenHobos[i].lastSeenMs > SEEN_FRESH_MS || isRejectedLocked(seenHobos[i].mac))
            continue;
        if (seenHobos[i].rssi > bestRssi) {
            best = i;
            bestRssi = seenHobos[i].rssi;
        }
    }
    if (best < 0)
        return false;

    snprintf(pendingCandidateMac, sizeof(pendingCandidateMac), "%s", seenHobos[best].mac);
    pendingCandidateType = seenHobos[best].addressType;
    pendingCandidateRssi = seenHobos[best].rssi;
    pendingCandidateLikelyMX2001 = seenHobos[best].likelyMX2001;
    pendingCandidateLikelyMX2203 = seenHobos[best].likelyMX2203;
    candidatePending = true;
    return true;
}

bool takePendingCandidate()
{
    std::lock_guard<std::mutex> guard(bleEventMutex);
    if (!candidatePending)
        return false;
    snprintf(loggerMac, sizeof(loggerMac), "%s", pendingCandidateMac);
    loggerAddressType = pendingCandidateType;
    loggerBleRssi = pendingCandidateRssi;
    candidateLikelyMX2001 = pendingCandidateLikelyMX2001;
    candidateLikelyMX2203 = pendingCandidateLikelyMX2203;
    candidatePending = false;
    return true;
}

bool beginConnect()
{
    if (!takePendingCandidate())
        return false;

    NimBLEDevice::getScan()->stop();
    if (!hoboClient) {
        hoboClient = NimBLEDevice::createClient();
        if (!hoboClient)
            return false;
        hoboClient->setClientCallbacks(&clientCallbacks, false);
    }

    const NimBLEAddress address(loggerMac, loggerAddressType);
    connecting = true;
#ifdef NIMBLE_TWO
    if (!hoboClient->connect(address, true, true, true)) {
#else
    if (!hoboClient->connect(address, true)) {
#endif
        connecting = false;
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        return false;
    }

    LOG_INFO("CCA HOBO: connecting %s RSSI=%d", loggerMac, loggerBleRssi);
    return true;
}

bool finishServiceDiscovery()
{
    if (!hoboClient || !hoboClient->isConnected())
        return false;

    NimBLERemoteService *service = hoboClient->getService(HOBO_SERVICE_UUID);
    if (!service)
        return false;
    hoboCharacteristic = service->getCharacteristic(HOBO_CHAR_UUID);
    if (!hoboCharacteristic)
        return false;
    if (!hoboCharacteristic->subscribe(true, notificationCallback, true))
        return false;

    connecting = false;
    connected = true;
    serviceReady = true;
    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    readPurpose = ReadPurpose::PROBE;
    probeAttempt = 0;
    resetMeasurementCapture();
    statusReady = false;
    haveStatusBaseline = false;
    statusTrackingAvailable = true;
    consecutiveStatusTimeouts = 0;
    state = HoboState::SEND_INIT;
    stateDueMs = millis() + SERVICE_SETTLE_MS;
    LOG_INFO("CCA HOBO: command channel ready %s", loggerMac);
    return true;
}

bool sendCommand(const uint8_t *command, size_t length, const char *name)
{
    if (!connected || !serviceReady || !hoboCharacteristic)
        return false;
    const bool response = hoboCharacteristic->canWrite();
    if (!response && !hoboCharacteristic->canWriteNoResponse()) {
        LOG_WARN("CCA HOBO: characteristic is not writable");
        return false;
    }
    if (!hoboCharacteristic->writeValue(command, length, response)) {
        LOG_WARN("CCA HOBO: %s write failed", name);
        return false;
    }
    return true;
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

void initializeClient()
{
    loadLoggerLock();
    initialized = true;
    startScan();
    LOG_INFO("CCA HOBO sensor enabled: MX2001 + MX2201 + MX2203; LOGGER/READ/LOCK/UNLOCK");
}

bool parseCommand(const uint8_t *bytes, size_t size, char command[16], char argument[32])
{
    command[0] = '\0';
    argument[0] = '\0';
    if (!bytes || size == 0)
        return false;

    char text[64] = {};
    size_t n = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
    memcpy(text, bytes, n);
    char *p = text;
    while (*p && std::isspace(static_cast<unsigned char>(*p)))
        ++p;
    if (*p == '/')
        ++p;

    size_t ci = 0;
    while (*p && !std::isspace(static_cast<unsigned char>(*p)) && ci < 15)
        command[ci++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p++)));
    command[ci] = '\0';
    while (*p && std::isspace(static_cast<unsigned char>(*p)))
        ++p;
    size_t ai = 0;
    while (*p && ai < 31)
        argument[ai++] = *p++;
    while (ai > 0 && std::isspace(static_cast<unsigned char>(argument[ai - 1])))
        --ai;
    argument[ai] = '\0';
    uppercaseMac(argument);
    return ci > 0;
}

bool lookupSeen(const char *mac, SeenHobo &result)
{
    std::lock_guard<std::mutex> guard(bleEventMutex);
    for (uint8_t i = 0; i < seenCount; ++i) {
        if (strcmp(seenHobos[i].mac, mac) == 0 && millis() - seenHobos[i].lastSeenMs <= SEEN_FRESH_MS) {
            result = seenHobos[i];
            return true;
        }
    }
    return false;
}

void appendSeenList(char *reply, size_t replySize)
{
    std::lock_guard<std::mutex> guard(bleEventMutex);
    size_t used = strlen(reply);
    if (used + 10 >= replySize)
        return;
    snprintf(reply + used, replySize - used, "\nSeen:");
    uint8_t shown = 0;
    for (uint8_t i = 0; i < seenCount && shown < 3; ++i) {
        if (millis() - seenHobos[i].lastSeenMs > SEEN_FRESH_MS)
            continue;
        used = strlen(reply);
        if (used + 28 >= replySize)
            break;
        snprintf(reply + used, replySize - used, "\n%s %d", seenHobos[i].mac, seenHobos[i].rssi);
        ++shown;
    }
    if (shown == 0) {
        used = strlen(reply);
        snprintf(reply + used, replySize - used, " none");
    }
}

} // namespace

HoboBleSensorModule::HoboBleSensorModule()
    : SinglePortModule("cca_hobo_ble", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("cca_hobo_ble")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool HoboBleSensorModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p && p->which_payload_variant == meshtastic_MeshPacket_decoded_tag &&
           p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

bool HoboBleSensorModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
{
    if (!text || destination == 0)
        return false;
    meshtastic_MeshPacket *packet = allocDataPacket();
    if (!packet)
        return false;
    size_t len = strlen(text);
    if (len > sizeof(packet->decoded.payload.bytes))
        len = sizeof(packet->decoded.payload.bytes);
    memcpy(packet->decoded.payload.bytes, text, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

ProcessMessage HoboBleSensorModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!nodeDB || mp.to != nodeDB->getNodeNum() || mp.from == nodeDB->getNodeNum())
        return ProcessMessage::CONTINUE;

    char command[16] = {};
    char argument[32] = {};
    if (!parseCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, argument))
        return ProcessMessage::CONTINUE;

    if (strcmp(command, "LOGGER") == 0) {
        char reply[230] = {};
        if (connected) {
            snprintf(reply, sizeof(reply), "HOBO CONNECTED\nModel: %s\nMAC: %s\nBLE: %d dBm\nInterval: %s\nLock: %s",
                     loggerTypeName(loggerType), loggerMac, loggerBleRssi,
                     loggerIntervalSeconds ? String(loggerIntervalSeconds).c_str() : "detecting",
                     loggerLockEnabled ? "ON" : "OFF");
        } else if (loggerLockEnabled) {
            snprintf(reply, sizeof(reply), "HOBO NOT CONNECTED\nLock: ON\nTarget: %s\nScanning for target", lockedMac);
        } else {
            snprintf(reply, sizeof(reply), "HOBO NOT CONNECTED\nLock: OFF\nScanning");
        }
        appendSeenList(reply, sizeof(reply));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "LOCK") == 0) {
        if (argument[0]) {
            if (!validMac(argument)) {
                sendTextReply(mp.from, mp.channel, "LOCK failed: use MAC AA:BB:CC:DD:EE:FF");
                return ProcessMessage::CONTINUE;
            }
            SeenHobo seen = {};
            if (!lookupSeen(argument, seen)) {
                sendTextReply(mp.from, mp.channel, "LOCK failed: logger not in recent scan; send LOGGER first");
                return ProcessMessage::CONTINUE;
            }
            snprintf(lockedMac, sizeof(lockedMac), "%s", seen.mac);
            lockedAddressType = seen.addressType;
        } else {
            if (!connected || loggerType == LoggerType::UNKNOWN || loggerType == LoggerType::UNSUPPORTED) {
                sendTextReply(mp.from, mp.channel, "LOCK failed: connect to an identified HOBO first");
                return ProcessMessage::CONTINUE;
            }
            snprintf(lockedMac, sizeof(lockedMac), "%s", loggerMac);
            lockedAddressType = loggerAddressType;
        }

        loggerLockEnabled = true;
        if (!saveLoggerLock()) {
            loggerLockEnabled = false;
            sendTextReply(mp.from, mp.channel, "LOCK failed: could not save to flash");
            return ProcessMessage::CONTINUE;
        }

        char reply[100] = {};
        snprintf(reply, sizeof(reply), "LOGGER LOCKED\nTarget: %s\nPersists after reboot", lockedMac);
        sendTextReply(mp.from, mp.channel, reply);
        if (connected && strcmp(loggerMac, lockedMac) != 0 && hoboClient)
            hoboClient->disconnect();
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "UNLOCK") == 0) {
        clearLoggerLock();
        sendTextReply(mp.from, mp.channel, "LOGGER UNLOCKED\nScanning supported HOBOs");
        if (connected && hoboClient)
            hoboClient->disconnect();
        return ProcessMessage::CONTINUE;
    }

    if (strcmp(command, "READ") != 0)
        return ProcessMessage::CONTINUE;

    if (!connected || !serviceReady) {
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

int32_t HoboBleSensorModule::runOnce()
{
    const uint32_t now = millis();
    drainNotifications();

    auto sendTemperatureTelemetry = [&](float temperatureC) -> bool {
        meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
        telemetry.time = getTime();
        telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
        telemetry.variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;
        telemetry.variant.environment_metrics.has_temperature = true;
        telemetry.variant.environment_metrics.temperature = temperatureC;
        if (nodeDB)
            nodeDB->updateTelemetry(nodeDB->getNodeNum(), telemetry, RX_SRC_LOCAL);

        meshtastic_MeshPacket *packet = allocDataPacket();
        if (!packet)
            return false;
        const size_t encoded = pb_encode_to_bytes(packet->decoded.payload.bytes, sizeof(packet->decoded.payload.bytes),
                                                  &meshtastic_Telemetry_msg, &telemetry);
        if (encoded == 0)
            return false;
        packet->decoded.payload.size = encoded;
        packet->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
        packet->to = NODENUM_BROADCAST;
        packet->channel = 0;
        packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
        service->sendToMesh(packet, RX_SRC_LOCAL, true);

        ++measurementSequence;
        bool cloudQueued = hoboHttpGatewayModule &&
            hoboHttpGatewayModule->queueLocalEnvironment(temperatureC, loggerTypeName(loggerType), loggerMac,
                                                         loggerBleRssi, measurementSequence);
        LOG_INFO("CCA HOBO AUTO temp %.2f C model=%s mesh=queued cloud=%s", temperatureC,
                 loggerTypeName(loggerType), cloudQueued ? "queued" : "FAILED");
        return cloudQueued;
    };

    auto sendMX2001 = [&]() -> bool {
        meshtastic_MeshPacket *packet = allocDataPacket();
        if (!packet)
            return false;
        int32_t stageScaled = static_cast<int32_t>(lroundf(latestStageFeet * 10.0f));
        int32_t tempScaled = static_cast<int32_t>(lroundf(latestTemperatureF * 10.0f));
        stageScaled = stageScaled < -32768 ? -32768 : (stageScaled > 32767 ? 32767 : stageScaled);
        tempScaled = tempScaled < -32768 ? -32768 : (tempScaled > 32767 ? 32767 : tempScaled);
        const int16_t stageTenths = static_cast<int16_t>(stageScaled);
        const int16_t tempTenths = static_cast<int16_t>(tempScaled);
        const uint16_t raw16 = static_cast<uint16_t>(latestTemperatureRaw & 0xFFFFU);
        ++measurementSequence;
        uint8_t macBytes[6] = {};
        macToBytes(loggerMac, macBytes);
        uint8_t *payload = packet->decoded.payload.bytes;
        payload[0] = 'M'; payload[1] = 'X'; payload[2] = 1; payload[3] = 0x03;
        payload[4] = static_cast<uint8_t>(measurementSequence & 0xFF);
        payload[5] = static_cast<uint8_t>((measurementSequence >> 8) & 0xFF);
        payload[6] = static_cast<uint8_t>(static_cast<uint16_t>(stageTenths) & 0xFF);
        payload[7] = static_cast<uint8_t>((static_cast<uint16_t>(stageTenths) >> 8) & 0xFF);
        payload[8] = static_cast<uint8_t>(static_cast<uint16_t>(tempTenths) & 0xFF);
        payload[9] = static_cast<uint8_t>((static_cast<uint16_t>(tempTenths) >> 8) & 0xFF);
        payload[10] = static_cast<uint8_t>(raw16 & 0xFF);
        payload[11] = static_cast<uint8_t>((raw16 >> 8) & 0xFF);
        memcpy(&payload[12], macBytes, 6);
        payload[18] = static_cast<uint8_t>(loggerBleRssi);
        packet->decoded.payload.size = 19;
        packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
        packet->to = NODENUM_BROADCAST;
        packet->channel = 0;
        packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
        service->sendToMesh(packet, RX_SRC_LOCAL, true);

        bool cloudQueued = hoboHttpGatewayModule &&
            hoboHttpGatewayModule->queueLocalMX2001(latestStageFeet, latestTemperatureF, latestTemperatureC,
                                                    raw16, loggerMac, loggerBleRssi, measurementSequence);
        LOG_INFO("CCA HOBO AUTO MX2001 level=%.2f ft temp=%.1f F mesh=queued cloud=%s",
                 latestStageFeet, latestTemperatureF, cloudQueued ? "queued" : "FAILED");
        return cloudQueued;
    };

    auto publishAutomatic = [&]() -> bool {
        if (loggerType == LoggerType::MX2001 && measurementHasStage)
            return sendMX2001();
        if (loggerType == LoggerType::MX2201 || loggerType == LoggerType::MX2203)
            return sendTemperatureTelemetry(latestTemperatureC);
        return false;
    };

    if (now < 15000)
        return 500;

    if (!initialized) {
        if (!nimbleBluetooth || !nimbleBluetooth->isActive())
            return 1000;
        initializeClient();
        return 250;
    }

    if (clientConnectFailedEvent.exchange(false)) {
        LOG_WARN("CCA HOBO: connect failed reason=%d", clientDisconnectReason.load());
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        resetConnectionState();
        startScan();
        return 250;
    }

    if (clientDisconnectedEvent.exchange(false)) {
        LOG_WARN("CCA HOBO: disconnected reason=%d", clientDisconnectReason.load());
        resetConnectionState();
        startScan();
        return 250;
    }

    if (clientConnectedEvent.exchange(false)) {
        if (!finishServiceDiscovery()) {
            LOG_WARN("CCA HOBO: service/characteristic discovery failed for %s", loggerMac);
            rejectCurrentCandidate(TRANSIENT_RETRY_MS);
            if (hoboClient && hoboClient->isConnected())
                hoboClient->disconnect();
            return 250;
        }
    }

    if (readFailureReplyPending && readRequester) {
        sendTextReply(readRequester, readChannel, "HOBO READ failed");
        readFailureReplyPending = false;
        readRequester = 0;
    }

    if (!connected) {
        if (connecting)
            return 100;
        if (!loggerLockEnabled && scanStartedMs && reached(now, scanStartedMs + UNLOCKED_SCAN_WINDOW_MS)) {
            if (chooseUnlockedCandidate()) {
                NimBLEDevice::getScan()->stop();
                beginConnect();
                return 100;
            }
            scanStartedMs = now;
        }
        if (loggerLockEnabled) {
            bool pending = false;
            {
                std::lock_guard<std::mutex> guard(bleEventMutex);
                pending = candidatePending;
            }
            if (pending) {
                beginConnect();
                return 100;
            }
        }
        if (!NimBLEDevice::getScan()->isScanning())
            startScan();
        return 250;
    }

    if (readRequestPending && state == HoboState::READY) {
        readRequestPending = false;
        readRequestInProgress = true;
        readPurpose = ReadPurpose::ON_DEMAND;
        state = HoboState::SEND_READ;
        stateDueMs = now;
    }

    switch (state) {
    case HoboState::SEND_INIT:
        if (reached(now, stateDueMs)) {
            if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
                state = HoboState::WAIT_INIT;
                stateDueMs = now + COMMAND_DELAY_MS;
            } else {
                stateDueMs = now + 1000;
            }
        }
        break;

    case HoboState::WAIT_INIT:
        if (reached(now, stateDueMs)) {
            readPurpose = ReadPurpose::PROBE;
            state = HoboState::SEND_READ;
        }
        break;

    case HoboState::SEND_META0: {
        size_t length = 0;
        const uint8_t *command = meta0Command(length);
        if (sendCommand(command, length, "META0")) {
            state = HoboState::WAIT_META0;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case HoboState::WAIT_META0:
        if (reached(now, stateDueMs))
            state = HoboState::SEND_META8;
        break;

    case HoboState::SEND_META8: {
        size_t length = 0;
        const uint8_t *command = meta8Command(length);
        if (sendCommand(command, length, "META8")) {
            state = HoboState::WAIT_META8;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case HoboState::WAIT_META8:
        if (reached(now, stateDueMs)) {
            readPurpose = ReadPurpose::PROBE;
            state = HoboState::SEND_READ;
        }
        break;

    case HoboState::SEND_READ:
        resetMeasurementCapture();
        directReadActive = true;
        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            state = HoboState::WAIT_READ;
            stateDueMs = now + READ_TIMEOUT_MS;
        } else {
            directReadActive = false;
            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
            }
            state = HoboState::READY;
            nextStatusCheckMs = now + 1000;
        }
        break;

    case HoboState::WAIT_READ:
        if (measurementReady) {
            measurementReady = false;
            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                char reply[190] = {};
                if (loggerType == LoggerType::MX2001 && measurementHasStage)
                    snprintf(reply, sizeof(reply), "MX2001\nLogger: %s\nBLE: %d dBm\nLevel: %.2f ft\nTemp: %.1f F",
                             loggerMac, loggerBleRssi, latestStageFeet, latestTemperatureF);
                else if (loggerType == LoggerType::MX2203)
                    snprintf(reply, sizeof(reply), "MX2203\nLogger: %s\nBLE: %d dBm\nTemp: %.2f F / %.2f C",
                             loggerMac, loggerBleRssi, latestTemperatureF, latestTemperatureC);
                else
                    snprintf(reply, sizeof(reply), "MX2201\nLogger: %s\nBLE: %d dBm\nTemp: %.1f F",
                             loggerMac, loggerBleRssi, latestTemperatureF);
                sendTextReply(readRequester, readChannel, reply);
                readRequestInProgress = false;
                readRequester = 0;
                readPurpose = ReadPurpose::AUTOMATIC;
                state = HoboState::READY;
                break;
            }

            if (readPurpose == ReadPurpose::PROBE) {
                LOG_INFO("CCA HOBO: identified %s at %s; syncing logger interval", loggerTypeName(loggerType), loggerMac);
                readPurpose = ReadPurpose::AUTOMATIC;
                consecutiveStatusTimeouts = 0;
                statusTrackingAvailable = true;
                intervalPhaseLocked = false;
                state = HoboState::SEND_STATUS;
                stateDueMs = now + 200;
                break;
            }

            if (readPurpose == ReadPurpose::AUTOMATIC) {
                if (!haveStatusBaseline || pendingWritePointer == lastWritePointer) {
                    nextStatusCheckMs = now + POINTER_FINE_POLL_MS;
                    state = HoboState::READY;
                    break;
                }
                if (publishAutomatic()) {
                    const uint32_t previousTx = lastAutomaticTxMs;
                    const uint32_t cadence = previousTx ? now - previousTx : 0;
                    const uint32_t pointerToTx = pendingPointerDetectedMs ? now - pendingPointerDetectedMs : 0;
                    ++automaticTxCount;
                    lastAutomaticTxMs = now;
                    lastWritePointer = pendingWritePointer;
                    intervalPhaseLocked = true;
                    LOG_INFO("CCA HOBO AUTO confirmed count=%lu pointer=0x%08lX interval=%u pointer_to_tx=%lu cadence=%lu",
                             static_cast<unsigned long>(automaticTxCount), static_cast<unsigned long>(lastWritePointer),
                             loggerIntervalSeconds, static_cast<unsigned long>(pointerToTx), static_cast<unsigned long>(cadence));
                    nextStatusCheckMs = now + nextRecordPrecheckDelayMs();
                } else {
                    LOG_WARN("CCA HOBO: mesh/cloud enqueue incomplete; retaining logger pointer for retry");
                    nextStatusCheckMs = now + 1000;
                }
                pendingPointerDetectedMs = 0;
                state = HoboState::READY;
                break;
            }
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;
            if (readPurpose == ReadPurpose::PROBE && loggerType == LoggerType::UNKNOWN && prepareFallbackProbe()) {
                state = HoboState::SEND_META0;
                stateDueMs = now + 200;
                break;
            }
            if (readPurpose == ReadPurpose::ON_DEMAND && readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                readPurpose = ReadPurpose::AUTOMATIC;
                state = HoboState::READY;
                break;
            }
            if (readPurpose == ReadPurpose::PROBE && loggerType == LoggerType::UNKNOWN) {
                loggerType = LoggerType::UNSUPPORTED;
                rejectCurrentCandidate();
                if (hoboClient)
                    hoboClient->disconnect();
                break;
            }
            readPurpose = ReadPurpose::AUTOMATIC;
            nextStatusCheckMs = now + 1000;
            state = HoboState::READY;
        }
        break;

    case HoboState::SEND_STATUS:
        statusReady = false;
        if (sendCommand(CMD_STATUS, sizeof(CMD_STATUS), "STATUS")) {
            state = HoboState::WAIT_STATUS;
            stateDueMs = now + STATUS_TIMEOUT_MS;
        } else {
            ++consecutiveStatusTimeouts;
            if (consecutiveStatusTimeouts >= STATUS_TIMEOUT_LIMIT) {
                statusTrackingAvailable = false;
                nextStatusCheckMs = now + STATUS_RECOVERY_RETRY_MS;
                LOG_WARN("CCA HOBO: STATUS unavailable; automatic TX PAUSED");
            } else {
                nextStatusCheckMs = now + 1000;
            }
            state = HoboState::READY;
        }
        break;

    case HoboState::WAIT_STATUS:
        if (statusReady) {
            statusReady = false;
            consecutiveStatusTimeouts = 0;
            statusTrackingAvailable = true;
            if (!haveStatusBaseline) {
                haveStatusBaseline = true;
                lastWritePointer = currentWritePointer;
                intervalPhaseLocked = false;
                LOG_INFO("CCA HOBO: STATUS baseline pointer=0x%08lX interval=%u sec",
                         static_cast<unsigned long>(lastWritePointer), loggerIntervalSeconds);
                nextStatusCheckMs = now + POINTER_INITIAL_SYNC_POLL_MS;
                state = HoboState::READY;
                break;
            }
            if (currentWritePointer != lastWritePointer) {
                pendingWritePointer = currentWritePointer;
                pendingPointerDetectedMs = now;
                readPurpose = ReadPurpose::AUTOMATIC;
                state = HoboState::SEND_READ;
                stateDueMs = now;
            } else {
                nextStatusCheckMs = now + (intervalPhaseLocked ? POINTER_FINE_POLL_MS : POINTER_INITIAL_SYNC_POLL_MS);
                state = HoboState::READY;
            }
            break;
        }
        if (reached(now, stateDueMs)) {
            ++consecutiveStatusTimeouts;
            if (consecutiveStatusTimeouts >= STATUS_TIMEOUT_LIMIT) {
                statusTrackingAvailable = false;
                nextStatusCheckMs = now + STATUS_RECOVERY_RETRY_MS;
                LOG_WARN("CCA HOBO: pointer tracking unavailable; automatic TX PAUSED");
                state = HoboState::READY;
            } else {
                state = HoboState::SEND_STATUS;
                stateDueMs = now + 1000;
            }
        }
        break;

    case HoboState::READY:
        if (!readRequestPending && (nextStatusCheckMs == 0 || reached(now, nextStatusCheckMs))) {
            state = HoboState::SEND_STATUS;
            stateDueMs = now;
        }
        break;

    case HoboState::IDLE:
    default:
        break;
    }

    return 100;
}

#endif // ARCH_ESP32 && HELTEC_V4 && Bluetooth
