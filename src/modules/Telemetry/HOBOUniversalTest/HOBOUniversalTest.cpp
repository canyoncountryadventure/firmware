#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "HOBOUniversalTest.h"

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

// -----------------------------------------------------------------------------
// Shared Onset HOBO BLE service/characteristic used by the proven MX2201 and
// MX2001 integrations.
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

static const uint8_t CMD_INIT[] = {
    0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00
};

// The hardware-proven live sensor request is common to MX2201 and MX2001.
static const uint8_t CMD_NEWREAD64[] = {
    0x01, 0x01, 0x08, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// MX2001 initialization reads used by the existing MX2001 integration.
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

// MX2201 initialization reads used by the existing MX2201 integration.
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

// -----------------------------------------------------------------------------
// Model and state
// -----------------------------------------------------------------------------

enum class LoggerType : uint8_t
{
    UNKNOWN = 0,
    MX2201,
    MX2001,
    UNSUPPORTED
};

enum class MetaProfile : uint8_t
{
    NONE = 0,
    MX2201,
    MX2001
};

enum class TestState : uint8_t
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
TestState state = TestState::IDLE;
uint32_t stateDueMs = 0;

// Probe 0 = INIT then direct NEWREAD64.
// Probe 1 = preferred metadata profile then NEWREAD64.
// Probe 2 = alternate metadata profile then NEWREAD64.
uint8_t probeAttempt = 0;
bool candidateLikelyMX2001 = false;

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
uint16_t latestTemperatureRaw = 0;

uint8_t mx2001Fragment1[20] = {};
uint8_t mx2001Fragment2[20] = {};
uint16_t mx2001Fragment1Length = 0;
uint16_t mx2001Fragment2Length = 0;
bool gotMX2001Fragment1 = false;
bool gotMX2001Fragment2 = false;

bool readRequestPending = false;
bool readRequestInProgress = false;
uint32_t readRequester = 0;
uint8_t readChannel = 0;

static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t READ_TIMEOUT_MS = 3000;
static constexpr uint32_t REJECT_RETRY_MS = 60000;
static constexpr uint32_t TRANSIENT_RETRY_MS = 5000;
static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;

static constexpr uint16_t MX2201_MIN_RAW = 400;
static constexpr uint16_t MX2201_MAX_RAW = 2400;
static constexpr float MX2201_RAW_TO_F_SLOPE = 0.0771942720f;
static constexpr float MX2201_RAW_TO_F_INTERCEPT = -52.2825573f;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

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
    uint32_t bits = readBE32(p);
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
    case LoggerType::MX2201:
        return "MX2201";
    case LoggerType::MX2001:
        return "MX2001";
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

        if (pos + fieldLength >= len)
            break;

        const uint8_t type = data[pos + 1];
        const uint8_t *payload = &data[pos + 2];
        const uint16_t payloadLength = static_cast<uint16_t>(fieldLength - 1);

        // Manufacturer specific data. Onset's manufacturer payload observed by
        // the MX2001 decoder begins C5 00. We use the company prefix as a
        // generic HOBO candidate finder instead of a hard-coded logger MAC.
        if (type == 0xFF && payloadLength >= 2) {
            if (payload[0] == 0xC5 && payload[1] == 0x00) {
                result.onsetManufacturer = true;

                // Existing public MX2001 advertisements use a 22-byte
                // manufacturer payload. This is only a hint; final model
                // identification comes from the NEWREAD64 response.
                if (payloadLength == 22)
                    result.likelyMX2001 = true;
            }
        }

        // Complete/incomplete 128-bit service UUID lists.
        if ((type == 0x06 || type == 0x07) && payloadLength >= 16) {
            for (uint16_t offset = 0; offset + 16 <= payloadLength; offset += 16) {
                if (memcmp(payload + offset, HOBO_SERVICE_UUID, 16) == 0) {
                    result.hoboService = true;
                    break;
                }
            }
        }

        // Short/complete local name. This is a fallback for HOBO firmware that
        // does not expose manufacturer data or the service UUID in advertising.
        if ((type == 0x08 || type == 0x09) && payloadLength > 0) {
            if (containsAsciiIgnoreCase(payload, payloadLength, "HOBO") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2201") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2001")) {
                result.hoboName = true;
            }
        }

        pos += static_cast<uint16_t>(fieldLength) + 1;
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
        std::toupper(static_cast<unsigned char>(p[3])) != 'D')
        return false;

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

    LOG_INFO(
        "HOBO TEST TX %s requested=%u written=%u",
        name,
        length,
        written);

    return written == length;
}

