#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "MX2001Diagnostic.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "main.h"

#include <bluefruit.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{

static const uint8_t HOBO_SERVICE_UUID[16] = {
    0xCF,0xCB,0xE6,0xBC,
    0xCC,0x83,
    0x49,0xAC,
    0x41,0x46,
    0x4E,0xED,
    0x4F,0x6E,
    0xE1,0x65
};

static const uint8_t HOBO_CHAR_UUID[16] = {
    0xCF,0xCB,0xE6,0xBC,
    0xCC,0x83,
    0x49,0xAC,
    0x41,0x46,
    0x4E,0xED,
    0x4F,0x6F,
    0xE1,0x65
};

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

static const uint8_t CMD_INIT[] = {
    0x01,0x01,0x04,0x05,0x1C,0x01,0x00
};

static const uint8_t CMD_READ0[] = {
    0x01,0x01,0x0A,0x0A,0x01,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x08
};

static const uint8_t CMD_READ8[] = {
    0x01,0x01,0x0A,0x0A,0x01,
    0x00,0x00,0x00,0x08,
    0x00,0x00,0x00,0x08
};

static const uint8_t CMD_STATUS[] = {
    0x01,0x01,0x08,0x04,0x05,
    0x00,0x00,0x00,0x00,0x00,0x00
};

static const uint8_t CMD_NEWREAD64[] = {
    0x01,0x01,0x08,0x04,0x04,
    0x00,0x00,0x00,0x00,0x00,0x00
};

bool initialized = false;
bool connecting = false;
bool connected = false;

uint16_t connectionHandle = BLE_CONN_HANDLE_INVALID;
uint8_t loggerMac[6] = {};
int8_t loggerBleRssi = 0;

bool statusReady = false;
uint32_t currentWritePointer = 0;
uint32_t lastWritePointer = 0;
uint32_t pendingWritePointer = 0;
uint16_t loggerIntervalSeconds = 0;

bool directReadActive = false;
bool measurementReady = false;

uint8_t fragment1[20] = {};
uint8_t fragment2[20] = {};
uint16_t fragment1Length = 0;
uint16_t fragment2Length = 0;
bool gotFragment1 = false;
bool gotFragment2 = false;

uint16_t latestTempRaw = 0;
float latestTempF = NAN;
float latestStageMeters = NAN;
float latestStageFeet = NAN;

uint16_t measurementSequence = 0;

bool onDemandReadPending = false;
bool onDemandReadInProgress = false;
uint32_t onDemandRequester = 0;
uint8_t onDemandChannel = 0;

static constexpr uint32_t COMMAND_DELAY_MS = 500;
static constexpr uint32_t STATUS_TIMEOUT_MS = 3000;
static constexpr uint32_t NEWREAD_TIMEOUT_MS = 3000;
static constexpr uint32_t POINTER_FINE_POLL_MS = 500;

uint32_t stateDueMs = 0;

enum class MX2001State : uint8_t
{
    IDLE,
    SEND_INIT,
    WAIT_INIT,
    SEND_META0,
    WAIT_META0,
    SEND_META8,
    WAIT_META8,
    SEND_BASELINE_STATUS,
    WAIT_BASELINE_STATUS,
    SEND_NEWREAD,
    WAIT_NEWREAD,
    WAIT_EXPECTED_RECORD,
    SEND_POINTER_STATUS,
    WAIT_POINTER_STATUS
};

MX2001State state = MX2001State::IDLE;

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

uint32_t nextRecordPrecheckDelayMs()
{
    if (loggerIntervalSeconds == 0)
        return 5000;

    uint32_t intervalMs =
        static_cast<uint32_t>(loggerIntervalSeconds) * 1000UL;

    if (intervalMs > 3000)
        return intervalMs - 2000;

    if (intervalMs > 1000)
        return intervalMs / 2;

    return 500;
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

bool isMX2001(const ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr || report->data.p_data == nullptr)
        return false;

    const uint8_t *data = report->data.p_data;
    uint16_t len = report->data.len;
    uint16_t pos = 0;

    while (pos < len) {
        uint8_t fieldLength = data[pos];

        if (fieldLength == 0)
            break;

        if (pos + fieldLength >= len)
            break;

        uint8_t type = data[pos + 1];

        if (type == 0xFF) {
            uint16_t manufacturerLength =
                static_cast<uint16_t>(fieldLength - 1);

            if (manufacturerLength == 22) {
                const uint8_t *mfg = &data[pos + 2];

                if (mfg[0] == 0xC5 && mfg[1] == 0x00)
                    return true;
            }
        }

        pos += fieldLength + 1;
    }

    return false;
}

