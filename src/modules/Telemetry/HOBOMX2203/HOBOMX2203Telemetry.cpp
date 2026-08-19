#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "HOBOMX2203Telemetry.h"

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
// Onset HOBO MX2203 BLE protocol
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

static const uint8_t CMD_INIT[] = {
    0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00
};

// Hardware-proven live read command.
static const uint8_t CMD_NEWREAD64[] = {
    0x01, 0x01, 0x08, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// -----------------------------------------------------------------------------
// Official OnsetSDK MX2203 temperature conversion
// -----------------------------------------------------------------------------
//
// Recovered from OnsetSDK.dll inside the HOBOconnect Android APKs on
// 2026-08-19. MX2203 maps to TempSensor2F, a 14-bit sensor:
//
//   C = raw * 175.72 / 16384 - 46.85
//   F = C * 9/5 + 32
//
// See ONSETSDK.md in this folder for the permanent APK/reverse-engineering
// record.

static constexpr float MX2203_CONST_A = 175.72f;
static constexpr float MX2203_FULL_RAW = 16384.0f;
static constexpr float MX2203_CONST_C = 46.85f;
static constexpr uint32_t MX2203_MAX_RAW = 16383;

static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;
static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t READ_TIMEOUT_MS = 3000;

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

// Use a module-specific name because Meshtastic's arduino-fsm dependency also
// exposes a global type named State.
enum class MX2203State : uint8_t
{
    IDLE = 0,
    SEND_INIT,
    WAIT_INIT,
    READY,
    SEND_READ,
    WAIT_READ
};

bool initialized = false;
bool connecting = false;
bool connected = false;
uint16_t connectionHandle = BLE_CONN_HANDLE_INVALID;
MX2203State mx2203State = MX2203State::IDLE;
uint32_t stateDueMs = 0;

bool directReadActive = false;
bool measurementReady = false;
uint32_t latestRaw = 0;
float latestTemperatureF = NAN;
float latestTemperatureC = NAN;

bool readRequestPending = false;
bool readRequestInProgress = false;
bool readFailureReplyPending = false;
uint32_t readRequester = 0;
uint8_t readChannel = 0;

uint8_t loggerMac[6] = {};

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

void makeHumanMac(const uint8_t in[6], uint8_t out[6])
{
    for (uint8_t i = 0; i < 6; ++i)
        out[i] = in[5 - i];
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

bool isMX2203Advertisement(const ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr || report->data.p_data == nullptr)
        return false;

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

        // Hardware-observed MX2203 manufacturer signature:
        // C5 00 ... 01 03 22 02 ...
        if (type == 0xFF && payloadLength >= 10 &&
            payload[0] == 0xC5 && payload[1] == 0x00 &&
            payload[6] == 0x01 && payload[7] == 0x03 &&
            payload[8] == 0x22 && payload[9] == 0x02) {
            return true;
        }

        pos = fieldEnd;
    }

    return false;
}

bool sendCommand(const uint8_t *command, uint16_t length)
{
    if (!connected)
        return false;

    const uint16_t written = hoboCharacteristic.write(command, length);
    if (written != length) {
        LOG_WARN(
            "MX2203: BLE command short write requested=%u written=%u",
            length,
            written);
        return false;
    }

    return true;
}

void resetMeasurement()
{
    directReadActive = false;
    measurementReady = false;
    latestRaw = 0;
    latestTemperatureF = NAN;
    latestTemperatureC = NAN;
}

void notifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;

    if (!directReadActive || data == nullptr || len < 12)
        return;

    // Hardware-proven MX2203 NEWREAD64 response:
    // 01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
    if (data[0] != 0x01 || data[1] != 0x01 || data[2] != 0x0B ||
        data[3] != 0x04 || data[4] != 0x04 || data[5] != 0x00 ||
        data[6] != 0x04 || data[7] != 0x04) {
        return;
    }

    const uint32_t raw = readBE32(&data[8]);
    if (raw > MX2203_MAX_RAW) {
        LOG_WARN(
            "MX2203: invalid 14-bit temperature raw=%lu",
            static_cast<unsigned long>(raw));
        return;
    }

    const float tempC =
        static_cast<float>(raw) * MX2203_CONST_A / MX2203_FULL_RAW -
        MX2203_CONST_C;
    const float tempF = tempC * (9.0f / 5.0f) + 32.0f;

    if (!isfinite(tempC) || !isfinite(tempF)) {
        LOG_WARN("MX2203: non-finite temperature result");
        return;
    }

    latestRaw = raw;
    latestTemperatureC = tempC;
    latestTemperatureF = tempF;
    measurementReady = true;
    directReadActive = false;
}

void scanCallback(ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr)
        return;

    if (connecting || connected) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (!isMX2203Advertisement(report)) {
        Bluefruit.Scanner.resume();
        return;
    }

    makeHumanMac(report->peer_addr.addr, loggerMac);
    connecting = true;
    Bluefruit.Scanner.stop();

    LOG_INFO(
        "MX2203: logger found %02X:%02X:%02X:%02X:%02X:%02X",
        loggerMac[0], loggerMac[1], loggerMac[2],
        loggerMac[3], loggerMac[4], loggerMac[5]);

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        LOG_WARN("MX2203: BLE connection request failed");
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
        LOG_WARN("MX2203: HOBO service discovery failed");
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
        LOG_WARN("MX2203: HOBO command characteristic discovery failed");
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
        LOG_WARN("MX2203: BLE notifications could not be enabled");
        Bluefruit.disconnect(connHandle);
        return;
    }

    resetMeasurement();
    mx2203State = MX2203State::SEND_INIT;
    stateDueMs = millis() + 500;

    LOG_INFO("MX2203: BLE command channel ready");
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("MX2203: BLE disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    connectionHandle = BLE_CONN_HANDLE_INVALID;
    mx2203State = MX2203State::IDLE;
    resetMeasurement();

    if (readRequestInProgress || readRequestPending) {
        readRequestInProgress = false;
        readRequestPending = false;
        readFailureReplyPending = (readRequester != 0);
    }
}

