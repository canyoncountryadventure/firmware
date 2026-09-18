#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

#include "embedded_dfu_image.h"

// Autonomous battery-powered RAK4631 drone flasher.
//
// Mission:
//   1. A separate Meshtastic controller sends DFU to the field target.
//   2. The target reboots into AdaDFU (Nordic Legacy DFU service 0x1530).
//   3. This drone Scout detects AdaDFU over BLE.
//   4. It decompresses the embedded target application directly from internal
//      flash and streams it to the target over BLE.
//   5. The target validates, activates, and reboots.
//
// No laptop, USB cable, SD card, ESP32, LoRa stack, or phone connection is
// required on the drone Scout during the mission.

extern "C" void logLegacy(const char *level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

extern "C" void lfs_assert(const char *reason)
{
    (void)reason;
    for (;;) {
        delay(1000);
    }
}

static const uint8_t legacyDfuServiceUuid128[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00
};
static const uint8_t legacyCtrlUuid128[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x31, 0x15, 0x00, 0x00
};
static const uint8_t legacyPacketUuid128[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x32, 0x15, 0x00, 0x00
};
static const uint8_t legacyVersionUuid128[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x34, 0x15, 0x00, 0x00
};

static BLEUuid legacyDfuUuid(legacyDfuServiceUuid128);
static BLEUuid legacyCtrlUuid(legacyCtrlUuid128);
static BLEUuid legacyPacketUuid(legacyPacketUuid128);
static BLEUuid legacyVersionUuid(legacyVersionUuid128);
static BLEClientService legacyDfuService(legacyDfuUuid);
static BLEClientCharacteristic legacyCtrl(legacyCtrlUuid);
static BLEClientCharacteristic legacyPacket(legacyPacketUuid);
static BLEClientCharacteristic legacyVersion(legacyVersionUuid);

static constexpr uint8_t OP_START_DFU = 0x01;
static constexpr uint8_t OP_INIT_DFU_PARAMS = 0x02;
static constexpr uint8_t OP_RECEIVE_FW = 0x03;
static constexpr uint8_t OP_VALIDATE = 0x04;
static constexpr uint8_t OP_ACTIVATE_AND_RESET = 0x05;
static constexpr uint8_t OP_RESET = 0x06;
static constexpr uint8_t OP_PKT_RECEIPT_NOTIF_REQ = 0x08;
static constexpr uint8_t OP_RESPONSE_CODE = 0x10;
static constexpr uint8_t OP_PKT_RECEIPT_NOTIF = 0x11;
static constexpr uint8_t STATUS_SUCCESS = 0x01;

static constexpr uint16_t PRN_PACKETS = 8;
static constexpr uint16_t BLE_PAYLOAD = 20;
static constexpr int8_t MIN_DFU_RSSI_DBM = -90;
static constexpr uint32_t RETRY_DELAY_MS = 5000;

static volatile bool connecting = false;
static volatile bool legacyConnected = false;
static volatile bool legacyReady = false;
static volatile bool responseReady = false;
static volatile bool notifReady = false;

static uint16_t connectedHandle = BLE_CONN_HANDLE_INVALID;
static uint8_t responseBuf[20] = {};
static uint8_t responseLen = 0;
static uint8_t notifBuf[20] = {};
static uint8_t notifLen = 0;

static bool missionComplete = false;
static bool flashRunning = false;
static uint32_t retryAfterMs = 0;
static uint32_t lastHeartbeatMs = 0;

static void startScan();
static void scanCallback(ble_gap_evt_adv_report_t *report);
static void connectCallback(uint16_t connHandle);
static void disconnectCallback(uint16_t connHandle, uint8_t reason);
static void legacyNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len);
static bool runEmbeddedLegacyDfu();

static void led(bool on)
{
    digitalWrite(PIN_LED1, on ? HIGH : LOW);
}

static void putU32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t getU32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void printBanner()
{
    Serial.println();
    Serial.println("==============================================");
    Serial.println("RAK4631 Remote Drone Flasher - AUTONOMOUS");
    Serial.print("Embedded target: ");
    Serial.println(kEmbeddedDfuTargetVersion);
    Serial.print("Target BIN: ");
    Serial.print(kEmbeddedDfuOriginalSize);
    Serial.print(" bytes; embedded LZ4: ");
    Serial.print(kEmbeddedDfuCompressedSize);
    Serial.println(" bytes");
    Serial.println("Waiting for AdaDFU. No computer required.");
    Serial.println("==============================================");
}

