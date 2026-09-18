#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

// The Meshtastic nRF52 build globally overrides LittleFS logging/assert hooks.
// This standalone scout does not use the Meshtastic logging layer, so provide
// tiny local implementations to satisfy the bundled Adafruit libraries.
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

// Phase 1 bench-test build for the drone flasher proof of concept.
//
// This firmware turns a spare RAK4631 into a BLE Central-only DFU scout.
// It does NOT transmit a firmware image yet. It proves that a nearby RAK4631
// can detect a target node after that target has been told (locally or by
// Meshtastic remote-admin) to reboot into Nordic Secure DFU mode.
//
// Support both Nordic Secure DFU and the Adafruit bootloader's Nordic
// legacy DFU service. The RAK4631 target used here advertises as "AdaDFU"
// with legacy service 00001530-1212-EFDE-1523-785FEABCD123.
static BLEClientService secureDfuService(0xFE59);

static const uint8_t legacyDfuServiceUuid128[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00
};
static BLEUuid legacyDfuUuid(legacyDfuServiceUuid128);
static BLEClientService legacyDfuService(legacyDfuUuid);

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

static BLEUuid legacyCtrlUuid(legacyCtrlUuid128);
static BLEUuid legacyPacketUuid(legacyPacketUuid128);
static BLEUuid legacyVersionUuid(legacyVersionUuid128);
static BLEClientCharacteristic legacyCtrl(legacyCtrlUuid);
static BLEClientCharacteristic legacyPacket(legacyPacketUuid);
static BLEClientCharacteristic legacyVersion(legacyVersionUuid);

enum class DfuKind : uint8_t {
    None = 0,
    SecureFe59,
    Legacy1530,
};

static volatile bool connecting = false;
static DfuKind connectingKind = DfuKind::None;
static uint32_t lastHeartbeatMs = 0;
static uint32_t heartbeatCount = 0;
static uint32_t advCount = 0;

static uint16_t connectedHandle = BLE_CONN_HANDLE_INVALID;
static volatile bool legacyReady = false;
static volatile bool legacyConnected = false;
static volatile bool responseReady = false;
static uint8_t responseBuf[20] = {0};
static uint8_t responseLen = 0;
static volatile bool notifReady = false;
static uint8_t notifBuf[20] = {0};
static uint8_t notifLen = 0;

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
static constexpr uint16_t SERIAL_BLOCK = 1024;

static void startScan();
static void scanCallback(ble_gap_evt_adv_report_t *report);
static void connectCallback(uint16_t connHandle);
static void disconnectCallback(uint16_t connHandle, uint8_t reason);
static void legacyNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len);
static bool runLegacyDfuFromSerial(uint32_t datSize, uint32_t binSize);
static void pollSerialCommand();

void setup()
{
    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, LOW);

    Serial.begin(115200);

    // Do not block forever when this eventually flies without a USB cable.
    const uint32_t serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart < 3000)) {
        delay(10);
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("RAK4631 Nordic DFU Scout - Phase 1");
    Serial.println("Looking for AdaDFU legacy 0x1530 or Secure DFU 0xFE59");
    Serial.println("========================================");
    Serial.flush();

    // RAK is the BLE Central only: 0 peripheral links, 1 central link.
    // Keep generous ATT buffers available for future high-MTU optimization,
    // but Phase 2 intentionally uses 20-byte writes for maximum compatibility.
    Bluefruit.configCentralConn(247, 6, 2, 4);
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(4);
    Bluefruit.setName("RAK_DFU_SCOUT");
    Bluefruit.setConnLedInterval(250);

    // Register both client-side DFU services so we can perform real GATT
    // discovery after connecting, rather than trusting advertisements.
    secureDfuService.begin();
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
    // Keep an obvious runtime heartbeat so bench testing never depends on
    // catching the one-time startup banner.
    const uint32_t now = millis();
    if (now - lastHeartbeatMs >= 2000) {
        lastHeartbeatMs = now;
        heartbeatCount++;

        Serial.print("SCOUT ALIVE #");
        Serial.print(heartbeatCount);
        Serial.print("  uptime=");
        Serial.print(now / 1000);
        Serial.println("s  scanning for AdaDFU/0x1530 + 0xFE59");
        Serial.flush();

        digitalWrite(PIN_LED1, HIGH);
        delay(80);
        digitalWrite(PIN_LED1, LOW);
    }

    pollSerialCommand();
    delay(20);
}