void decodeDirectMeasurement()
{
    if (!gotFragment1 || !gotFragment2)
        return;

    if (fragment1Length < 19) {
        LOG_WARN("MX2001: NEWREAD fragment 1 too short");
        directReadActive = false;
        return;
    }

    latestTempRaw =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(fragment1[17]) << 8) |
            static_cast<uint16_t>(fragment1[18]));

    latestTempF =
        -0.1805f * static_cast<float>(latestTempRaw) + 169.64f;

    if (fragment2Length < 7) {
        LOG_WARN("MX2001: NEWREAD fragment 2 too short");
        directReadActive = false;
        return;
    }

    latestStageMeters = readBEFloat(&fragment2[3]);
    latestStageFeet = latestStageMeters * 3.280839895f;

    if (!isfinite(latestTempF) || !isfinite(latestStageFeet)) {
        LOG_WARN("MX2001: invalid direct measurement");
        directReadActive = false;
        return;
    }

    if (latestTempF < -50.0f || latestTempF > 180.0f) {
        LOG_WARN("MX2001: implausible temperature %.2f F", latestTempF);
        directReadActive = false;
        return;
    }

    if (latestStageFeet < -100.0f || latestStageFeet > 1000.0f) {
        LOG_WARN("MX2001: implausible WL %.3f ft", latestStageFeet);
        directReadActive = false;
        return;
    }

    measurementReady = true;
    directReadActive = false;

    LOG_INFO("========================================");
    LOG_INFO("MX2001 MEASUREMENT READY");
    LOG_INFO("WL: %.3f ft", latestStageFeet);
    LOG_INFO("WL: %.5f m", latestStageMeters);
    LOG_INFO("Temp: %.2f F", latestTempF);
    LOG_INFO("Temp raw: %u", latestTempRaw);
    LOG_INFO("Logger interval: %u sec", loggerIntervalSeconds);
    LOG_INFO(
        "Write pointer: 0x%08lX",
        static_cast<unsigned long>(pendingWritePointer));
    LOG_INFO("========================================");
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
        data[0] == 0x01 &&
        data[1] == 0x02 &&
        data[2] == 0x04 &&
        data[3] == 0x05) {

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
            "MX2001 STATUS pointer=0x%08lX interval=%u",
            static_cast<unsigned long>(currentWritePointer),
            loggerIntervalSeconds);

        return;
    }

    if (!directReadActive)
        return;

    if (onDemandReadInProgress) {
        char raw[192] = {};
        size_t used = 0;

        for (uint16_t i = 0; i < len && used + 4 < sizeof(raw); ++i) {
            int written = snprintf(
                &raw[used],
                sizeof(raw) - used,
                "%02X%s",
                data[i],
                (i + 1 < len) ? " " : "");

            if (written <= 0)
                break;

            used += static_cast<size_t>(written);
        }

        LOG_INFO(
            "MX2001 RAW NEWREAD len=%u: %s",
            len,
            raw);
    }

    if (len >= 20 &&
        data[0] == 0x01 &&
        data[1] == 0x02 &&
        data[2] == 0x04 &&
        data[3] == 0x04) {

        fragment1Length =
            (len > sizeof(fragment1)) ? sizeof(fragment1) : len;

        memcpy(fragment1, data, fragment1Length);
        gotFragment1 = true;
    } else if (len >= 7 && data[0] == 0x02) {
        fragment2Length =
            (len > sizeof(fragment2)) ? sizeof(fragment2) : len;

        memcpy(fragment2, data, fragment2Length);
        gotFragment2 = true;
    }

    if (gotFragment1 && gotFragment2)
        decodeDirectMeasurement();
}

bool sendCommand(
    const uint8_t *command,
    uint16_t length,
    const char *name)
{
    if (!connected)
        return false;

    uint16_t written = hoboCharacteristic.write(command, length);

    LOG_DEBUG(
        "MX2001 TX %s requested=%u written=%u",
        name,
        length,
        written);

    return written == length;
}

