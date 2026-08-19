#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "MX2203Discovery.h"
#include "main.h"

#include <bluefruit.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{

// Common HOBO BLE service and command characteristic already proven on
// MX2201/MX2001. MX2203 discovery deliberately treats these as hypotheses;
// all advertisement and notification bytes are logged before any decoder is
// assumed.
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

static const uint8_t CMD_INIT[] = {
    0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00
};

static const uint8_t CMD_NEWREAD64[] = {
    0x01, 0x01, 0x08, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

enum class DiscoveryState : uint8_t
{
    IDLE = 0,
    SEND_INIT,
    WAIT_INIT,
    SEND_LIVE_READ,
    WAIT_LIVE_READ,
    READY
};

bool initialized = false;
bool connecting = false;
bool connected = false;
uint16_t connectionHandle = BLE_CONN_HANDLE_INVALID;
DiscoveryState state = DiscoveryState::IDLE;
uint32_t stateDueMs = 0;
bool notificationSeen = false;

uint8_t candidateAddrRaw[6] = {};
uint8_t candidateMac[6] = {};
int8_t candidateRssi = 0;

uint8_t cooldownAddrRaw[6] = {};
bool haveCooldownAddr = false;
uint32_t cooldownUntilMs = 0;

static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;
static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t LIVE_READ_TIMEOUT_MS = 5000;
static constexpr uint32_t TRANSIENT_RETRY_MS = 5000;

bool reached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
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
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void logHex(const char *prefix, const uint8_t *data, uint16_t len)
{
    if (data == nullptr || len == 0) {
        LOG_INFO("%s <empty>", prefix);
        return;
    }

    const uint16_t capped = len > 64 ? 64 : len;
    char hex[(64 * 3) + 1] = {};
    size_t pos = 0;

    for (uint16_t i = 0; i < capped && pos + 4 < sizeof(hex); ++i) {
        const int written = snprintf(
            hex + pos,
            sizeof(hex) - pos,
            i == 0 ? "%02X" : " %02X",
            data[i]);

        if (written <= 0)
            break;

        pos += static_cast<size_t>(written);
    }

    if (len > capped)
        LOG_INFO("%s len=%u bytes=%s ...", prefix, len, hex);
    else
        LOG_INFO("%s len=%u bytes=%s", prefix, len, hex);
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

struct AdvertisementInfo
{
    bool candidate;
    bool onsetManufacturer;
    bool commonHoboService;
    bool hoboName;
    uint16_t manufacturerLength;
    char localName[32];
};

AdvertisementInfo inspectAdvertisement(const ble_gap_evt_adv_report_t *report)
{
    AdvertisementInfo result = {};

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

        if (type == 0xFF && payloadLength >= 2) {
            if (payload[0] == 0xC5 && payload[1] == 0x00) {
                result.onsetManufacturer = true;
                result.manufacturerLength = payloadLength;
            }
        }

        if ((type == 0x06 || type == 0x07) && payloadLength >= 16) {
            for (uint16_t offset = 0; offset + 16 <= payloadLength; offset += 16) {
                if (memcmp(payload + offset, HOBO_SERVICE_UUID, 16) == 0) {
                    result.commonHoboService = true;
                    break;
                }
            }
        }

        if ((type == 0x08 || type == 0x09) && payloadLength > 0) {
            const uint16_t copyLength =
                payloadLength >= sizeof(result.localName)
                    ? static_cast<uint16_t>(sizeof(result.localName) - 1)
                    : payloadLength;

            memcpy(result.localName, payload, copyLength);
            result.localName[copyLength] = '\0';

            if (containsAsciiIgnoreCase(payload, payloadLength, "HOBO") ||
                containsAsciiIgnoreCase(payload, payloadLength, "MX2203")) {
                result.hoboName = true;
            }
        }

        pos = fieldEnd;
    }

    result.candidate =
        result.onsetManufacturer ||
        result.commonHoboService ||
        result.hoboName;

    return result;
}

void logAdvertisementFields(const ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr || report->data.p_data == nullptr)
        return;

    const uint8_t *data = report->data.p_data;
    const uint16_t len = report->data.len;
    logHex("MX2203 DISCOVERY ADV RAW", data, len);

    uint16_t pos = 0;
    uint8_t index = 0;

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

        char prefix[48] = {};
        snprintf(
            prefix,
            sizeof(prefix),
            "MX2203 ADV field %u type=0x%02X",
            index,
            type);
        logHex(prefix, payload, payloadLength);

        pos = fieldEnd;
        ++index;
    }
}

bool candidateInCooldown(const uint8_t rawAddr[6])
{
    if (!haveCooldownAddr)
        return false;

    if (reached(millis(), cooldownUntilMs)) {
        haveCooldownAddr = false;
        return false;
    }

    return memcmp(rawAddr, cooldownAddrRaw, 6) == 0;
}

void cooldownCurrentCandidate()
{
    memcpy(cooldownAddrRaw, candidateAddrRaw, 6);
    haveCooldownAddr = true;
    cooldownUntilMs = millis() + TRANSIENT_RETRY_MS;
}

bool sendCommand(const uint8_t *command, uint16_t length, const char *name)
{
    if (!connected)
        return false;

    const uint16_t written = hoboCharacteristic.write(command, length);

    LOG_INFO(
        "MX2203 DISCOVERY TX %s requested=%u written=%u",
        name,
        length,
        written);

    return written == length;
}

void notifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;

    notificationSeen = true;
    logHex("MX2203 DISCOVERY RX NOTIFY", data, len);
}