void setup()
{
    pinMode(PIN_LED1, OUTPUT);
    led(false);

    // Serial is diagnostic only. Never block waiting for USB; flight operation
    // is fully autonomous from battery power.
    Serial.begin(115200);
    printBanner();

    Bluefruit.configCentralConn(247, 6, 2, 4);
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(4);
    Bluefruit.setName("RAK_DRONE_FLASHER");
    Bluefruit.setConnLedInterval(250);

    legacyDfuService.begin();
    legacyCtrl.setNotifyCallback(legacyNotifyCallback);
    legacyCtrl.begin();
    legacyPacket.begin();
    legacyVersion.begin();

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    startScan();
}

void loop()
{
    const uint32_t now = millis();

    if (missionComplete) {
        // Solid LED = successful flash completed.
        led(true);
        delay(250);
        return;
    }

    if (legacyReady && legacyConnected && !flashRunning && now >= retryAfterMs) {
        flashRunning = true;
        Bluefruit.Scanner.stop();

        Serial.println("AUTONOMOUS DFU START");
        const bool ok = runEmbeddedLegacyDfu();
        flashRunning = false;
        legacyReady = false;

        if (ok) {
            missionComplete = true;
            Serial.println("AUTONOMOUS DFU SUCCESS");
            led(true);
            return;
        }

        Serial.println("AUTONOMOUS DFU FAILED; retrying scan in 5 seconds");
        retryAfterMs = millis() + RETRY_DELAY_MS;
        led(false);

        if (legacyConnected)
            Bluefruit.disconnect(connectedHandle);
    }

    // Brief heartbeat every two seconds while waiting/scanning.
    if (!flashRunning && now - lastHeartbeatMs >= 2000) {
        lastHeartbeatMs = now;
        led(true);
        delay(40);
        led(false);
    }

    if (!connecting && !legacyConnected && now >= retryAfterMs &&
        !Bluefruit.Scanner.isRunning()) {
        startScan();
    }

    delay(10);
}

static void startScan()
{
    if (missionComplete)
        return;

    connecting = false;
    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(false);
    Bluefruit.Scanner.setInterval(160, 80);
    Bluefruit.Scanner.useActiveScan(true);
    Bluefruit.Scanner.start(0);
    Serial.println("SCANNING FOR AdaDFU...");
}

static void scanCallback(ble_gap_evt_adv_report_t *report)
{
    if (missionComplete || flashRunning || connecting || report == nullptr) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (report->rssi < MIN_DFU_RSSI_DBM) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (!Bluefruit.Scanner.checkReportForUuid(report, legacyDfuUuid)) {
        Bluefruit.Scanner.resume();
        return;
    }

    uint8_t name[32] = {};
    Bluefruit.Scanner.parseReportByType(
        report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name) - 1);

    Serial.print("AdaDFU FOUND RSSI=");
    Serial.print(report->rssi);
    Serial.print(" dBm addr=");
    Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
    if (name[0] != '\0') {
        Serial.print(" name=");
        Serial.print(reinterpret_cast<char *>(name));
    }
    Serial.println();

    connecting = true;
    Bluefruit.Scanner.stop();

    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        Serial.println("BLE CONNECT FAILED");
        Bluefruit.Scanner.resume();
    }
}

static void connectCallback(uint16_t connHandle)
{
    connecting = false;
    Serial.println("BLE CONNECTED; discovering Legacy DFU");

    if (!legacyDfuService.discover(connHandle) ||
        !legacyCtrl.discover() ||
        !legacyPacket.discover()) {
        Serial.println("DFU DISCOVERY FAILED");
        Bluefruit.disconnect(connHandle);
        return;
    }

    uint16_t version = 0;
    if (legacyVersion.discover()) {
        uint8_t verbuf[2] = {};
        if (legacyVersion.read(verbuf, sizeof(verbuf)) == 2)
            version = (uint16_t)verbuf[0] | ((uint16_t)verbuf[1] << 8);
    }

    if (!legacyCtrl.enableNotify()) {
        Serial.println("DFU NOTIFY ENABLE FAILED");
        Bluefruit.disconnect(connHandle);
        return;
    }

    connectedHandle = connHandle;
    legacyConnected = true;
    legacyReady = true;

    Serial.print("AdaDFU READY version=0x");
    Serial.println(version, HEX);
}

static void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;
    connecting = false;
    legacyConnected = false;
    legacyReady = false;
    connectedHandle = BLE_CONN_HANDLE_INVALID;

    Serial.print("BLE DISCONNECTED reason=0x");
    Serial.println(reason, HEX);
}

static void legacyNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
    (void)chr;
    if (len > sizeof(responseBuf))
        len = sizeof(responseBuf);

    if (len > 0 && data[0] == OP_RESPONSE_CODE) {
        memcpy(responseBuf, data, len);
        responseLen = (uint8_t)len;
        responseReady = true;
        return;
    }

    memcpy(notifBuf, data, len);
    notifLen = (uint8_t)len;
    notifReady = true;
}

