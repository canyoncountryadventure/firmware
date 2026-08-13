#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "MX2201Telemetry.h"
#include "main.h"

#include <bluefruit.h>
#include <cstring>

namespace
{

// HOBO MX2201 used for the first Chew can integration.
// Visible BLE MAC: EB:9A:E4:52:6D:5F
//
// Bluefruit / Nordic byte order:
static const uint8_t HOBO_MAC[6] = {
    0x5F,
    0x6D,
    0x52,
    0xE4,
    0x9A,
    0xEB
};

// HOBO service:
// 65e16e4f-ed4e-4641-ac49-83ccbce6cbcf
static const uint8_t HOBO_SERVICE_UUID[16] = {
    0xCF, 0xCB, 0xE6, 0xBC,
    0xCC, 0x83,
    0x49, 0xAC,
    0x41, 0x46,
    0x4E, 0xED,
    0x4F, 0x6E,
    0xE1, 0x65
};

// HOBO command / notification characteristic:
// 65e16f4f-ed4e-4641-ac49-83ccbce6cbcf
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

bool hoboClientInitialized = false;
bool hoboConnected = false;

bool isTargetHobo(const ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr) {
        return false;
    }

    return std::memcmp(
               report->peer_addr.addr,
               HOBO_MAC,
               sizeof(HOBO_MAC)) == 0;
}

void hoboNotifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;
    (void)data;

    LOG_INFO("MX2201: BLE notification received, %u bytes", len);
}

void hoboDisconnectCallback(uint16_t connHandle, uint8_t reason)
{
    (void)connHandle;

    hoboConnected = false;

    LOG_INFO(
        "MX2201: disconnected, reason=0x%02x",
        reason);
}

void hoboConnectCallback(uint16_t connHandle)
{
    LOG_INFO("MX2201: BLE connection established");

    LOG_INFO("MX2201: discovering HOBO service");

    if (!hoboService.discover(connHandle)) {
        LOG_WARN("MX2201: HOBO service not found");
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2201: HOBO service found");

    LOG_INFO("MX2201: discovering command characteristic");

    if (!hoboCharacteristic.discover()) {
        LOG_WARN("MX2201: command characteristic not found");
        Bluefruit.disconnect(connHandle);
        return;
    }

    LOG_INFO("MX2201: command characteristic found");

    if (!hoboCharacteristic.enableNotify()) {
        LOG_WARN("MX2201: failed to enable notifications");
        Bluefruit.disconnect(connHandle);
        return;
    }

    hoboConnected = true;

    LOG_INFO("========================================");
    LOG_INFO("MX2201 CONNECTED AND READY");
    LOG_INFO("Service found: YES");
    LOG_INFO("Characteristic found: YES");
    LOG_INFO("Notifications enabled: YES");
    LOG_INFO("========================================");
}

void hoboScanCallback(ble_gap_evt_adv_report_t *report)
{
    if (!isTargetHobo(report)) {
        return;
    }

    LOG_INFO("MX2201: target logger found");

    Bluefruit.Scanner.stop();

    LOG_INFO("MX2201: connecting");

    if (!Bluefruit.Central.connect(report)) {
        LOG_WARN("MX2201: connection attempt failed");
    }
}

void initializeHoboClient()
{
    LOG_INFO("MX2201: initializing BLE central client");

    hoboService.begin();

    hoboCharacteristic.setNotifyCallback(
        hoboNotifyCallback);

    hoboCharacteristic.begin(
        &hoboService);

    Bluefruit.Central.setConnectCallback(
        hoboConnectCallback);

    Bluefruit.Central.setDisconnectCallback(
        hoboDisconnectCallback);

    Bluefruit.Scanner.setRxCallback(
        hoboScanCallback);

    Bluefruit.Scanner.restartOnDisconnect(false);

    // 100 ms interval, 50 ms scan window.
    // Units are 0.625 ms.
    Bluefruit.Scanner.setInterval(160, 80);

    Bluefruit.Scanner.useActiveScan(false);

    hoboClientInitialized = true;

    LOG_INFO("MX2201: BLE central client initialized");
}

} // namespace

MX2201TelemetryModule::MX2201TelemetryModule()
    : concurrency::OSThread("MX2201Telemetry")
{
    // Allow the normal Meshtastic boot sequence and Bluetooth
    // peripheral setup to finish first.
    setIntervalFromNow(10000);
}

int32_t MX2201TelemetryModule::runOnce()
{
    // Meshtastic's NRF52 Bluetooth object is created when the
    // normal Meshtastic BLE stack is ready. Do not touch the
    // Bluefruit central/scanner before that point.
    if (nrf52Bluetooth == nullptr) {
        LOG_INFO("MX2201: waiting for Meshtastic Bluetooth startup");
        return 5000;
    }

    if (!config.bluetooth.enabled) {
        LOG_WARN("MX2201: Meshtastic Bluetooth is disabled");
        return 30000;
    }

    if (!hoboClientInitialized) {
        initializeHoboClient();
    }

    if (hoboConnected) {
        return 30000;
    }

    LOG_INFO("MX2201: scanning for EB:9A:E4:52:6D:5F");

    if (!Bluefruit.Scanner.start(5)) {
        LOG_WARN("MX2201: could not start BLE scan");
    }

    // Scan for five seconds and retry approximately every
    // ten seconds until the MX2201 is found.
    return 10000;
}

#endif