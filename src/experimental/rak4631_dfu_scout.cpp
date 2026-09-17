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

static void startScan();
static void scanCallback(ble_gap_evt_adv_report_t *report);
static void connectCallback(uint16_t connHandle);
static void disconnectCallback(uint16_t connHandle, uint8_t reason);

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
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(4);
    Bluefruit.setName("RAK_DFU_SCOUT");
    Bluefruit.setConnLedInterval(250);

    // Register both client-side DFU services so we can perform real GATT
    // discovery after connecting, rather than trusting advertisements.
    secureDfuService.begin();
    legacyDfuService.begin();

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

        Serial.println("PHASE 1 PASS: AdaDFU legacy service 0x1530 discovered");
        Serial.println("Target is in BLE OTA bootloader and reachable from this RAK.");
        Serial.println("Next phase is legacy DFU Control Point 0x1531 + Packet 0x1532 transfer.");
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

    Serial.print("DISCONNECTED, reason 0x");
    Serial.println(reason, HEX);
    Serial.println("Resuming DFU scan...");
}