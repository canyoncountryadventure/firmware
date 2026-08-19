#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "MX2203DecodeTest.h"

#include <bluefruit.h>
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

static const uint8_t CMD_INIT[] = {
    0x01, 0x01, 0x04, 0x05, 0x1C, 0x01, 0x00
};

static const uint8_t CMD_NEWREAD64[] = {
    0x01, 0x01, 0x08, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Candidate conversion derived from two hardware calibration points:
// raw 5626 = 58.15 F (exact HOBOconnect observation)
// raw 7547 = ~92.5 F (midpoint of 92-93 F stable hot-bath observation)
// Independent check: raw 6045 predicts 65.64 F, matching the ~65 F cold bath.
static constexpr float MX2203_RAW_TO_F_SLOPE = 0.0178813118f;
static constexpr float MX2203_RAW_TO_F_INTERCEPT = -42.4502603f;

static constexpr uint32_t SERVICE_SETTLE_MS = 500;
static constexpr uint32_t SERVICE_RETRY_DELAY_MS = 350;
static constexpr uint8_t SERVICE_DISCOVERY_ATTEMPTS = 3;
static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t READ_TIMEOUT_MS = 3000;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

enum class State : uint8_t
{
    IDLE = 0,
    SEND_INIT,
    WAIT_INIT,
    SEND_READ,
    WAIT_READ,
    READY
};

bool initialized = false;
bool connecting = false;
bool connected = false;
State state = State::IDLE;
uint32_t dueMs = 0;
bool readActive = false;
bool readReady = false;

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

        // Hardware-observed MX2203 Onset manufacturer signature:
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

bool sendCommand(const uint8_t *command, uint16_t length, const char *name)
{
    if (!connected)
        return false;

    const uint16_t written = hoboCharacteristic.write(command, length);
    LOG_INFO("MX2203 TEST TX %s requested=%u written=%u", name, length, written);
    return written == length;
}

void notifyCallback(BLEClientCharacteristic *characteristic, uint8_t *data, uint16_t len)
{
    (void)characteristic;

    if (!readActive || len < 12)
        return;

    // Hardware-proven MX2203 live response:
    // 01 01 0B 04 04 00 04 04 [RAW32 BE] ...
    if (data[0] != 0x01 || data[1] != 0x01 || data[2] != 0x0B ||
        data[3] != 0x04 || data[4] != 0x04 || data[5] != 0x00 ||
        data[6] != 0x04 || data[7] != 0x04) {
        return;
    }

    const uint32_t raw = readBE32(&data[8]);
    const float tempF = MX2203_RAW_TO_F_SLOPE * static_cast<float>(raw) + MX2203_RAW_TO_F_INTERCEPT;
    const float tempC = (tempF - 32.0f) * (5.0f / 9.0f);

    readActive = false;
    readReady = true;

    LOG_INFO("========================================");
    LOG_INFO("MX2203 CANDIDATE DECODE");
    LOG_INFO("Raw: %lu (0x%08lX)", static_cast<unsigned long>(raw), static_cast<unsigned long>(raw));
    LOG_INFO("Temp: %.2f F / %.2f C", tempF, tempC);
    LOG_INFO("Calibration: candidate linear equation; verify against HOBOconnect");
    LOG_INFO("========================================");
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

    uint8_t mac[6] = {};
    for (uint8_t i = 0; i < 6; ++i)
        mac[i] = report->peer_addr.addr[5 - i];

    LOG_INFO("========================================");
    LOG_INFO("MX2203 TEST: candidate found");
    LOG_INFO(
        "MAC: %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d dBm",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], report->rssi);
    LOG_INFO("========================================");

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        LOG_WARN("MX2203 TEST: connection request failed");
        Bluefruit.Scanner.start(0);
    }
}

void connectCallback(uint16_t connHandle)
{
    connecting = false;
    connected = true;

    LOG_INFO("MX2203 TEST: BLE connected; settling");
    delay(SERVICE_SETTLE_MS);

    bool serviceFound = false;
    for (uint8_t attempt = 1; attempt <= SERVICE_DISCOVERY_ATTEMPTS; ++attempt) {
        LOG_INFO("MX2203 TEST: service discovery attempt %u/%u", attempt, SERVICE_DISCOVERY_ATTEMPTS);
        if (hoboService.discover(connHandle)) {
            serviceFound = true;
            break;
        }
        if (attempt < SERVICE_DISCOVERY_ATTEMPTS)
            delay(SERVICE_RETRY_DELAY_MS);
    }

    if (!serviceFound) {
        LOG_WARN("MX2203 TEST: common HOBO service not found");
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

    if (!characteristicFound || !hoboCharacteristic.enableNotify()) {
        LOG_WARN("MX2203 TEST: command characteristic/notify setup failed");
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2203 TEST: command channel ready");
    readActive = false;
    readReady = false;
    state = State::SEND_INIT;
    dueMs = millis() + 500;
}

void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    LOG_WARN("MX2203 TEST: disconnected reason=0x%02X", reason);
    connected = false;
    connecting = false;
    readActive = false;
    readReady = false;
    state = State::IDLE;
}

void initializeClient()
{
    LOG_INFO("========================================");
    LOG_INFO("MX2203 CANDIDATE DECODER TEST");
    LOG_INFO("Targets Onset model discriminator 0x03 only");
    LOG_INFO("Reads MX2203 every 5 seconds");
    LOG_INFO("Candidate equation: F = 0.0178813118 * raw - 42.4502603");
    LOG_INFO("Verify decoded temperature against HOBOconnect");
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
        LOG_WARN("MX2203 TEST: scanner failed to start");
}

} // namespace

MX2203DecodeTestModule::MX2203DecodeTestModule()
    : concurrency::OSThread("MX2203DecodeTest")
{
    setIntervalFromNow(500);
}

int32_t MX2203DecodeTestModule::runOnce()
{
    const uint32_t now = millis();

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
            LOG_INFO("MX2203 TEST: scanning for model byte 0x03");
            Bluefruit.Scanner.start(0);
        }
        return 500;
    }

    switch (state) {
    case State::SEND_INIT:
        if (!reached(now, dueMs))
            break;
        if (sendCommand(CMD_INIT, sizeof(CMD_INIT), "INIT")) {
            state = State::WAIT_INIT;
            dueMs = now + COMMAND_DELAY_MS;
        } else {
            dueMs = now + 1000;
        }
        break;

    case State::WAIT_INIT:
        if (reached(now, dueMs))
            state = State::SEND_READ;
        break;

    case State::SEND_READ:
        readReady = false;
        readActive = true;
        if (sendCommand(CMD_NEWREAD64, sizeof(CMD_NEWREAD64), "NEWREAD64")) {
            state = State::WAIT_READ;
            dueMs = now + READ_TIMEOUT_MS;
        } else {
            readActive = false;
            dueMs = now + 1000;
        }
        break;

    case State::WAIT_READ:
        if (readReady) {
            readReady = false;
            state = State::READY;
            dueMs = now + SAMPLE_INTERVAL_MS;
        } else if (reached(now, dueMs)) {
            readActive = false;
            LOG_WARN("MX2203 TEST: NEWREAD64 timeout");
            state = State::READY;
            dueMs = now + SAMPLE_INTERVAL_MS;
        }
        break;

    case State::READY:
        if (reached(now, dueMs))
            state = State::SEND_READ;
        break;

    default:
        break;
    }

    return 100;
}

#endif