void finishMX2001Measurement()
{
    if (!gotMX2001Fragment1 || !gotMX2001Fragment2)
        return;

    if (mx2001Fragment1Length < 19 || mx2001Fragment2Length < 7) {
        LOG_WARN("HOBO TEST: MX2001 fragments too short");
        directReadActive = false;
        return;
    }

    latestTemperatureRaw =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(mx2001Fragment1[17]) << 8) |
            static_cast<uint16_t>(mx2001Fragment1[18]));

    latestTemperatureF =
        -0.1805f * static_cast<float>(latestTemperatureRaw) + 169.64f;

    latestTemperatureC =
        (latestTemperatureF - 32.0f) * 5.0f / 9.0f;

    latestStageMeters = readBEFloat(&mx2001Fragment2[3]);
    latestStageFeet = latestStageMeters * 3.280839895f;

    if (!isfinite(latestTemperatureF) || !isfinite(latestStageFeet) ||
        latestTemperatureF < -50.0f || latestTemperatureF > 180.0f ||
        latestStageFeet < -100.0f || latestStageFeet > 1000.0f) {
        LOG_WARN("HOBO TEST: implausible MX2001 live measurement");
        directReadActive = false;
        return;
    }

    loggerType = LoggerType::MX2001;
    measurementHasStage = true;
    measurementReady = true;
    directReadActive = false;

    LOG_INFO("========================================");
    LOG_INFO("HOBO UNIVERSAL IDENTIFIED MX2001");
    LOG_INFO("Level: %.3f ft / %.5f m", latestStageFeet, latestStageMeters);
    LOG_INFO("Temp: %.2f F / %.2f C", latestTemperatureF, latestTemperatureC);
    logMac("Logger:", loggerMac);
    LOG_INFO("========================================");
}

// -----------------------------------------------------------------------------
// BLE callbacks
// -----------------------------------------------------------------------------

void notifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;

    if (!directReadActive || data == nullptr || len == 0)
        return;

    // MX2201 hardware-proven NEWREAD64 response:
    // 01 01 07 04 04 00 04 04 [TEMP32 BE] ...
    if (len >= 12 &&
        data[0] == 0x01 &&
        data[1] == 0x01 &&
        data[2] == 0x07 &&
        data[3] == 0x04 &&
        data[4] == 0x04 &&
        data[5] == 0x00 &&
        data[6] == 0x04 &&
        data[7] == 0x04) {

        const uint32_t raw32 = readBE32(&data[8]);

        if (raw32 < MX2201_MIN_RAW || raw32 > MX2201_MAX_RAW) {
            LOG_WARN(
                "HOBO TEST: MX2201 raw value implausible %lu",
                static_cast<unsigned long>(raw32));
            return;
        }

        latestTemperatureRaw = static_cast<uint16_t>(raw32);
        latestTemperatureF =
            MX2201_RAW_TO_F_SLOPE * static_cast<float>(latestTemperatureRaw) +
            MX2201_RAW_TO_F_INTERCEPT;
        latestTemperatureC =
            (latestTemperatureF - 32.0f) * 5.0f / 9.0f;

        loggerType = LoggerType::MX2201;
        measurementHasStage = false;
        measurementReady = true;
        directReadActive = false;

        LOG_INFO("========================================");
        LOG_INFO("HOBO UNIVERSAL IDENTIFIED MX2201");
        LOG_INFO("Temp: %.2f F / %.2f C", latestTemperatureF, latestTemperatureC);
        logMac("Logger:", loggerMac);
        LOG_INFO("========================================");
        return;
    }

    // MX2001 first NEWREAD64 fragment.
    if (len >= 20 &&
        data[0] == 0x01 &&
        data[1] == 0x02 &&
        data[2] == 0x04 &&
        data[3] == 0x04) {

        mx2001Fragment1Length =
            (len > sizeof(mx2001Fragment1)) ? sizeof(mx2001Fragment1) : len;
        memcpy(mx2001Fragment1, data, mx2001Fragment1Length);
        gotMX2001Fragment1 = true;
        loggerType = LoggerType::MX2001;
    }
    // MX2001 continuation fragment.
    else if (len >= 7 && data[0] == 0x02) {
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

    LOG_INFO("========================================");
    LOG_INFO("HOBO UNIVERSAL CANDIDATE FOUND");
    logMac("Logger candidate:", loggerMac);
    LOG_INFO("BLE RSSI: %d dBm", loggerBleRssi);
    LOG_INFO(
        "Finder match: Onset=%s Service=%s Name=%s MX2001Hint=%s",
        info.onsetManufacturer ? "yes" : "no",
        info.hoboService ? "yes" : "no",
        info.hoboName ? "yes" : "no",
        info.likelyMX2001 ? "yes" : "no");
    LOG_INFO("Connecting to discover HOBO command service...");
    LOG_INFO("========================================");

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        LOG_WARN("HOBO TEST: connection request failed");
        Bluefruit.Scanner.start(0);
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connected = true;
    connectionHandle = connHandle;

    LOG_INFO("HOBO TEST: BLE connected; settling before service discovery");
    delay(SERVICE_SETTLE_MS);

    bool serviceFound = false;
    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        LOG_INFO(
            "HOBO TEST: service discovery attempt %u/%u",
            attempt,
            SERVICE_DISCOVERY_ATTEMPTS);

        if (hoboService.discover(connHandle)) {
            serviceFound = true;
            break;
        }

        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!serviceFound) {
        LOG_WARN("HOBO TEST: candidate has no HOBO service after retries");
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

        LOG_WARN(
            "HOBO TEST: characteristic discovery attempt %u/%u failed",
            attempt,
            SERVICE_DISCOVERY_ATTEMPTS);

        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!characteristicFound) {
        LOG_WARN("HOBO TEST: HOBO characteristic discovery failed after retries");
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

        LOG_WARN(
            "HOBO TEST: notification enable attempt %u/%u failed",
            attempt,
            SERVICE_DISCOVERY_ATTEMPTS);

        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!notifyEnabled) {
        LOG_WARN("HOBO TEST: notification enable failed after retries");
        rejectCurrentCandidate(TRANSIENT_RETRY_MS);
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("HOBO TEST: command channel ready");

    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    probeAttempt = 0;
    resetMeasurementCapture();

    state = TestState::SEND_INIT;
    stateDueMs = millis() + 500;
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("HOBO TEST: disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    connectionHandle = BLE_CONN_HANDLE_INVALID;
    loggerType = LoggerType::UNKNOWN;
    activeMetaProfile = MetaProfile::NONE;
    state = TestState::IDLE;
    resetMeasurementCapture();

    if (readRequestInProgress || readRequestPending) {
        readRequestInProgress = false;
        readRequestPending = false;
        readRequester = 0;
    }
}

void initializeClient()
{
    LOG_INFO("========================================");
    LOG_INFO("HOBO UNIVERSAL TEMP TEST MODULE");
    LOG_INFO("Target hardware: Seeed XIAO nRF52840");
    LOG_INFO("Supports test READ for MX2201 + MX2001");
    LOG_INFO("Finder: no hard-coded HOBO MAC");
    LOG_INFO("MX2201 READ => temperature");
    LOG_INFO("MX2001 READ => temperature + water level");
    LOG_INFO("========================================");

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
        LOG_WARN("HOBO TEST: scanner failed to start");
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

HOBOUniversalTestModule::HOBOUniversalTestModule()
    : SinglePortModule(
          "HOBOUniversalTest",
          meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread(
          "HOBOUniversalTest")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool HOBOUniversalTestModule::wantPacket(
    const meshtastic_MeshPacket *p)
{
    if (p == nullptr)
        return false;

    return
        p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP ||
        p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOUniversalTestModule::handleReceived(
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

    LOG_INFO(
        "HOBO TEST: READ DM received from !%08lx",
        static_cast<unsigned long>(mp.from));

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

bool HOBOUniversalTestModule::sendTextReply(
    uint32_t destination,
    uint8_t channel,
    const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();

    if (packet == nullptr) {
        LOG_WARN("HOBO TEST: text packet allocation failed");
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

int32_t HOBOUniversalTestModule::runOnce()
{
    const uint32_t now = millis();

    // Let normal Meshtastic BLE initialization settle first.
    if (now < 15000)
        return 500;

    if (!initialized) {
        initializeClient();
        return 500;
    }

    if (!connected) {
        if (connecting)
            return 500;

        if (!Bluefruit.Scanner.isRunning()) {
            LOG_INFO("HOBO TEST: scanning for any HOBO candidate");
            Bluefruit.Scanner.start(0);
        }

        return 500;
    }

    // A remote READ received during startup waits until model discovery is
    // complete, then triggers a fresh direct read.
    if (readRequestPending && state == TestState::READY) {
        readRequestPending = false;
        readRequestInProgress = true;
        state = TestState::SEND_READ;
        stateDueMs = now;
    }

    switch (state) {
    case TestState::SEND_INIT:
        if (!reached(now, stateDueMs))
            break;

        if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
            state = TestState::WAIT_INIT;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;

    case TestState::WAIT_INIT:
        if (reached(now, stateDueMs)) {
            // First attempt deliberately skips model-specific metadata. If the
            // logger requires its old initialization reads, the timeout path
            // automatically tries both known profiles.
            state = TestState::SEND_READ;
        }
        break;

    case TestState::SEND_META0: {
        size_t length = 0;
        const uint8_t *command = meta0Command(length);
        const char *name =
            activeMetaProfile == MetaProfile::MX2001 ?
                "MX2001 META0" : "MX2201 META0";

        if (sendCommand(command, static_cast<uint16_t>(length), name)) {
            state = TestState::WAIT_META0;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case TestState::WAIT_META0:
        if (reached(now, stateDueMs))
            state = TestState::SEND_META8;
        break;

    case TestState::SEND_META8: {
        size_t length = 0;
        const uint8_t *command = meta8Command(length);
        const char *name =
            activeMetaProfile == MetaProfile::MX2001 ?
                "MX2001 META8" : "MX2201 META8";

        if (sendCommand(command, static_cast<uint16_t>(length), name)) {
            state = TestState::WAIT_META8;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;
    }

    case TestState::WAIT_META8:
        if (reached(now, stateDueMs))
            state = TestState::SEND_READ;
        break;

    case TestState::SEND_READ:
        resetMeasurementCapture();
        directReadActive = true;

        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            state = TestState::WAIT_READ;
            stateDueMs = now + READ_TIMEOUT_MS;
        } else {
            directReadActive = false;
            stateDueMs = now + 1000;
        }
        break;

    case TestState::WAIT_READ:
        if (measurementReady) {
            measurementReady = false;

            LOG_INFO(
                "HOBO TEST: live READ complete model=%s",
                loggerTypeName(loggerType));

            if (readRequestInProgress) {
                char reply[128] = {};

                if (loggerType == LoggerType::MX2201) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2201\nTemp: %.1f F",
                        latestTemperatureF);
                } else if (loggerType == LoggerType::MX2001 && measurementHasStage) {
                    snprintf(
                        reply,
                        sizeof(reply),
                        "MX2001\nLevel: %.2f ft\nTemp: %.1f F",
                        latestStageFeet,
                        latestTemperatureF);
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

            state = TestState::READY;
            break;
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;

            // During startup discovery, try both known metadata profiles before
            // declaring the candidate unsupported.
            if (loggerType == LoggerType::UNKNOWN && prepareFallbackProbe()) {
                LOG_WARN(
                    "HOBO TEST: direct READ timeout; trying %s metadata profile",
                    activeMetaProfile == MetaProfile::MX2001 ? "MX2001" : "MX2201");
                state = TestState::SEND_META0;
                stateDueMs = now + 200;
                break;
            }

            if (readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "HOBO READ failed");
                readRequestInProgress = false;
                readRequester = 0;
                state = TestState::READY;
                break;
            }

            if (loggerType == LoggerType::UNKNOWN) {
                loggerType = LoggerType::UNSUPPORTED;
                LOG_WARN("HOBO TEST: candidate did not match MX2201 or MX2001 READ response");
                rejectCurrentCandidate();
                Bluefruit.disconnect(connectionHandle);
            } else {
                state = TestState::READY;
            }
        }
        break;

    case TestState::READY:
        // Idle until a direct Meshtastic READ command arrives.
        break;

    default:
        break;
    }

    return 100;
}

#endif