void initializeClient()
{
    LOG_INFO("MX2203 bridge: starting BLE client");

    hoboService.begin();
    hoboCharacteristic.setNotifyCallback(notifyCallback);
    hoboCharacteristic.begin(&hoboService);

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(false);
    Bluefruit.Scanner.setInterval(160, 80);

    // Match the proven production MX2201/MX2001 dual-role BLE behavior.
    Bluefruit.Scanner.useActiveScan(false);

    initialized = true;

    if (!Bluefruit.Scanner.start(0))
        LOG_WARN("MX2203: scanner failed to start");
}

} // namespace

HOBOMX2203TelemetryModule::HOBOMX2203TelemetryModule()
    : SinglePortModule(
          "HOBOMX2203Telemetry",
          meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread(
          "HOBOMX2203Telemetry")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool HOBOMX2203TelemetryModule::wantPacket(
    const meshtastic_MeshPacket *p)
{
    if (p == nullptr)
        return false;

    return
        p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP ||
        p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOMX2203TelemetryModule::handleReceived(
    const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    if (nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();

    // READ is accepted only as a direct message to this node.
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    if (!isReadCommand(mp.decoded.payload.bytes, mp.decoded.payload.size))
        return ProcessMessage::CONTINUE;

    if (!connected) {
        sendTextReply(mp.from, mp.channel, "MX2203 unavailable");
        return ProcessMessage::CONTINUE;
    }

    if (readRequestPending || readRequestInProgress) {
        sendTextReply(mp.from, mp.channel, "MX2203 read already in progress");
        return ProcessMessage::CONTINUE;
    }

    readRequester = mp.from;
    readChannel = mp.channel;
    readRequestPending = true;
    setIntervalFromNow(10);

    return ProcessMessage::CONTINUE;
}

bool HOBOMX2203TelemetryModule::sendTextReply(
    uint32_t destination,
    uint8_t channel,
    const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("MX2203: text packet allocation failed");
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

int32_t HOBOMX2203TelemetryModule::runOnce()
{
    const uint32_t now = millis();

    // Let Meshtastic's normal BLE peripheral initialization settle before
    // starting the central role used for the HOBO connection.
    if (now < 15000)
        return 500;

    if (!initialized) {
        initializeClient();
        return 500;
    }

    if (readFailureReplyPending && readRequester != 0) {
        sendTextReply(readRequester, readChannel, "MX2203 READ failed");
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

    // A READ received while INIT is finishing waits here until the logger is
    // ready, then causes exactly one fresh NEWREAD64 request.
    if (readRequestPending && mx2203State == MX2203State::READY) {
        readRequestPending = false;
        readRequestInProgress = true;
        mx2203State = MX2203State::SEND_READ;
        stateDueMs = now;
    }

    switch (mx2203State) {
    case MX2203State::SEND_INIT:
        if (!reached(now, stateDueMs))
            break;

        if (sendCommand(CMD_INIT, sizeof(CMD_INIT))) {
            mx2203State = MX2203State::WAIT_INIT;
            stateDueMs = now + COMMAND_DELAY_MS;
        } else {
            stateDueMs = now + 1000;
        }
        break;

    case MX2203State::WAIT_INIT:
        if (reached(now, stateDueMs))
            mx2203State = MX2203State::READY;
        break;

    case MX2203State::READY:
        // No periodic polling. Idle until a direct Meshtastic READ arrives.
        break;

    case MX2203State::SEND_READ:
        resetMeasurement();
        directReadActive = true;

        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64))) {
            mx2203State = MX2203State::WAIT_READ;
            stateDueMs = now + READ_TIMEOUT_MS;
        } else {
            directReadActive = false;
            if (readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "MX2203 READ failed");
                readRequestInProgress = false;
                readRequester = 0;
            }
            mx2203State = MX2203State::READY;
        }
        break;

    case MX2203State::WAIT_READ:
        if (measurementReady) {
            measurementReady = false;

            if (readRequestInProgress) {
                char reply[128] = {};
                snprintf(
                    reply,
                    sizeof(reply),
                    "MX2203\nTemp: %.2f F / %.2f C",
                    latestTemperatureF,
                    latestTemperatureC);

                sendTextReply(readRequester, readChannel, reply);
                readRequestInProgress = false;
                readRequester = 0;
            }

            LOG_INFO(
                "MX2203: READ complete raw=%lu temp=%.2f F",
                static_cast<unsigned long>(latestRaw),
                latestTemperatureF);

            mx2203State = MX2203State::READY;
            break;
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;

            if (readRequestInProgress) {
                sendTextReply(readRequester, readChannel, "MX2203 READ failed");
                readRequestInProgress = false;
                readRequester = 0;
            }

            mx2203State = MX2203State::READY;
        }
        break;

    case MX2203State::IDLE:
    default:
        break;
    }

    return 100;
}

#endif
