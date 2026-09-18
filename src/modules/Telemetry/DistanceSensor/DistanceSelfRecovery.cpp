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
static constexpr char FIRMWARE_LABEL[] = "WATER DISTANCE 2.0";

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

bool normalizeRecoveryCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize)
{
    if (bytes == nullptr || size == 0 || out == nullptr || outSize < 2)
        return false;

    size_t n = size;
    if (n >= outSize)
        n = outSize - 1;
    memcpy(out, bytes, n);
    out[n] = '\0';

    char *p = out;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p == '/')
        ++p;
    if (p != out)
        memmove(out, p, strlen(p) + 1);

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t' || out[len - 1] == '\r' || out[len - 1] == '\n'))
        out[--len] = '\0';

    for (size_t i = 0; i < len; ++i)
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));

    if (strncmp(out, "DIST ", 5) == 0)
        memmove(out, out + 5, strlen(out + 5) + 1);
    else if (strncmp(out, "WATER ", 6) == 0)
        memmove(out, out + 6, strlen(out + 6) + 1);

    return out[0] != '\0';
}

bool isCommand(const char *command, const char *expected)
{
    return command != nullptr && expected != nullptr && strcmp(command, expected) == 0;
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
        LOG_INFO("Water recovery: on-chip watchdog already running; leaving ownership unchanged");
        return;
    }

    NRF_WDT->CONFIG = 1UL;
    NRF_WDT->CRV = WDT_TICKS_PER_SECOND * WDT_TIMEOUT_SECONDS;
    NRF_WDT->RREN = 1UL;
    NRF_WDT->TASKS_START = 1UL;
    watchdogOwned = true;
    feedWatchdog();
    LOG_INFO("Water recovery: watchdog armed for %lu sec", static_cast<unsigned long>(WDT_TIMEOUT_SECONDS));
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

    char command[64] = {};
    if (!normalizeRecoveryCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, sizeof(command)))
        return ProcessMessage::CONTINUE;

    char reply[220] = {};

    if (isCommand(command, "PING") || isCommand(command, "WAKE")) {
        snprintf(reply, sizeof(reply), "PONG %s uptime=%lus", platformName(), static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(command, "VERSION")) {
        snprintf(reply, sizeof(reply), "%s\nPlatform:%s\nWater-only ultrasonic field firmware", FIRMWARE_LABEL, platformName());
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(command, "UPTIME")) {
        snprintf(reply, sizeof(reply), "UPTIME:%lu sec", static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(command, "POWER") || isCommand(command, "BATTERY")) {
        if (powerStatus != nullptr) {
            snprintf(reply, sizeof(reply), "POWER:%umV %u%% battery=%s charging=%s",
                     static_cast<unsigned int>(powerStatus->getBatteryVoltageMv()),
                     static_cast<unsigned int>(powerStatus->getBatteryChargePercent()),
                     powerStatus->getHasBattery() ? "YES" : "NO", powerStatus->getIsCharging() ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply), "POWER:status unavailable");
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(command, "WATCHDOG")) {
#if defined(FIELD_RECOVERY_V2)
        snprintf(reply, sizeof(reply), "WATCHDOG:core=90s field-channel=%s run-in-sleep=YES reset=0x%08lX",
                 nrf52FieldWatchdogIsArmed() ? "ARMED" : "OFF", static_cast<unsigned long>(resetReasonAtBoot));
#else
        if (watchdogOwned) {
            snprintf(reply, sizeof(reply), "WATCHDOG:running=YES owner=WATER timeout=%lus reset=0x%08lX",
                     static_cast<unsigned long>(WDT_TIMEOUT_SECONDS), static_cast<unsigned long>(resetReasonAtBoot));
        } else {
            snprintf(reply, sizeof(reply), "WATCHDOG:running=%s owner=CORE/OTHER custom_timeout=N/A reset=0x%08lX",
                     NRF_WDT->RUNSTATUS ? "YES" : "NO", static_cast<unsigned long>(resetReasonAtBoot));
        }
#endif
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(command, "RECOVER") || isCommand(command, "REBOOT")) {
        commandRecoveryCount++;
#if defined(FIELD_RECOVERY_V2)
        if (rebootAtMsec == 0)
            rebootAtMsec = millis() + 2000UL;
#else
        rebootPending = true;
        rebootDueMs = millis() + 2000UL;
        feedWatchdog();
        setIntervalFromNow(100);
#endif
        sendTextReply(mp.from, mp.channel,
                      "RECOVERY:flash-safe reboot in 2 sec. Meshtastic identity/keys/channels and WATER calibration/settings are preserved.");
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

#if !defined(FIELD_RECOVERY_V2)
    if (rebootPending && reached(now, rebootDueMs)) {
        LOG_WARN("Water recovery: executing legacy reboot after command #%lu", static_cast<unsigned long>(commandRecoveryCount));
        feedWatchdog();
        delay(20);
        NVIC_SystemReset();
        return 1000;
    }
#endif

    if (now < BOOT_SETTLE_MS)
        return 5000;

    initializeWatchdog();
    feedWatchdog();
    return SUPERVISOR_INTERVAL_MS;
}

#endif