void scanCallback(ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr)
        return;

    if (connecting || connected) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (!isMX2001(report)) {
        Bluefruit.Scanner.resume();
        return;
    }

    makeHumanMac(report->peer_addr.addr, loggerMac);
    loggerBleRssi = report->rssi;

    LOG_INFO("========================================");
    LOG_INFO("MX2001 AUTO-DETECTED");
    logMac("Logger:", loggerMac);
    LOG_INFO("BLE RSSI: %d dBm", loggerBleRssi);
    LOG_INFO("Connecting...");
    LOG_INFO("========================================");

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        LOG_WARN("MX2001 connection request failed");
        Bluefruit.Scanner.start(0);
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connected = true;
    connectionHandle = connHandle;

    LOG_INFO("MX2001 BLE CONNECTED");

    if (!hoboService.discover(connHandle)) {
        LOG_WARN("MX2001 service discovery failed");
        Bluefruit.disconnect(connHandle);
        return;
    }

    if (!hoboCharacteristic.discover()) {
        LOG_WARN("MX2001 characteristic discovery failed");
        Bluefruit.disconnect(connHandle);
        return;
    }

    if (!hoboCharacteristic.enableNotify()) {
        LOG_WARN("MX2001 notification enable failed");
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2001 command channel READY");

    lastWritePointer = 0;
    currentWritePointer = 0;
    pendingWritePointer = 0;
    statusReady = false;
    measurementReady = false;
    directReadActive = false;

    state = MX2001State::SEND_INIT;
    stateDueMs = millis() + 500;
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("MX2001 disconnected reason=0x%02X", reason);

    connected = false;
    connecting = false;
    connectionHandle = BLE_CONN_HANDLE_INVALID;
    statusReady = false;
    directReadActive = false;
    measurementReady = false;
    onDemandReadPending = false;
    onDemandReadInProgress = false;
    onDemandRequester = 0;
    state = MX2001State::IDLE;
}

void initializeClient()
{
    LOG_INFO("========================================");
    LOG_INFO("MX2001 PRODUCTION SENSOR SENDER");
    LOG_INFO("Reads: WL + temperature");
    LOG_INFO("Uses logger's actual configured interval");
    LOG_INFO("Mesh port: PRIVATE_APP 256");
    LOG_INFO("DM command: READ");
    LOG_INFO("One packet per new logger record");
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
        LOG_WARN("MX2001 scanner failed to start");
}

} // namespace

