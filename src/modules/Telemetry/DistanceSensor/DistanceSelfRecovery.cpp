#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "DistanceSelfRecovery.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "main.h"

#include <nrf.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
static constexpr uint32_t BOOT_SETTLE_MS = 30000UL;
static constexpr uint32_t SUPERVISOR_INTERVAL_MS = 30000UL;
static constexpr uint32_t WDT_TIMEOUT_SECONDS = 15UL * 60UL;
static constexpr uint32_t WDT_TICKS_PER_SECOND = 32768UL;
static constexpr uint32_t WDT_RELOAD_MAGIC = 0x6E524635UL;
static constexpr char FIRMWARE_LABEL[] = "DISTANCE SELF-RECOVERY 1.0";

bool watchdogOwned = false;
bool watchdogChecked = false;
bool rebootPending = false;
uint32_t rebootDueMs = 0;
uint32_t resetReasonAtBoot = 0;
uint32_t commandRecoveryCount = 0;

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
    return "RAK4631/RAK19007";
#else
    return "SEEED-XIAO";
#endif
}

void feedWatchdog()
{
    if (watchdogOwned)
        NRF_WDT->RR[0] = WDT_RELOAD_MAGIC;
}

void initializeWatchdog()
{
    if (watchdogChecked)
        return;
    watchdogChecked = true;

    if (NRF_WDT->RUNSTATUS != 0) {
        LOG_INFO("Distance recovery: on-chip watchdog already running; leaving ownership unchanged");
        return;
    }

    NRF_WDT->CONFIG = 1UL; // run while CPU sleeps
    NRF_WDT->CRV = WDT_TICKS_PER_SECOND * WDT_TIMEOUT_SECONDS;
    NRF_WDT->RREN = 1UL;
    NRF_WDT->TASKS_START = 1UL;
    watchdogOwned = true;
    feedWatchdog();
    LOG_INFO("Distance recovery: watchdog armed for %lu sec", static_cast<unsigned long>(WDT_TIMEOUT_SECONDS));
}
} // namespace

DistanceSelfRecoveryModule::DistanceSelfRecoveryModule()
    : SinglePortModule("distance_self_recovery", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("distance_self_recovery")
{
    isPromiscuous = true;
    resetReasonAtBoot = NRF_POWER->RESETREAS;
    setIntervalFromNow(5000);
}

bool DistanceSelfRecoveryModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage DistanceSelfRecoveryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (nodeDB == nullptr || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    char reply[220] = {};

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "PING") ||
        isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "WAKE")) {
        snprintf(reply, sizeof(reply), "PONG %s uptime=%lus", platformName(), static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "VERSION")) {
        snprintf(reply, sizeof(reply), "%s\nPlatform: %s\nDistance sensor field firmware", FIRMWARE_LABEL, platformName());
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "UPTIME")) {
        snprintf(reply, sizeof(reply), "UPTIME: %lu sec", static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "POWER") ||
        isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "BATTERY")) {
        if (powerStatus != nullptr) {
            snprintf(reply, sizeof(reply), "POWER: %umV %u%% battery=%s charging=%s",
                     static_cast<unsigned int>(powerStatus->getBatteryVoltageMv()),
                     static_cast<unsigned int>(powerStatus->getBatteryChargePercent()),
                     powerStatus->getHasBattery() ? "YES" : "NO", powerStatus->getIsCharging() ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply), "POWER: status unavailable");
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "WATCHDOG")) {
        snprintf(reply, sizeof(reply), "WATCHDOG: running=%s owner=%s timeout=%lus reset=0x%08lX",
                 NRF_WDT->RUNSTATUS ? "YES" : "NO", watchdogOwned ? "DISTANCE" : "CORE/OTHER",
                 static_cast<unsigned long>(WDT_TIMEOUT_SECONDS), static_cast<unsigned long>(resetReasonAtBoot));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "RECOVER") ||
        isCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, "REBOOT")) {
        commandRecoveryCount++;
        rebootPending = true;
        rebootDueMs = millis() + 2000UL;
        feedWatchdog();
        sendTextReply(mp.from, mp.channel,
                      "RECOVERY: safe reboot in 2 sec. NVS, node identity, channels, keys, calibration and counts are preserved.");
        setIntervalFromNow(100);
        return ProcessMessage::CONTINUE;
    }

    return ProcessMessage::CONTINUE;
}

bool DistanceSelfRecoveryModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
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

int32_t DistanceSelfRecoveryModule::runOnce()
{
    const uint32_t now = millis();

    if (rebootPending && reached(now, rebootDueMs)) {
        LOG_WARN("Distance recovery: executing safe reboot after command #%lu", static_cast<unsigned long>(commandRecoveryCount));
        feedWatchdog();
        delay(20);
        NVIC_SystemReset();
        return 1000;
    }

    if (now < BOOT_SETTLE_MS)
        return 5000;

    initializeWatchdog();
    feedWatchdog();
    return SUPERVISOR_INTERVAL_MS;
}

#endif
