#include "configuration.h"

#if defined(ARCH_NRF52) && (defined(SEEED_XIAO_NRF52840_KIT) || defined(RAK_4631))

#include "HOBOSelfRecovery.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "main.h"
#include "mesh/RadioLibInterface.h"

#include <bluefruit.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
static constexpr uint32_t BOOT_SETTLE_MS = 30000UL;
static constexpr uint32_t SUPERVISOR_INTERVAL_MS = 30000UL;
static constexpr char FIRMWARE_LABEL[] = "HOBO FIELD-RECOVERY v2";

uint32_t commandRecoveryCount = 0;
uint32_t resetReasonAtBoot = 0;

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

} // namespace

HOBOSelfRecoveryModule::HOBOSelfRecoveryModule()
    : SinglePortModule("hobo_self_recovery", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("hobo_self_recovery")
{
    isPromiscuous = true;
    resetReasonAtBoot = readResetReason();
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
                      "CMDS: READ LOGGER LOCK UNLOCK | STATUS HEALTH POWER BLE AUTO STATS NODES UPTIME VERSION WATCHDOG RECOVER REBOOT DFU PING HELP");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "PING") || isCommand(payload, payloadSize, "WAKE")) {
        snprintf(reply, sizeof(reply), "PONG %s uptime=%lus", platformName(),
                 static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "VERSION")) {
        snprintf(reply, sizeof(reply), "%s\nPlatform:%s\nSX1262+BLE+dual-WDT+12h safety reset",
                 FIRMWARE_LABEL, platformName());
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
                     static_cast<unsigned int>(powerStatus->getBatteryVoltageMv()),
                     static_cast<unsigned int>(powerStatus->getBatteryChargePercent()),
                     powerStatus->getHasBattery() ? "YES" : "NO",
                     powerStatus->getIsCharging() ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply), "POWER: status unavailable");
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "BLE")) {
        snprintf(reply, sizeof(reply), "BLE: central_links=%u scanner=%s owner=HOBO-state-machine",
                 static_cast<unsigned int>(Bluefruit.Central.connected()),
                 Bluefruit.Scanner.isRunning() ? "SCANNING" : "STOPPED");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "AUTO")) {
        sendTextReply(mp.from, mp.channel,
                      "AUTO: ON. STATUS pointer gating + NEWREAD. BLE connect/STATUS/read failures self-recover.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "WATCHDOG")) {
        snprintf(reply, sizeof(reply), "WATCHDOG: core=90s field-channel=%s run-in-sleep=YES",
                 nrf52FieldWatchdogIsArmed() ? "ARMED" : "OFF");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "NODES")) {
        snprintf(reply, sizeof(reply), "NODES: %u mesh nodes",
                 static_cast<unsigned int>(nodeDB->getNumMeshNodes()));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATS")) {
        const auto *radio = RadioLibInterface::instance;
        snprintf(reply, sizeof(reply),
                 "STATS: radio_recover=%lu/%lu tx=%lu last_tx=%lus cmd_reboots=%lu reset=0x%08lX",
                 radio ? static_cast<unsigned long>(radio->radioRecoverySuccesses) : 0UL,
                 radio ? static_cast<unsigned long>(radio->radioRecoveryAttempts) : 0UL,
                 radio ? static_cast<unsigned long>(radio->txGood) : 0UL,
                 radio && radio->lastTxCompleteMs ? static_cast<unsigned long>((millis() - radio->lastTxCompleteMs) / 1000UL) : 0UL,
                 static_cast<unsigned long>(commandRecoveryCount),
                 static_cast<unsigned long>(resetReasonAtBoot));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATUS") || isCommand(payload, payloadSize, "HEALTH")) {
        const unsigned int mv = powerStatus ? powerStatus->getBatteryVoltageMv() : 0;
        const unsigned int pct = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
        const auto *radio = RadioLibInterface::instance;
        snprintf(reply, sizeof(reply),
                 "%s %s\nUp:%lus Power:%umV/%u%% BLE:%u\nTX:%lu RadioRec:%lu/%lu Reset:0x%08lX",
                 FIRMWARE_LABEL, platformName(),
                 static_cast<unsigned long>(millis() / 1000UL), mv, pct,
                 static_cast<unsigned int>(Bluefruit.Central.connected()),
                 radio ? static_cast<unsigned long>(radio->txGood) : 0UL,
                 radio ? static_cast<unsigned long>(radio->radioRecoverySuccesses) : 0UL,
                 radio ? static_cast<unsigned long>(radio->radioRecoveryAttempts) : 0UL,
                 static_cast<unsigned long>(resetReasonAtBoot));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "SCAN") || isCommand(payload, payloadSize, "RECONNECT")) {
        sendTextReply(mp.from, mp.channel,
                      "BLE recovery is automatic in v2; scanner/link lifecycle is owned by the HOBO state machine.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "RECOVER") || isCommand(payload, payloadSize, "REBOOT")) {
        commandRecoveryCount++;
        if (rebootAtMsec == 0)
            rebootAtMsec = millis() + 2000UL;
        sendTextReply(mp.from, mp.channel,
                      "RECOVERY: flash-safe whole-node reboot scheduled in 2 sec; settings and logger lock preserved.");
        return ProcessMessage::CONTINUE;
    }

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
    if (millis() < BOOT_SETTLE_MS)
        return 5000;

    // Scanner and central-link lifecycle intentionally belong to the HOBO telemetry state machine.
    // This supervisor only exposes diagnostics/commands; exhausted BLE/radio recovery trips the
    // independent field watchdog channel in the owning subsystem.
    return SUPERVISOR_INTERVAL_MS;
}

#endif