static void startScan()
{
    connecting = false;
    connectingKind = DfuKind::None;

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(true);
    Bluefruit.Scanner.setInterval(160, 80); // 100 ms interval, 50 ms window
    Bluefruit.Scanner.useActiveScan(true);
    Bluefruit.Scanner.start(0); // scan indefinitely

    Serial.println("SCANNING: raw BLE diagnostics enabled (RSSI >= -75 dBm)");
    Serial.println("Auto-connect: AdaDFU legacy 0x1530 and Nordic Secure DFU 0xFE59.");
}

static void scanCallback(ble_gap_evt_adv_report_t *report)
{
    // Diagnostic mode: print all reasonably nearby BLE advertisements so we
    // can identify what a target actually emits in normal vs bootloader mode.
    // At ~5 ft a target should normally be far stronger than -75 dBm.
    if (report->rssi >= -75) {
        advCount++;

        uint8_t name[32] = {0};
        Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name) - 1);
        if (name[0] == '\0') {
            Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, name, sizeof(name) - 1);
        }

        Serial.println();
        Serial.print("ADV #");
        Serial.print(advCount);
        Serial.print("  RSSI=");
        Serial.print(report->rssi);
        Serial.print(" dBm  addr=");
        Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
        if (name[0] != '\0') {
            Serial.print("  name=");
            Serial.print(reinterpret_cast<char *>(name));
        }
        Serial.println();

        Serial.print("  RAW:");
        for (uint16_t i = 0; i < report->data.len; i++) {
            Serial.print(' ');
            if (report->data.p_data[i] < 0x10) {
                Serial.print('0');
            }
            Serial.print(report->data.p_data[i], HEX);
        }
        Serial.println();
        Serial.flush();
    }

    const bool hasSecureDfu = Bluefruit.Scanner.checkReportForService(report, secureDfuService);
    const bool hasLegacyDfu = Bluefruit.Scanner.checkReportForUuid(report, legacyDfuUuid);

    // SoftDevice pauses scanning while this callback runs. Resume it whenever
    // we decide not to connect.
    if (!hasSecureDfu && !hasLegacyDfu) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (connecting) {
        Bluefruit.Scanner.resume();
        return;
    }

    uint8_t name[32] = {0};
    Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name) - 1);

    connectingKind = hasLegacyDfu ? DfuKind::Legacy1530 : DfuKind::SecureFe59;

    Serial.println();
    if (connectingKind == DfuKind::Legacy1530) {
        Serial.println("*** ADAFRUIT / NORDIC LEGACY DFU 0x1530 FOUND ***");
    } else {
        Serial.println("*** NORDIC SECURE DFU 0xFE59 FOUND ***");
    }
    Serial.print("  address: ");
    Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
    Serial.println();
    Serial.print("  RSSI: ");
    Serial.print(report->rssi);
    Serial.println(" dBm");
    if (name[0] != '\0') {
        Serial.print("  name: ");
        Serial.println(reinterpret_cast<char *>(name));
    }
    Serial.print("  protocol: ");
    Serial.println(connectingKind == DfuKind::Legacy1530
                       ? "Legacy DFU 00001530-1212-EFDE-1523-785FEABCD123"
                       : "Secure DFU 0xFE59");
    Serial.println("CONNECTING...");

    connecting = true;
    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        connectingKind = DfuKind::None;
        Serial.println("CONNECT FAILED; resuming scan");
        Bluefruit.Scanner.resume();
    }
}

