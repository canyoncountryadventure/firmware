#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "HOBOMX2001MX2201MX2203Telemetry.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "main.h"

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
    READY
};

bool initialized = false;
bool connecting = false;
bool connected = false;
uint16_t connectionHandle = BLE_CONN_HANDLE_INVALID;

LoggerType loggerType = LoggerType::UNKNOWN;
MetaProfile activeMetaProfile = MetaProfile::NONE;
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

bool readRequestPending = false;
bool readRequestInProgress = false;
bool readFailureReplyPending = false;
uint32_t readRequester = 0;
uint8_t readChannel = 0;

static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t READ_TIMEOUT_MS = 3000;
static constexpr uint32_t REJECT_RETRY_MS = 60000;
static constexpr uint32_t TRANSIENT_RETRY_MS = 5000;
static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;

// Preserve the hardware-proven MX2201 conversion from the combined reader.
static constexpr uint32_t MX2201_MIN_RAW = 400;
static constexpr uint32_t MX2201_MAX_RAW = 2400;
static constexpr float MX2201_RAW_TO_F_SLOPE = 0.0771942720f;
static constexpr float MX2201_RAW_TO_F_INTERCEPT = -52.2825573f;

// HOBOconnect / OnsetSDK TempSensor2F conversion for MX2203.
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

            // Existing MX2001 discovery hint retained from the proven reader.
            if (payloadLength == 22)
                result.likelyMX2001 = true;

            // Hardware-observed MX2203 manufacturer signature:
            // C5 00 ... 01 03 22 02 ...
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

bool isReadCommand(const uint8_t *bytes, size_t size)
{
    if (bytes == nullptr || size == 0)
        return false;

    char command[24] = {};
    size_t n = size;
    if (n > sizeof(command) - 1)
        n = sizeof(command) - 1;

    memcpy(command, bytes, n);

    char *p = command;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    if (*p == '/')
        ++p;

    if (strlen(p) < 4)
        return false;

    if (std::toupper(static_cast<unsigned char>(p[0])) != 'R' ||
        std::toupper(static_cast<unsigned char>(p[1])) != 'E' ||
        std::toupper(static_cast<unsigned char>(p[2])) != 'A' ||
        std::toupper(static_cast<unsigned char>(p[3])) != 'D') {
        return false;
    }

    return p[4] == '\0' ||
           std::isspace(static_cast<unsigned char>(p[4]));
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

    if (!directReadActive || data == nullptr || len == 0)
        return;

    // MX2203 NEWREAD64 response:
    // 01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
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

    // MX2201 NEWREAD64 response:
    // 01 01 07 04 04 00 04 04 [TEMP32 BE] ...
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

    // MX2001 first NEWREAD64 fragment.
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
        "HOBO universal: candidate %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d",
        loggerMac[0], loggerMac[1], loggerMac[2],
        loggerMac[3], loggerMac[4], loggerMac[5],
        loggerBleRssi);

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
    probeAttempt = 0;
    resetMeasurementCapture();

    universalState = UniversalState::SEND_INIT;
    stateDueMs = millis() + 500;

    LOG_INFO("HOBO universal: BLE command channel ready");
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
    universalState = UniversalState::IDLE;
    resetMeasurementCapture();

    if (readRequestInProgress || readRequestPending) {
        readRequestInProgress = false;
        readRequestPending = false;
        readFailureReplyPending = (readRequester != 0);
    }
}

void initializeClient()
{
    LOG_INFO("HOBO universal bridge: MX2001 + MX2201 + MX2203");

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
    // The MX2203 is hardware-proven to answer the direct INIT + NEWREAD64 path.
    // Do not send MX2001/MX2201 metadata commands to a positively identified
    // MX2203 advertisement if its direct read fails.
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

    if (!isReadCommand(mp.decoded.payload.bytes, mp.decoded.payload.size))
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
        if (reached(now, stateDueMs))
            universalState = UniversalState::SEND_READ;
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
        if (reached(now, stateDueMs))
            universalState = UniversalState::SEND_READ;
        break;

    case UniversalState::SEND_READ:
        resetMeasurementCapture();
        directReadActive = true;

        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            universalState = UniversalState::WAIT_READ;
            stateDueMs = now + READ_TIMEOUT_MS;
        } else {
            directReadActive = false;
            if (readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                universalState = UniversalState::READY;
            } else {
                stateDueMs = now + 1000;
            }
        }
        break;

    case UniversalState::WAIT_READ:
        if (measurementReady) {
            measurementReady = false;

            LOG_INFO(
                "HOBO universal: READ complete model=%s",
                loggerTypeName(loggerType));

            if (readRequestInProgress) {
                char reply[128] = {};

                if (loggerType == LoggerType::MX2001 && measurementHasStage) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2001\nLevel: %.2f ft\nTemp: %.1f F",
                        latestStageFeet,
                        latestTemperatureF);
                } else if (loggerType == LoggerType::MX2201) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2201\nTemp: %.1f F",
                        latestTemperatureF);
                } else if (loggerType == LoggerType::MX2203) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2203\nTemp: %.2f F / %.2f C",
                        latestTemperatureF,
                        latestTemperatureC);
                } else {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "HOBO read completed; model unknown");
                }

                sendTextReply(readRequester, readChannel, reply);
                readRequestInProgress = false;
                readRequester = 0;
            }

            universalState = UniversalState::READY;
            break;
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;

            if (loggerType == LoggerType::UNKNOWN && prepareFallbackProbe()) {
                universalState = UniversalState::SEND_META0;
                stateDueMs = now + 200;
                break;
            }

            if (readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                universalState = UniversalState::READY;
                break;
            }

            if (loggerType == LoggerType::UNKNOWN) {
                loggerType = LoggerType::UNSUPPORTED;
                LOG_WARN("HOBO universal: unsupported NEWREAD64 response");
                rejectCurrentCandidate();
                Bluefruit.disconnect(connectionHandle);
            } else {
                universalState = UniversalState::READY;
            }
        }
        break;

    case UniversalState::READY:
        // No periodic polling. Idle until a direct Meshtastic READ arrives.
        break;

    case UniversalState::IDLE:
    default:
        break;
    }

    return 100;
}

#endif