void scanCallback(ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr)
        return;

    if (connecting || connected) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (candidateInCooldown(report->peer_addr.addr)) {
        Bluefruit.Scanner.resume();
        return;
    }

    const AdvertisementInfo info = inspectAdvertisement(report);

    if (!info.candidate) {
        Bluefruit.Scanner.resume();
        return;
    }

    memcpy(candidateAddrRaw, report->peer_addr.addr, 6);
    makeHumanMac(report->peer_addr.addr, candidateMac);
    candidateRssi = report->rssi;

    LOG_INFO("========================================");
    LOG_INFO("MX2203 DISCOVERY: ONSET/HOBO CANDIDATE");
    logMac("Candidate MAC:", candidateMac);
    LOG_INFO("RSSI: %d dBm", candidateRssi);
    LOG_INFO(
        "Match: Onset=%s CommonService=%s Name=%s ManufacturerLen=%u",
        info.onsetManufacturer ? "yes" : "no",
        info.commonHoboService ? "yes" : "no",
        info.hoboName ? "yes" : "no",
        info.manufacturerLength);

    if (info.localName[0] != '\0')
        LOG_INFO("Local name: %s", info.localName);

    logAdvertisementFields(report);
    LOG_INFO("Connecting to test the known HOBO command service...");
    LOG_INFO("========================================");

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        cooldownCurrentCandidate();
        LOG_WARN("MX2203 DISCOVERY: connection request failed");
        Bluefruit.Scanner.start(0);
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connected = true;
    connectionHandle = connHandle;

    LOG_INFO("MX2203 DISCOVERY: BLE connected; settling");
    delay(SERVICE_SETTLE_MS);

    bool serviceFound = false;

    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        LOG_INFO(
            "MX2203 DISCOVERY: common service attempt %u/%u",
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
        LOG_WARN("MX2203 DISCOVERY: known MX2201/MX2001 HOBO service NOT found");
        cooldownCurrentCandidate();
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2203 DISCOVERY: known HOBO service FOUND");

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
        LOG_WARN("MX2203 DISCOVERY: known command characteristic NOT found");
        cooldownCurrentCandidate();
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2203 DISCOVERY: known command characteristic FOUND");

    if (!hoboCharacteristic.enableNotify()) {
        LOG_WARN("MX2203 DISCOVERY: notification enable failed");
        cooldownCurrentCandidate();
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2203 DISCOVERY: notifications enabled; probing raw live read");

    notificationSeen = false;
    state = DiscoveryState::SEND_INIT;
    stateDueMs = millis() + 500;
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("MX2203 DISCOVERY: disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    connectionHandle = BLE_CONN_HANDLE_INVALID;
    notificationSeen = false;
    state = DiscoveryState::IDLE;
}

void initializeDiscoveryClient()
{
    LOG_INFO("========================================");
    LOG_INFO("MX2203 DISCOVERY TEST MODULE");
    LOG_INFO("Target: HOBO TidbiT MX2203");
    LOG_INFO("Mode: active BLE scan + raw advertisement logging");
    LOG_INFO("Then test common HOBO service + raw NEWREAD64 response");
    LOG_INFO("No MX2203 decoder assumptions are made");
    LOG_INFO("========================================");

    hoboService.begin();
    hoboCharacteristic.setNotifyCallback(notifyCallback);
    hoboCharacteristic.begin(&hoboService);

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(false);
    Bluefruit.Scanner.setInterval(160, 80);
    Bluefruit.Scanner.useActiveScan(true);

    initialized = true;

    if (!Bluefruit.Scanner.start(0))
        LOG_WARN("MX2203 DISCOVERY: scanner failed to start");
}

} // namespace

MX2203DiscoveryModule::MX2203DiscoveryModule()
    : concurrency::OSThread("MX2203Discovery")
{
    setIntervalFromNow(500);
}

int32_t MX2203DiscoveryModule::runOnce()
{
    const uint32_t now = millis();

    // Let normal Meshtastic BLE initialization settle first.
    if (now < 15000)
        return 500;

    if (!initialized) {
        initializeDiscoveryClient();
        return 500;
    }

    if (!connected) {
        if (connecting)
            return 500;

        if (!Bluefruit.Scanner.isRunning()) {
            LOG_INFO("MX2203 DISCOVERY: scanning for Onset/HOBO advertisements");
            Bluefruit.Scanner.start(0);
        }

        return 500;
    }

    switch (state) {
    case DiscoveryState::SEND_INIT:
        if (!reached(now, stateDueMs))
            break;

        if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
            state = DiscoveryState::WAIT_INIT;
            stateDueMs = now + COMMAND_DELAY_MS;
        } else {
            stateDueMs = now + 1000;
        }
        break;

    case DiscoveryState::WAIT_INIT:
        if (reached(now, stateDueMs)) {
            notificationSeen = false;
            state = DiscoveryState::SEND_LIVE_READ;
        }
        break;

    case DiscoveryState::SEND_LIVE_READ:
        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            state = DiscoveryState::WAIT_LIVE_READ;
            stateDueMs = now + LIVE_READ_TIMEOUT_MS;
        } else {
            stateDueMs = now + 1000;
        }
        break;

    case DiscoveryState::WAIT_LIVE_READ:
        if (notificationSeen) {
            LOG_INFO("MX2203 DISCOVERY: raw response captured; leave connected for inspection");
            state = DiscoveryState::READY;
        } else if (reached(now, stateDueMs)) {
            LOG_WARN("MX2203 DISCOVERY: NEWREAD64 produced no notification within timeout");
            state = DiscoveryState::READY;
        }
        break;

    case DiscoveryState::READY:
        // Remain connected. All asynchronous notifications continue to be
        // printed raw for reverse-engineering.
        break;

    default:
        break;
    }

    return 100;
}

#endif
