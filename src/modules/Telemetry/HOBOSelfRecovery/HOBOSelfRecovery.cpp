#include "configuration.h"

#if defined(ARCH_NRF52) && (defined(SEEED_XIAO_NRF52840_KIT) || defined(RAK_4631))

#include "HOBOSelfRecovery.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "main.h"

#include <bluefruit.h>
#include <nrf.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
static constexpr uint32_t BOOT_SETTLE_MS = 30000UL;
static constexpr uint32_t SUPERVISOR_INTERVAL_MS = 30000UL;
static constexpr uint32_t SCAN_RESTART_INTERVAL_MS = 30UL * 60UL * 1000UL;
static constexpr uint32_t BLE_STALE_REBOOT_MS = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t WDT_TIMEOUT_SECONDS = 15UL * 60UL;
static constexpr uint32_t WDT_TICKS_PER_SECOND = 32768UL;
static constexpr uint32_t WDT_RELOAD_MAGIC = 0x6E524635UL;
static constexpr uint16_t LOW_DUTY_SCAN_INTERVAL = 320; // 200 ms
static constexpr uint16_t LOW_DUTY_SCAN_WINDOW = 32;    // 20 ms = 10% receive duty
static constexpr char FIRMWARE_LABEL[] = "HOBO SELF-RECOVERY 1.0";

bool watchdogOwned = false;
bool watchdogChecked = false;
bool bleTuned = false;
bool lastScannerRunning = false;
bool haveScannerState = false;
bool rebootPending = false;
uint32_t scannerStateSinceMs = 0;
uint32_t scanRestartCount = 0;
uint32_t automaticRecoveryCount = 0;
uint32_t commandRecoveryCount = 0;
uint32_t resetReasonAtBoot = 0;
uint32_t rebootDueMs = 0;

bool reached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

bool isCommand(const uint8_t *bytes, size_t size, const char *expected)
{
    if (bytes == nullptr || size == 0 || expected == nullptr)
        return false;

    char command[40] = {};
    size_t n = size;
    if (n > sizeof(command) - 1)
        n = sizeof(command) - 1;
    memcpy(command, bytes, n);

    char *p = command;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p == '/')
        ++p;

    const size_t expectedLength = strlen(expected);
    for (size_t i = 0; i < expectedLength; ++i) {
        if (p[i] == '\0')
            return false;
        if (std::toupper(static_cast<unsigned char>(p[i])) !=
            std::toupper(static_cast<unsigned char>(expected[i])))
            return false;
    }

    p += expectedLength;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    return *p == '\0';
}

const char *platformName()
{
#if defined(RAK_4631)
    return "RAK4631";
#else
    return "SEEED-XIAO";
#endif
}

void feedInternalWatchdog()
{
    if (watchdogOwned)
        NRF_WDT->RR[0] = WDT_RELOAD_MAGIC;
}