static void connectCallback(uint16_t connHandle)
{
    connecting = false;
    Serial.println("BLE CONNECTED");

    if (connectingKind == DfuKind::Legacy1530) {
        Serial.println("DISCOVERING AdaDFU legacy 0x1530 service over GATT...");

        if (!legacyDfuService.discover(connHandle)) {
            Serial.println("PHASE 1 FAIL: connected, but legacy DFU 0x1530 service was not discoverable");
            Bluefruit.disconnect(connHandle);
            return;
        }

        const bool ctrlOk = legacyCtrl.discover();
        const bool packetOk = legacyPacket.discover();
        const bool versionOk = legacyVersion.discover();

        Serial.print("Legacy chars: ctrl=");
        Serial.print(ctrlOk ? "YES" : "NO");
        Serial.print(" packet=");
        Serial.print(packetOk ? "YES" : "NO");
        Serial.print(" version=");
        Serial.println(versionOk ? "YES" : "NO");

        if (!ctrlOk || !packetOk) {
            Serial.println("PHASE 1 FAIL: required legacy DFU characteristics missing");
            Bluefruit.disconnect(connHandle);
            return;
        }

        uint16_t version = 0;
        if (versionOk) {
            uint8_t verbuf[2] = {0, 0};
            const int n = legacyVersion.read(verbuf, sizeof(verbuf));
            if (n == 2) {
                version = (uint16_t)verbuf[0] | ((uint16_t)verbuf[1] << 8);
            }
        }

        if (!legacyCtrl.enableNotify()) {
            Serial.println("PHASE 1 FAIL: could not enable Control Point notifications");
            Bluefruit.disconnect(connHandle);
            return;
        }

        connectedHandle = connHandle;
        legacyConnected = true;
        legacyReady = true;

        Serial.println("PHASE 1 PASS: AdaDFU legacy service 0x1530 discovered");
        Serial.print("DFU version raw=0x");
        Serial.println(version, HEX);
        Serial.println("Target is in BLE OTA bootloader and reachable from this RAK.");
        Serial.println("PHASE 2 READY: run tools/rak_dfu_serial_upload.py against this COM port.");
    } else {
        Serial.println("DISCOVERING Secure DFU service 0xFE59 over GATT...");

        if (!secureDfuService.discover(connHandle)) {
            Serial.println("PHASE 1 FAIL: connected, but Secure DFU service was not discoverable");
            Bluefruit.disconnect(connHandle);
            return;
        }

        Serial.println("PHASE 1 PASS: Secure DFU service 0xFE59 discovered");
        Serial.println("Target is in the correct bootloader and reachable from this RAK.");
    }

    Serial.println("No firmware bytes will be written in this build.");
    Serial.println("Leave connected for inspection; power-cycle/reset the target when finished.");
}

static void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;
    connecting = false;
    connectingKind = DfuKind::None;
    legacyReady = false;
    legacyConnected = false;
    connectedHandle = BLE_CONN_HANDLE_INVALID;

    Serial.print("DISCONNECTED, reason 0x");
    Serial.println(reason, HEX);
    Serial.println("Resuming DFU scan...");
}

static void legacyNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
    (void)chr;
    if (len > sizeof(responseBuf)) {
        len = sizeof(responseBuf);
    }

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

static uint8_t waitResponse(uint8_t expectedOp, uint32_t timeoutMs)
{
    const uint32_t started = millis();
    while (!responseReady && legacyConnected && (millis() - started < timeoutMs)) {
        delay(10);
    }

    if (!responseReady) {
        Serial.print("DFU ERROR: response timeout op=0x");
        Serial.println(expectedOp, HEX);
        return 0xFF;
    }

    responseReady = false;
    if (responseLen < 3 ||
        responseBuf[0] != OP_RESPONSE_CODE ||
        responseBuf[1] != expectedOp) {
        Serial.print("DFU ERROR: unexpected response for op=0x");
        Serial.print(expectedOp, HEX);
        Serial.print(" got:");
        for (uint8_t i = 0; i < responseLen; i++) {
            Serial.print(' ');
            if (responseBuf[i] < 0x10) Serial.print('0');
            Serial.print(responseBuf[i], HEX);
        }
        Serial.println();
        return 0xFF;
    }

    return responseBuf[2];
}

static bool waitPrn(uint32_t expectedBytes, uint32_t timeoutMs)
{
    const uint32_t started = millis();
    while (!notifReady && legacyConnected && (millis() - started < timeoutMs)) {
        delay(5);
    }

    if (!notifReady) {
        Serial.print("DFU ERROR: PRN timeout at ");
        Serial.println(expectedBytes);
        return false;
    }

    notifReady = false;
    if (notifLen < 5 || notifBuf[0] != OP_PKT_RECEIPT_NOTIF) {
        Serial.print("DFU ERROR: unexpected notification op=0x");
        Serial.println(notifLen ? notifBuf[0] : 0, HEX);
        return false;
    }

    const uint32_t peerBytes = getU32le(notifBuf + 1);
    if (peerBytes != expectedBytes) {
        Serial.print("DFU ERROR: PRN mismatch local=");
        Serial.print(expectedBytes);
        Serial.print(" peer=");
        Serial.println(peerBytes);
        return false;
    }

    return true;
}

static bool readSerialExact(uint8_t *dst, uint16_t want, uint32_t timeoutMs)
{
    uint16_t got = 0;
    const uint32_t started = millis();

    while (got < want && (millis() - started < timeoutMs)) {
        while (Serial.available() && got < want) {
            const int b = Serial.read();
            if (b >= 0) dst[got++] = (uint8_t)b;
        }
        if (got < want) delay(1);
    }

    return got == want;
}