static uint8_t waitResponse(uint8_t expectedOp, uint32_t timeoutMs)
{
    const uint32_t started = millis();
    while (!responseReady && legacyConnected && millis() - started < timeoutMs)
        delay(10);

    if (!responseReady) {
        Serial.print("DFU RESPONSE TIMEOUT op=0x");
        Serial.println(expectedOp, HEX);
        return 0xFF;
    }

    responseReady = false;
    if (responseLen < 3 ||
        responseBuf[0] != OP_RESPONSE_CODE ||
        responseBuf[1] != expectedOp) {
        Serial.println("DFU UNEXPECTED RESPONSE");
        return 0xFF;
    }

    return responseBuf[2];
}

static bool waitPrn(uint32_t expectedBytes, uint32_t timeoutMs)
{
    const uint32_t started = millis();
    while (!notifReady && legacyConnected && millis() - started < timeoutMs)
        delay(5);

    if (!notifReady) {
        Serial.println("DFU PRN TIMEOUT");
        return false;
    }

    notifReady = false;
    if (notifLen < 5 || notifBuf[0] != OP_PKT_RECEIPT_NOTIF)
        return false;

    return getU32le(notifBuf + 1) == expectedBytes;
}

static bool writePacketReliable(const uint8_t *data, uint16_t len)
{
    for (uint16_t tries = 0; tries < 400; ++tries) {
        const int written = legacyPacket.write(data, len);
        if (written == len)
            return true;
        if (!legacyConnected)
            return false;
        delay(5);
    }
    return false;
}

static void abortLegacyDfu()
{
    if (!legacyConnected)
        return;

    const uint8_t resetCmd[1] = {OP_RESET};
    legacyCtrl.write_resp(resetCmd, sizeof(resetCmd));
    delay(500);
}

// Streaming LZ4 block decoder.
//
// LZ4 match offsets are 16 bits, so a 64 KiB circular history buffer is
// sufficient. Decompressed bytes never need to be stored as a complete image;
// they are sent to the target as they are produced.
static uint8_t history[65536];
static uint8_t txBuf[BLE_PAYLOAD];
static uint8_t txLen = 0;
static uint32_t decompressedOut = 0;
static uint32_t bleSent = 0;
static uint16_t packetsSincePrn = 0;
static uint8_t lastPct = 255;

static bool flushTx()
{
    if (txLen == 0)
        return true;

    if (!writePacketReliable(txBuf, txLen))
        return false;

    bleSent += txLen;
    txLen = 0;
    packetsSincePrn++;

    if (PRN_PACKETS > 0 &&
        packetsSincePrn >= PRN_PACKETS &&
        bleSent < kEmbeddedDfuOriginalSize) {
        packetsSincePrn = 0;
        if (!waitPrn(bleSent, 10000))
            return false;
    }

    const uint8_t pct =
        (uint8_t)(((uint64_t)bleSent * 100ULL) / kEmbeddedDfuOriginalSize);
    if (pct != lastPct && (pct % 5 == 0 || pct == 100)) {
        lastPct = pct;
        Serial.print("PROGRESS ");
        Serial.print(pct);
        Serial.println("%");
    }

    return true;
}

static bool emitByte(uint8_t b)
{
    if (decompressedOut >= kEmbeddedDfuOriginalSize)
        return false;

    history[decompressedOut & 0xFFFFU] = b;
    decompressedOut++;

    txBuf[txLen++] = b;
    if (txLen == BLE_PAYLOAD)
        return flushTx();

    return true;
}

static bool readLengthExtension(uint32_t &ip, uint32_t &length)
{
    while (ip < kEmbeddedDfuCompressedSize) {
        const uint8_t v = kEmbeddedDfuLz4[ip++];
        length += v;
        if (v != 255)
            return true;
    }
    return false;
}