void initializeInternalWatchdog()
{
    if (watchdogChecked)
        return;

    watchdogChecked = true;

    // Do not take over a watchdog already owned by the board/core.
    if (NRF_WDT->RUNSTATUS != 0) {
        LOG_INFO("HOBO self-recovery: on-chip watchdog already running; leaving owner unchanged");
        return;
    }

#ifdef WDT_CONFIG_SLEEP_Run
    NRF_WDT->CONFIG =
        (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
        (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
#else
    // nRF52 CONFIG bit 0 = watchdog runs while CPU is sleeping.
    NRF_WDT->CONFIG = 1UL;
#endif
    NRF_WDT->CRV = WDT_TICKS_PER_SECOND * WDT_TIMEOUT_SECONDS;
    NRF_WDT->RREN = 1UL;
    NRF_WDT->TASKS_START = 1UL;
    watchdogOwned = true;
    feedInternalWatchdog();

    LOG_INFO("HOBO self-recovery: on-chip watchdog armed for %lu sec and RUN-IN-SLEEP",
             static_cast<unsigned long>(WDT_TIMEOUT_SECONDS));
}

void configureLowDutyScanner(bool restartIfRunning)
{
    const bool wasRunning = Bluefruit.Scanner.isRunning();
    if (wasRunning && restartIfRunning) {
        Bluefruit.Scanner.stop();
        delay(20);
    }

    Bluefruit.Scanner.setInterval(LOW_DUTY_SCAN_INTERVAL, LOW_DUTY_SCAN_WINDOW);
    Bluefruit.Scanner.useActiveScan(false);

    if (wasRunning && restartIfRunning)
        Bluefruit.Scanner.start(0);

    bleTuned = true;
}

void restartScanner()
{
    if (Bluefruit.Scanner.isRunning()) {
        Bluefruit.Scanner.stop();
        delay(20);
    }
    Bluefruit.Scanner.setInterval(LOW_DUTY_SCAN_INTERVAL, LOW_DUTY_SCAN_WINDOW);
    Bluefruit.Scanner.useActiveScan(false);
    Bluefruit.Scanner.start(0);
    scanRestartCount++;
    scannerStateSinceMs = millis();
    lastScannerRunning = true;
    haveScannerState = true;
    LOG_WARN("HOBO self-recovery: BLE scanner restarted (%lu)", static_cast<unsigned long>(scanRestartCount));
}

uint32_t scannerStateAgeSeconds()
{
    if (!haveScannerState)
        return 0;
    return (millis() - scannerStateSinceMs) / 1000UL;
}

} // namespace

HOBOSelfRecoveryModule::HOBOSelfRecoveryModule()
    : SinglePortModule("hobo_self_recovery", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("hobo_self_recovery")
{
    isPromiscuous = true;
    resetReasonAtBoot = NRF_POWER->RESETREAS;
    setIntervalFromNow(5000);
}

bool HOBOSelfRecoveryModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOSelfRecoveryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (nodeDB == nullptr || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    const uint8_t *payload = mp.decoded.payload.bytes;
    const size_t payloadSize = mp.decoded.payload.size;
    char reply[230] = {};

    if (isCommand(payload, payloadSize, "HELP")) {
        sendTextReply(mp.from, mp.channel,
                      "CMDS: READ LOGGER LOCK UNLOCK | STATUS HEALTH POWER BLE AUTO STATS NODES UPTIME VERSION WATCHDOG SCAN RECONNECT RECOVER REBOOT PING HELP");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "PING") || isCommand(payload, payloadSize, "WAKE")) {
        snprintf(reply, sizeof(reply), "PONG %s uptime=%lus", platformName(),
                 static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "VERSION")) {
        snprintf(reply, sizeof(reply), "%s\nPlatform: %s\nNEWREAD/AUTO + self-recovery", FIRMWARE_LABEL, platformName());
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "UPTIME")) {
        snprintf(reply, sizeof(reply), "UPTIME: %lu sec", static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "POWER") || isCommand(payload, payloadSize, "BATTERY")) {
        if (powerStatus != nullptr) {
            snprintf(reply, sizeof(reply), "POWER: %umV %u%% battery=%s charging=%s",
                     powerStatus->getBatteryVoltageMv(),
                     powerStatus->getBatteryChargePercent(),
                     powerStatus->getHasBattery() ? "YES" : "NO",
                     powerStatus->getIsCharging() ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply), "POWER: status unavailable");
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "BLE")) {
        snprintf(reply, sizeof(reply), "BLE: scanner=%s age=%lus low-duty=%s restarts=%lu",
                 Bluefruit.Scanner.isRunning() ? "SCANNING" : "CONNECTED/IDLE",
                 static_cast<unsigned long>(scannerStateAgeSeconds()),
                 bleTuned ? "ON(10%)" : "PENDING",
                 static_cast<unsigned long>(scanRestartCount));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "AUTO")) {
        sendTextReply(mp.from, mp.channel,
                      "AUTO: ON. HOBO core uses STATUS write-pointer + NEWREAD; manual READ does not consume AUTO pointer. Self-recovery supervisor ON.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "WATCHDOG")) {
        snprintf(reply, sizeof(reply), "WATCHDOG: running=%s owner=%s timeout=%lus run-in-sleep=%s",
                 NRF_WDT->RUNSTATUS ? "YES" : "NO",
                 watchdogOwned ? "SELFRECOVERY" : "CORE/OTHER",
                 static_cast<unsigned long>(WDT_TIMEOUT_SECONDS),
                 watchdogOwned ? "YES" : "UNKNOWN");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "NODES")) {
        snprintf(reply, sizeof(reply), "NODES: %u mesh nodes", nodeDB->getNumMeshNodes());
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATS")) {
        snprintf(reply, sizeof(reply), "RECOVERY STATS: scan_restarts=%lu auto_reboots=%lu command_recovers=%lu reset_reason=0x%08lX",
                 static_cast<unsigned long>(scanRestartCount),
                 static_cast<unsigned long>(automaticRecoveryCount),
                 static_cast<unsigned long>(commandRecoveryCount),
                 static_cast<unsigned long>(resetReasonAtBoot));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATUS") || isCommand(payload, payloadSize, "HEALTH")) {
        const uint16_t mv = powerStatus ? powerStatus->getBatteryVoltageMv() : 0;
        const uint8_t pct = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
        snprintf(reply, sizeof(reply), "%s %s\nUptime:%lus Power:%umV/%u%%\nBLE:%s WDT:%s\nScanRestarts:%lu Reset:0x%08lX",
                 FIRMWARE_LABEL,
                 platformName(),
                 static_cast<unsigned long>(millis() / 1000UL),
                 mv,
                 pct,
                 Bluefruit.Scanner.isRunning() ? "SCAN" : "LINK/IDLE",
                 NRF_WDT->RUNSTATUS ? "ON" : "OFF",
                 static_cast<unsigned long>(scanRestartCount),
                 static_cast<unsigned long>(resetReasonAtBoot));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "SCAN")) {
        restartScanner();
        sendTextReply(mp.from, mp.channel, "SCAN: BLE scanner restarted in passive 10% duty mode");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "RECONNECT")) {
        if (Bluefruit.Scanner.isRunning()) {
            restartScanner();
            sendTextReply(mp.from, mp.channel, "RECONNECT: disconnected-state scanner rebuilt");
        } else {
            sendTextReply(mp.from, mp.channel, "RECONNECT: BLE link appears active. Use RECOVER for a full safe reboot if link is stale.");
        }
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "RECOVER") || isCommand(payload, payloadSize, "REBOOT")) {
        commandRecoveryCount++;
        rebootPending = true;
        rebootDueMs = millis() + 2000UL;
        feedInternalWatchdog();
        sendTextReply(mp.from, mp.channel,
                      "RECOVERY: safe reboot in 2 sec. NVS, node identity, channels, keys and logger lock are preserved.");
        setIntervalFromNow(100);
        return ProcessMessage::CONTINUE;
    }

    // READ/LOGGER/LOCK/UNLOCK are intentionally left to the proven HOBO module.
    return ProcessMessage::CONTINUE;
}

bool HOBOSelfRecoveryModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr)
        return false;

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

int32_t HOBOSelfRecoveryModule::runOnce()
{
    const uint32_t now = millis();

    if (rebootPending && reached(now, rebootDueMs)) {
        LOG_WARN("HOBO self-recovery: executing requested safe reboot");
        feedInternalWatchdog();
        delay(20);
        NVIC_SystemReset();
        return 1000;
    }

    if (now < BOOT_SETTLE_MS)
        return 5000;

    initializeInternalWatchdog();
    feedInternalWatchdog();

    if (!bleTuned) {
        configureLowDutyScanner(true);
        LOG_INFO("HOBO self-recovery: BLE passive scan tuned to 10%% duty");
    }

    const bool scannerRunning = Bluefruit.Scanner.isRunning();
    if (!haveScannerState || scannerRunning != lastScannerRunning) {
        haveScannerState = true;
        lastScannerRunning = scannerRunning;
        scannerStateSinceMs = now;
    }

    if (scannerRunning) {
        const uint32_t scanAge = now - scannerStateSinceMs;

        if (scanAge >= BLE_STALE_REBOOT_MS) {
            automaticRecoveryCount++;
            LOG_ERROR("HOBO self-recovery: BLE has scanned continuously for 6h; rebooting to rebuild BLE stack");
            rebootPending = true;
            rebootDueMs = now + 1000UL;
            setIntervalFromNow(100);
            return 100;
        }

        if (scanAge >= SCAN_RESTART_INTERVAL_MS) {
            restartScanner();
            return 1000;
        }
    }

    feedInternalWatchdog();
    return SUPERVISOR_INTERVAL_MS;
}

#endif