static bool requestSerialBlock(const char *kind, uint32_t offset, uint16_t want, uint8_t *dst)
{
    Serial.print("REQ ");
    Serial.print(kind);
    Serial.print(' ');
    Serial.print(offset);
    Serial.print(' ');
    Serial.println(want);
    Serial.flush();

    if (!readSerialExact(dst, want, 15000)) {
        Serial.println("DFU ERROR: USB serial block timeout");
        return false;
    }
    return true;
}

static bool writePacketReliable(const uint8_t *data, uint16_t len)
{
    for (uint16_t tries = 0; tries < 400; tries++) {
        const int w = legacyPacket.write(data, len);
        if (w == len) return true;
        if (!legacyConnected) return false;
        delay(5);
    }
    return false;
}

static void abortLegacyDfu()
{
    if (legacyConnected) {
        const uint8_t resetCmd[1] = {OP_RESET};
        legacyCtrl.write_resp(resetCmd, sizeof(resetCmd));
        delay(500);
    }
}

static bool runLegacyDfuFromSerial(uint32_t datSize, uint32_t binSize)
{
    if (!legacyReady || !legacyConnected || connectedHandle == BLE_CONN_HANDLE_INVALID) {
        Serial.println("DFU ERROR: no AdaDFU target connected");
        return false;
    }

    if (datSize == 0 || datSize > 2048 || binSize == 0 || binSize > 900000) {
        Serial.println("DFU ERROR: invalid DAT/BIN sizes");
        return false;
    }

    responseReady = false;
    notifReady = false;

    Serial.print("DFU BEGIN dat=");
    Serial.print(datSize);
    Serial.print(" bin=");
    Serial.println(binSize);
    Serial.println("DFU NOTE: START may take up to 120 seconds while target erases flash.");
    Serial.flush();

    // Application-only legacy DFU: START_DFU = [0x01, 0x04].
    const uint8_t startCmd[2] = {OP_START_DFU, 0x04};
    if (legacyCtrl.write_resp(startCmd, sizeof(startCmd)) <= 0) {
        Serial.println("DFU ERROR: START_DFU write failed");
        return false;
    }

    uint8_t sizes[12] = {0};
    putU32le(sizes + 8, binSize);
    if (!writePacketReliable(sizes, sizeof(sizes))) {
        Serial.println("DFU ERROR: image-size write failed");
        abortLegacyDfu();
        return false;
    }

    uint8_t status = waitResponse(OP_START_DFU, 120000);
    Serial.print("START_DFU status=0x");
    Serial.println(status, HEX);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t initStart[2] = {OP_INIT_DFU_PARAMS, 0x00};
    const uint8_t initDone[2] = {OP_INIT_DFU_PARAMS, 0x01};
    if (legacyCtrl.write_resp(initStart, sizeof(initStart)) <= 0) {
        Serial.println("DFU ERROR: INIT start failed");
        abortLegacyDfu();
        return false;
    }

    static uint8_t serialBlock[SERIAL_BLOCK];
    uint32_t datOff = 0;
    while (datOff < datSize) {
        uint16_t want = (uint16_t)(datSize - datOff);
        if (want > SERIAL_BLOCK) want = SERIAL_BLOCK;
        if (!requestSerialBlock("DAT", datOff, want, serialBlock)) {
            abortLegacyDfu();
            return false;
        }

        uint16_t pos = 0;
        while (pos < want) {
            uint16_t n = want - pos;
            if (n > BLE_PAYLOAD) n = BLE_PAYLOAD;
            if (!writePacketReliable(serialBlock + pos, n)) {
                Serial.println("DFU ERROR: DAT BLE write failed");
                abortLegacyDfu();
                return false;
            }
            pos += n;
        }
        datOff += want;
    }

    delay(50);
    if (legacyCtrl.write_resp(initDone, sizeof(initDone)) <= 0) {
        Serial.println("DFU ERROR: INIT complete write failed");
        abortLegacyDfu();
        return false;
    }

    status = waitResponse(OP_INIT_DFU_PARAMS, 30000);
    Serial.print("INIT status=0x");
    Serial.println(status, HEX);
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
        Serial.println("DFU ERROR: PRN command failed");
        abortLegacyDfu();
        return false;
    }

    const uint8_t recvCmd[1] = {OP_RECEIVE_FW};
    if (legacyCtrl.write_resp(recvCmd, sizeof(recvCmd)) <= 0) {
        Serial.println("DFU ERROR: RECEIVE_FW command failed");
        abortLegacyDfu();
        return false;
    }

    Serial.println("DFU STREAM START");
    Serial.flush();

    uint32_t sent = 0;
    uint16_t packetsSincePrn = 0;
    uint8_t lastPct = 255;

    while (sent < binSize) {
        uint16_t blockWant = (uint16_t)(binSize - sent);
        if (blockWant > SERIAL_BLOCK) blockWant = SERIAL_BLOCK;

        if (!requestSerialBlock("BIN", sent, blockWant, serialBlock)) {
            abortLegacyDfu();
            return false;
        }

        uint16_t pos = 0;
        while (pos < blockWant) {
            uint16_t n = blockWant - pos;
            if (n > BLE_PAYLOAD) n = BLE_PAYLOAD;

            if (!writePacketReliable(serialBlock + pos, n)) {
                Serial.print("DFU ERROR: firmware BLE write failed at ");
                Serial.println(sent + pos);
                abortLegacyDfu();
                return false;
            }

            pos += n;
            sent += n;
            packetsSincePrn++;

            if (PRN_PACKETS > 0 && packetsSincePrn >= PRN_PACKETS) {
                packetsSincePrn = 0;
                if (sent < binSize && !waitPrn(sent, 10000)) {
                    abortLegacyDfu();
                    return false;
                }
            }

            const uint8_t pct = (uint8_t)(((uint64_t)sent * 100ULL) / binSize);
            if (pct != lastPct && (pct % 2 == 0 || pct == 100)) {
                lastPct = pct;
                Serial.print("PROGRESS ");
                Serial.print(pct);
                Serial.print("% ");
                Serial.print(sent);
                Serial.print("/");
                Serial.println(binSize);
                Serial.flush();
            }
        }
    }

    Serial.println("DFU STREAM COMPLETE; waiting for target flash response...");
    status = waitResponse(OP_RECEIVE_FW, 60000);
    Serial.print("RECEIVE_FW status=0x");
    Serial.println(status, HEX);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t validateCmd[1] = {OP_VALIDATE};
    if (legacyCtrl.write_resp(validateCmd, sizeof(validateCmd)) <= 0) {
        Serial.println("DFU ERROR: VALIDATE write failed");
        abortLegacyDfu();
        return false;
    }

    status = waitResponse(OP_VALIDATE, 60000);
    Serial.print("VALIDATE status=0x");
    Serial.println(status, HEX);
    if (status != STATUS_SUCCESS) {
        abortLegacyDfu();
        return false;
    }

    const uint8_t activateCmd[1] = {OP_ACTIVATE_AND_RESET};
    legacyCtrl.write_resp(activateCmd, sizeof(activateCmd));
    Serial.println("ACTIVATE sent; waiting up to 120 seconds for target reboot...");
    Serial.flush();

    const uint32_t started = millis();
    while (legacyConnected && (millis() - started < 120000)) {
        delay(50);
    }

    if (legacyConnected) {
        Serial.println("DFU ERROR: target did not reboot after ACTIVATE");
        return false;
    }

    Serial.println("DFU SUCCESS: target accepted image and rebooted");
    Serial.flush();
    return true;
}

static void pollSerialCommand()
{
    static char line[96];
    static uint8_t len = 0;

    while (Serial.available()) {
        const int v = Serial.read();
        if (v < 0) return;
        const char ch = (char)v;

        if (ch == '\r') continue;
        if (ch == '\n') {
            line[len] = '\0';
            len = 0;

            if (strcmp(line, "STATUS") == 0) {
                Serial.print("DFU_STATUS ");
                Serial.println(legacyReady && legacyConnected ? "READY" : "WAITING");
                Serial.flush();
                continue;
            }

            uint32_t datSize = 0;
            uint32_t binSize = 0;
            if (sscanf(line, "UPLOAD %lu %lu", (unsigned long *)&datSize, (unsigned long *)&binSize) == 2) {
                if (runLegacyDfuFromSerial(datSize, binSize)) {
                    legacyReady = false;
                }
                continue;
            }

            if (line[0] != '\0') {
                Serial.print("UNKNOWN COMMAND: ");
                Serial.println(line);
            }
            continue;
        }

        if (len < sizeof(line) - 1) {
            line[len++] = ch;
        } else {
            len = 0;
        }
    }
}