static bool streamEmbeddedLz4()
{
    uint32_t ip = 0;
    decompressedOut = 0;
    bleSent = 0;
    packetsSincePrn = 0;
    txLen = 0;
    lastPct = 255;

    while (ip < kEmbeddedDfuCompressedSize) {
        const uint8_t token = kEmbeddedDfuLz4[ip++];

        uint32_t literalLength = token >> 4;
        if (literalLength == 15 && !readLengthExtension(ip, literalLength))
            return false;

        if (ip + literalLength > kEmbeddedDfuCompressedSize)
            return false;

        for (uint32_t i = 0; i < literalLength; ++i) {
            if (!emitByte(kEmbeddedDfuLz4[ip++]))
                return false;
        }

        // A raw LZ4 block ends after its final literal run.
        if (ip == kEmbeddedDfuCompressedSize)
            break;

        if (ip + 2 > kEmbeddedDfuCompressedSize)
            return false;

        const uint16_t offset =
            (uint16_t)kEmbeddedDfuLz4[ip] |
            ((uint16_t)kEmbeddedDfuLz4[ip + 1] << 8);
        ip += 2;

        if (offset == 0 || offset > decompressedOut)
            return false;

        uint32_t matchLength = token & 0x0F;
        if (matchLength == 15 && !readLengthExtension(ip, matchLength))
            return false;
        matchLength += 4;

        for (uint32_t i = 0; i < matchLength; ++i) {
            const uint8_t b = history[(decompressedOut - offset) & 0xFFFFU];
            if (!emitByte(b))
                return false;
        }
    }

    if (!flushTx())
        return false;

    return decompressedOut == kEmbeddedDfuOriginalSize &&
           bleSent == kEmbeddedDfuOriginalSize;
}

static bool runEmbeddedLegacyDfu()
{
    if (!legacyReady || !legacyConnected ||
        connectedHandle == BLE_CONN_HANDLE_INVALID)
        return false;

    if (kEmbeddedDfuDatSize == 0 ||
        kEmbeddedDfuOriginalSize == 0 ||
        kEmbeddedDfuOriginalSize > 900000UL)
        return false;

    responseReady = false;
    notifReady = false;

    Serial.print("DFU BEGIN target=");
    Serial.print(kEmbeddedDfuTargetVersion);
    Serial.print(" bin=");
    Serial.println(kEmbeddedDfuOriginalSize);

    // Application-only Legacy DFU.
    const uint8_t startCmd[2] = {OP_START_DFU, 0x04};
    if (legacyCtrl.write_resp(startCmd, sizeof(startCmd)) <= 0)
        return false;

    uint8_t sizes[12] = {};
    putU32le(sizes + 8, kEmbeddedDfuOriginalSize);
    if (!writePacketReliable(sizes, sizeof(sizes))) {
        abortLegacyDfu();
        return false;
    }

    uint8_t status = waitResponse(OP_START_DFU, 120000);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t initStart[2] = {OP_INIT_DFU_PARAMS, 0x00};
    const uint8_t initDone[2] = {OP_INIT_DFU_PARAMS, 0x01};

    if (legacyCtrl.write_resp(initStart, sizeof(initStart)) <= 0) {
        abortLegacyDfu();
        return false;
    }

    for (uint16_t pos = 0; pos < kEmbeddedDfuDatSize;) {
        uint16_t n = kEmbeddedDfuDatSize - pos;
        if (n > BLE_PAYLOAD)
            n = BLE_PAYLOAD;

        if (!writePacketReliable(kEmbeddedDfuDat + pos, n)) {
            abortLegacyDfu();
            return false;
        }
        pos += n;
    }

    delay(50);
    if (legacyCtrl.write_resp(initDone, sizeof(initDone)) <= 0) {
        abortLegacyDfu();
        return false;
    }

    status = waitResponse(OP_INIT_DFU_PARAMS, 30000);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t prnCmd[3] = {
        OP_PKT_RECEIPT_NOTIF_REQ,
        (uint8_t)(PRN_PACKETS & 0xFF),
        (uint8_t)(PRN_PACKETS >> 8)
    };
    if (legacyCtrl.write_resp(prnCmd, sizeof(prnCmd)) <= 0) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t recvCmd[1] = {OP_RECEIVE_FW};
    if (legacyCtrl.write_resp(recvCmd, sizeof(recvCmd)) <= 0) {
        abortLegacyDfu();
        return false;
    }

    Serial.println("STREAMING EMBEDDED FIRMWARE...");
    if (!streamEmbeddedLz4()) {
        Serial.println("LZ4/STREAM FAILURE");
        abortLegacyDfu();
        return false;
    }

    status = waitResponse(OP_RECEIVE_FW, 60000);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t validateCmd[1] = {OP_VALIDATE};
    if (legacyCtrl.write_resp(validateCmd, sizeof(validateCmd)) <= 0) {
        abortLegacyDfu();
        return false;
    }

    status = waitResponse(OP_VALIDATE, 60000);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t activateCmd[1] = {OP_ACTIVATE_AND_RESET};
    legacyCtrl.write_resp(activateCmd, sizeof(activateCmd));

    Serial.println("VALIDATED; ACTIVATE SENT");
    const uint32_t started = millis();
    while (legacyConnected && millis() - started < 120000)
        delay(50);

    if (legacyConnected)
        return false;

    return true;
}
