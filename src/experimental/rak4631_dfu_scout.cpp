#include <Arduino.h>
#include <bluefruit.h>

// Phase 1 bench-test build for the drone flasher proof of concept.
//
// This firmware turns a spare RAK4631 into a BLE Central-only DFU scout.
// It does NOT transmit a firmware image yet. It proves that a nearby RAK4631
// can detect a target node after that target has been told (locally or by
// Meshtastic remote-admin) to reboot into Nordic Secure DFU mode.
//
// Nordic Secure DFU service UUID: 0xFE59.
static BLEClientService dfuService(0xFE59);
static volatile bool connecting = false;

static void startScan();
static void scanCallback(ble_gap_evt_adv_report_t *report);
static void connectCallback(uint16_t connHandle);
static void disconnectCallback(uint16_t connHandle, uint8_t reason);

void setup()
{
    Serial.begin(115200);

    // Do not block forever when this eventually flies without a USB cable.
    const uint32_t serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart < 3000)) {
        delay(10);
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("RAK4631 Nordic DFU Scout - Phase 1");
    Serial.println("Looking for Secure DFU service 0xFE59");
    Serial.println("========================================");

    // RAK is the BLE Central only: 0 peripheral links, 1 central link.
    Bluefruit.begin(0, 1);
    Bluefruit.setTxPower(4);
    Bluefruit.setName("RAK_DFU_SCOUT");
    Bluefruit.setConnLedInterval(250);

    // Register the client-side Secure DFU service so we can perform a real
    // GATT discovery after connecting, rather than trusting the advertisement.
    dfuService.begin();

    Bluefruit.Central.setConnectCallback(connectCallback);
    Bluefruit.Central.setDisconnectCallback(disconnectCallback);

    startScan();
}

void loop()
{
    // Phase 1 is callback driven. Nothing is sent to the target.
    delay(100);
}

static void startScan()
{
    connecting = false;

    Bluefruit.Scanner.setRxCallback(scanCallback);
    Bluefruit.Scanner.restartOnDisconnect(true);
    Bluefruit.Scanner.setInterval(160, 80); // 100 ms interval, 50 ms window
    Bluefruit.Scanner.useActiveScan(true);
    Bluefruit.Scanner.start(0); // scan indefinitely

    Serial.println("SCANNING: waiting for a Nordic Secure DFU target...");
}

static void scanCallback(ble_gap_evt_adv_report_t *report)
{
    // SoftDevice pauses scanning while this callback runs. Resume it whenever
    // we decide not to connect.
    if (!Bluefruit.Scanner.checkReportForService(report, dfuService)) {
        Bluefruit.Scanner.resume();
        return;
    }

    if (connecting) {
        Bluefruit.Scanner.resume();
        return;
    }

    uint8_t name[32] = {0};
    Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name) - 1);

    Serial.println();
    Serial.println("DFU ADVERTISEMENT FOUND");
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
    Serial.println("  advertised service: 0xFE59");
    Serial.println("CONNECTING...");

    connecting = true;
    if (!Bluefruit.Central.connect(report)) {
        connecting = false;
        Serial.println("CONNECT FAILED; resuming scan");
        Bluefruit.Scanner.resume();
    }
}

static void connectCallback(uint16_t connHandle)
{
    connecting = false;
    Serial.println("BLE CONNECTED");
    Serial.println("DISCOVERING Secure DFU service over GATT...");

    if (!dfuService.discover(connHandle)) {
        Serial.println("PHASE 1 FAIL: connected, but Secure DFU service was not discoverable");
        Bluefruit.disconnect(connHandle);
        return;
    }

    Serial.println("PHASE 1 PASS: Secure DFU service 0xFE59 discovered");
    Serial.println("Target is in the correct bootloader and reachable from this RAK.");
    Serial.println("No firmware bytes will be written in this build.");
    Serial.println("Leave connected for inspection; power-cycle/reset the target when finished.");
}

static void disconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;
    connecting = false;

    Serial.print("DISCONNECTED, reason 0x");
    Serial.println(reason, HEX);
    Serial.println("Resuming DFU scan...");
}