MX2001DiagnosticModule::MX2001DiagnosticModule()
    : SinglePortModule(
          "MX2001",
          meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread(
          "MX2001")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool MX2001DiagnosticModule::wantPacket(
    const meshtastic_MeshPacket *p)
{
    if (p == nullptr)
        return false;

    return
        p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP ||
        p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage MX2001DiagnosticModule::handleReceived(
    const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    if (nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();

    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    if (!isReadCommand(
            mp.decoded.payload.bytes,
            mp.decoded.payload.size))
        return ProcessMessage::CONTINUE;

    LOG_INFO(
        "MX2001 READ DM received from !%08lx",
        static_cast<unsigned long>(mp.from));

    if (!connected) {
        sendTextReply(
            mp.from,
            mp.channel,
            "MX2001 unavailable");
        return ProcessMessage::CONTINUE;
    }

    if (onDemandReadPending || onDemandReadInProgress) {
        sendTextReply(
            mp.from,
            mp.channel,
            "MX2001 read already in progress");
        return ProcessMessage::CONTINUE;
    }

    onDemandRequester = mp.from;
    onDemandChannel = mp.channel;
    onDemandReadPending = true;

    setIntervalFromNow(10);

    return ProcessMessage::CONTINUE;
}

bool MX2001DiagnosticModule::sendTextReply(
    uint32_t destination,
    uint8_t channel,
    const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();

    if (packet == nullptr) {
        LOG_WARN("MX2001: text packet allocation failed");
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

bool MX2001DiagnosticModule::sendMeasurementPacket(
    float stageFeet,
    float temperatureF,
    uint16_t temperatureRaw)
{
    meshtastic_MeshPacket *packet = allocDataPacket();

    if (packet == nullptr) {
        LOG_WARN("MX2001: mesh packet allocation failed");
        return false;
    }

    int32_t stageScaled =
        static_cast<int32_t>(lroundf(stageFeet * 10.0f));

    int32_t tempScaled =
        static_cast<int32_t>(lroundf(temperatureF * 10.0f));

    if (stageScaled < -32768)
        stageScaled = -32768;
    if (stageScaled > 32767)
        stageScaled = 32767;
    if (tempScaled < -32768)
        tempScaled = -32768;
    if (tempScaled > 32767)
        tempScaled = 32767;

    int16_t stageTenths = static_cast<int16_t>(stageScaled);
    int16_t tempTenths = static_cast<int16_t>(tempScaled);
    uint16_t stageBits = static_cast<uint16_t>(stageTenths);
    uint16_t tempBits = static_cast<uint16_t>(tempTenths);

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

    payload[10] = static_cast<uint8_t>(temperatureRaw & 0xFF);
    payload[11] = static_cast<uint8_t>((temperatureRaw >> 8) & 0xFF);

    memcpy(&payload[12], loggerMac, 6);
    payload[18] = static_cast<uint8_t>(loggerBleRssi);

    packet->decoded.payload.size = 19;
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    LOG_INFO("========================================");
    LOG_INFO("MX2001 MESH TX");
    LOG_INFO("Sequence: %u", measurementSequence);
    LOG_INFO("WL: %.1f ft", stageTenths / 10.0f);
    LOG_INFO("Temp: %.1f F", tempTenths / 10.0f);
    logMac("Logger:", loggerMac);
    LOG_INFO("Payload: 19 bytes / PRIVATE_APP");
    LOG_INFO("========================================");

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

int32_t MX2001DiagnosticModule::runOnce()
{
    uint32_t now = millis();

    if (millis() < 15000)
        return 500;

    if (!initialized) {
        initializeClient();
        return 500;
    }

    if (!connected) {
        if (connecting)
            return 500;

        if (!Bluefruit.Scanner.isRunning()) {
            LOG_INFO("MX2001: scanning for logger");
            Bluefruit.Scanner.start(0);
        }

        return 500;
    }

    if (onDemandReadPending &&
        state == MX2001State::WAIT_EXPECTED_RECORD) {

        onDemandReadPending = false;
        onDemandReadInProgress = true;
        pendingWritePointer = currentWritePointer;

        LOG_INFO("========================================");
        LOG_INFO("MX2001 ON-DEMAND READ");
        LOG_INFO("Requesting level + temperature");
        LOG_INFO("========================================");

        state = MX2001State::SEND_NEWREAD;
        stateDueMs = now;
    }

    switch (state) {
    case MX2001State::SEND_INIT:
        if (!reached(now, stateDueMs))
            break;

        if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
            state = MX2001State::WAIT_INIT;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;

    case MX2001State::WAIT_INIT:
        if (reached(now, stateDueMs))
            state = MX2001State::SEND_META0;
        break;

    case MX2001State::SEND_META0:
        if (sendCommand(CMD_READ0, sizeof(CMD_READ0), "META0")) {
            state = MX2001State::WAIT_META0;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;

    case MX2001State::WAIT_META0:
        if (reached(now, stateDueMs))
            state = MX2001State::SEND_META8;
        break;

    case MX2001State::SEND_META8:
        if (sendCommand(CMD_READ8, sizeof(CMD_READ8), "META8")) {
            state = MX2001State::WAIT_META8;
            stateDueMs = now + COMMAND_DELAY_MS;
        }
        break;

    case MX2001State::WAIT_META8:
        if (reached(now, stateDueMs))
            state = MX2001State::SEND_BASELINE_STATUS;
        break;

    case MX2001State::SEND_BASELINE_STATUS:
        statusReady = false;

        if (sendCommand(
                CMD_STATUS,
                sizeof(CMD_STATUS),
                "BASELINE STATUS")) {
            state = MX2001State::WAIT_BASELINE_STATUS;
            stateDueMs = now + STATUS_TIMEOUT_MS;
        }
        break;

    case MX2001State::WAIT_BASELINE_STATUS:
        if (statusReady) {
            statusReady = false;
            pendingWritePointer = currentWritePointer;

            LOG_INFO("========================================");
            LOG_INFO("MX2001 LOGGER READY");
            LOG_INFO("Interval: %u seconds", loggerIntervalSeconds);
            LOG_INFO(
                "Pointer: 0x%08lX",
                static_cast<unsigned long>(currentWritePointer));
            LOG_INFO("Taking startup Temp + WL reading");
            LOG_INFO("========================================");

            state = MX2001State::SEND_NEWREAD;
            break;
        }

        if (reached(now, stateDueMs)) {
            LOG_WARN("MX2001 baseline status timeout");
            state = MX2001State::SEND_BASELINE_STATUS;
        }
        break;

    case MX2001State::SEND_NEWREAD:
        gotFragment1 = false;
        gotFragment2 = false;
        fragment1Length = 0;
        fragment2Length = 0;
        measurementReady = false;
        directReadActive = true;

        if (sendCommand(
                CMD_NEWREAD64,
                sizeof(CMD_NEWREAD64),
                "NEWREAD64")) {
            state = MX2001State::WAIT_NEWREAD;
            stateDueMs = now + NEWREAD_TIMEOUT_MS;
        } else {
            directReadActive = false;
        }
        break;

    case MX2001State::WAIT_NEWREAD:
        if (measurementReady) {
            measurementReady = false;

            if (onDemandReadInProgress) {
                char reply[96];
                snprintf(
                    reply,
                    sizeof(reply),
                    "Level: %.2f ft\nTemp: %.1f F",
                    latestStageFeet,
                    latestTempF);

                sendTextReply(
                    onDemandRequester,
                    onDemandChannel,
                    reply);

                LOG_INFO("MX2001 READ DM reply sent");

                onDemandReadInProgress = false;
                onDemandRequester = 0;

                state = MX2001State::WAIT_EXPECTED_RECORD;
                stateDueMs = now + POINTER_FINE_POLL_MS;
                break;
            }

            if (sendMeasurementPacket(
                    latestStageFeet,
                    latestTempF,
                    latestTempRaw)) {
                lastWritePointer = pendingWritePointer;

                LOG_INFO(
                    "MX2001 record consumed at pointer 0x%08lX",
                    static_cast<unsigned long>(lastWritePointer));
            }

            state = MX2001State::WAIT_EXPECTED_RECORD;
            stateDueMs = now + nextRecordPrecheckDelayMs();
            break;
        }

        if (reached(now, stateDueMs)) {
            directReadActive = false;
            LOG_WARN("MX2001 NEWREAD timeout");

            if (onDemandReadInProgress) {
                sendTextReply(
                    onDemandRequester,
                    onDemandChannel,
                    "MX2001 read failed");

                onDemandReadInProgress = false;
                onDemandRequester = 0;
                state = MX2001State::WAIT_EXPECTED_RECORD;
                stateDueMs = now + POINTER_FINE_POLL_MS;
            } else {
                state = MX2001State::SEND_NEWREAD;
                stateDueMs = now + 1000;
            }
        }
        break;

    case MX2001State::WAIT_EXPECTED_RECORD:
        if (reached(now, stateDueMs))
            state = MX2001State::SEND_POINTER_STATUS;
        break;

    case MX2001State::SEND_POINTER_STATUS:
        statusReady = false;

        if (sendCommand(
                CMD_STATUS,
                sizeof(CMD_STATUS),
                "POINTER STATUS")) {
            state = MX2001State::WAIT_POINTER_STATUS;
            stateDueMs = now + STATUS_TIMEOUT_MS;
        }
        break;

    case MX2001State::WAIT_POINTER_STATUS:
        if (statusReady) {
            statusReady = false;

            if (currentWritePointer != lastWritePointer) {
                LOG_INFO("========================================");
                LOG_INFO("MX2001 NEW LOGGER RECORD");
                LOG_INFO(
                    "Old pointer: 0x%08lX",
                    static_cast<unsigned long>(lastWritePointer));
                LOG_INFO(
                    "New pointer: 0x%08lX",
                    static_cast<unsigned long>(currentWritePointer));
                LOG_INFO(
                    "Delta: %ld bytes",
                    static_cast<long>(
                        currentWritePointer - lastWritePointer));
                LOG_INFO("Reading Temp + WL");
                LOG_INFO("========================================");

                pendingWritePointer = currentWritePointer;
                state = MX2001State::SEND_NEWREAD;
            } else {
                state = MX2001State::WAIT_EXPECTED_RECORD;
                stateDueMs = now + POINTER_FINE_POLL_MS;
            }

            break;
        }

        if (reached(now, stateDueMs)) {
            LOG_WARN("MX2001 pointer status timeout");
            state = MX2001State::SEND_POINTER_STATUS;
            stateDueMs = now + 1000;
        }
        break;

    default:
        break;
    }

    return 100;
}

#endif