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
static constexpr char FIRMWARE_LABEL[] = "RAK WATER DISTANCE + HOBO V6";

uint32_t commandRecoveryCount = 0;
uint32_t resetReasonAtBoot = 0;
uint32_t batteryMinMv = UINT32_MAX;
uint32_t batteryMaxMv = 0;
uint32_t batteryFirstMv = 0;
uint32_t batteryLastMv = 0;
uint32_t batterySampleCount = 0;
uint32_t batteryLastSampleMs = 0;
uint32_t batteryLastChargeTransitions = 0;
bool lastCharging = false;

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
        if (std::toupper(static_cast<unsigned char>(p[i])) != std::toupper(static_cast<unsigned char>(expected[i])))
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
    : SinglePortModule("hobo_self_recovery", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("hobo_self_recovery")
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
        // Split the complete command reference into LoRa-safe messages so no
        // command is truncated.
        sendTextReply(mp.from, mp.channel,
                      "HELP 1/8 HOBO: HELP | READ | LOGGER | LOCK | LOCK <MAC> | "
                      "UNLOCK | AUTO | STATUS | HEALTH | BLE");
        sendTextReply(mp.from, mp.channel,
                      "HELP 2/8 SYSTEM: POWER | BATTERY | STATS | DIAG | CLEAR "
                      "DIAG | NODES | UPTIME | VERSION | WATCHDOG | PING | WAKE | "
                      "SCAN | RECONNECT | RECOVER | REBOOT | SOFT REBOOT | DFU");
        sendTextReply(mp.from, mp.channel,
                      "HELP 3/8 WATER CORE: WATER | WATER HELP | WATER STATUS | WATER CHECK "
                      "| WATER INSTALL | WATER READ | WATER RAW | WATER VERIFY");
        sendTextReply(mp.from, mp.channel,
                      "HELP 4/8 WATER SENSOR: WATER SENSOR | WATER SENSOR AUTO | WATER "
                      "SENSOR SEN0590 | WATER SENSOR SEN0311 | WATER SENSOR A02YYUW");
        sendTextReply(mp.from, mp.channel,
                      "HELP 5/8 WATER SENSOR/CAL: WATER SENSOR SEN0313 | WATER SENSOR "
                      "A01NYUB | WATER CAL STATUS | WATER CAL STAGE 1.42FT | WATER CAL LOCK");
        sendTextReply(mp.from, mp.channel,
                      "HELP 6/8 WATER CONFIG: WATER INTERVAL 1H | WATER CAL UNLOCK "
                      "| WATER CAL UNLOCK CONFIRM | WATER CAL RESET | WATER CAL "
                      "RESET CONFIRM | WATER RESET WATER | WATER RESET WATER "
                      "CONFIRM | WATER TELEMETRY NOW | WATER MODE | SAMPLES 10");
        sendTextReply(mp.from, mp.channel,
                      "HELP 7/8 V6: HUB | HUB SET !NODEID | HUB AUTO | SAMPLES 3-20 | BATTERY HISTORY | "
                      "RADIO RESET | HOBO RESTART | SENSOR RESTART | LAST RESORT");
        sendTextReply(mp.from, mp.channel,
                      "HELP 8/8 RELAY: RELAY SHOW | RELAY PREFER !NODEID | RELAY ONCE !NODEID | "
                      "RELAY AUTO (broadcast with 25s fallback)");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "PING") || isCommand(payload, payloadSize, "WAKE")) {
        snprintf(reply, sizeof(reply), "PONG %s uptime=%lus", platformName(), static_cast<unsigned long>(millis() / 1000UL));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "VERSION")) {
        snprintf(reply, sizeof(reply),
                 "%s\nFW:%s\nHW:%s\nTarget:%s\nHOBO poll:30s | DFU:ON",
                 FIRMWARE_LABEL, optstr(APP_VERSION), platformName(),
                 optstr(APP_ENV));
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
                     powerStatus->getHasBattery() ? "YES" : "NO", powerStatus->getIsCharging() ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply), "POWER: status unavailable");
        }
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "BATTERY HISTORY")) {
        if (batterySampleCount == 0) {
            sendTextReply(mp.from, mp.channel, "BATTERY HISTORY: no voltage samples yet");
        } else {
            snprintf(reply, sizeof(reply),
                     "BATTERY HISTORY (this boot) n=%lu\nfirst:%lumV last:%lumV min:%lumV max:%lumV charge_edges:%lu",
                     static_cast<unsigned long>(batterySampleCount),
                     static_cast<unsigned long>(batteryFirstMv),
                     static_cast<unsigned long>(batteryLastMv),
                     static_cast<unsigned long>(batteryMinMv),
                     static_cast<unsigned long>(batteryMaxMv),
                     static_cast<unsigned long>(batteryLastChargeTransitions));
            sendTextReply(mp.from, mp.channel, reply);
        }
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
                      "AUTO: ON. STATUS every 30s; TX on new logger record only. "
                      "BLE failures self-recover.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "WATCHDOG")) {
        snprintf(reply, sizeof(reply), "WATCHDOG: core=90s field-channel=%s run-in-sleep=YES",
                 nrf52FieldWatchdogIsArmed() ? "ARMED" : "OFF");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "DIAG") || isCommand(payload, payloadSize, "CRASHLOG")) {
        nrf52FieldDiagFormat(reply, sizeof(reply));
        sendTextReply(mp.from, mp.channel, reply);
        const auto *radio = RadioLibInterface::instance;
        snprintf(reply, sizeof(reply),
                 "DIAG 2/2 %s: Up:%lus\nRadio recovery:%lu/%lu TX:%lu\nBattery:%umV %u%% BLE:%u WDT:%s",
                 platformName(), static_cast<unsigned long>(millis() / 1000UL),
                 radio ? static_cast<unsigned long>(radio->radioRecoverySuccesses) : 0UL,
                 radio ? static_cast<unsigned long>(radio->radioRecoveryAttempts) : 0UL,
                 radio ? static_cast<unsigned long>(radio->txGood) : 0UL,
                 powerStatus ? powerStatus->getBatteryVoltageMv() : 0U,
                 powerStatus ? powerStatus->getBatteryChargePercent() : 0U,
                 static_cast<unsigned int>(Bluefruit.Central.connected()),
                 nrf52FieldWatchdogIsArmed() ? "ON" : "OFF");
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "CLEAR DIAG")) {
        nrf52FieldDiagClear();
        sendTextReply(mp.from, mp.channel, "DIAG cleared.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "NODES")) {
        snprintf(reply, sizeof(reply), "NODES: %u mesh nodes", static_cast<unsigned int>(nodeDB->getNumMeshNodes()));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATS")) {
        const auto *radio = RadioLibInterface::instance;
        snprintf(reply, sizeof(reply),
                 "STATS: radio_recover=%lu/%lu tx=%lu last_tx=%lus cmd_reboots=%lu "
                 "reset=0x%08lX hobo_wake=%u hobo_hf=%u hf_rc=%lu",
                 radio ? static_cast<unsigned long>(radio->radioRecoverySuccesses) : 0UL,
                 radio ? static_cast<unsigned long>(radio->radioRecoveryAttempts) : 0UL,
                 radio ? static_cast<unsigned long>(radio->txGood) : 0UL,
                 radio && radio->lastTxCompleteMs ? static_cast<unsigned long>((millis() - radio->lastTxCompleteMs) / 1000UL)
                                                  : 0UL,
                 static_cast<unsigned long>(commandRecoveryCount), static_cast<unsigned long>(resetReasonAtBoot),
                 nrf52HoboHandshakeAwake() ? 1U : 0U, nrf52HoboHandshakeHfclkRunning() ? 1U : 0U,
                 static_cast<unsigned long>(nrf52HoboHandshakeHfclkResult()));
        sendTextReply(mp.from, mp.channel, reply);
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "STATUS") || isCommand(payload, payloadSize, "HEALTH")) {
        const unsigned int mv = powerStatus ? powerStatus->getBatteryVoltageMv() : 0;
        const unsigned int pct = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
        const auto *radio = RadioLibInterface::instance;
        snprintf(reply, sizeof(reply),
                 "%s %s\nUp:%lus Power:%umV/%u%% BLE:%u\nTX:%lu RadioRec:%lu/%lu "
                 "Reset:0x%08lX",
                 FIRMWARE_LABEL, platformName(), static_cast<unsigned long>(millis() / 1000UL), mv, pct,
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
                      "BLE recovery is automatic in V5; scanner/link lifecycle is "
                      "owned by the HOBO state machine.");
        return ProcessMessage::CONTINUE;
    }

    // Preserve the original fast software-reset path for an explicit command.
    // DFU and internal update resets remain handled by their existing routines.
    if (isCommand(payload, payloadSize, "RADIO RESET")) {
        if (!nrf52MarkRebootPacket(mp.from, mp.id))
            return ProcessMessage::CONTINUE;
        commandRecoveryCount++;
        if (rebootAtMsec == 0)
            rebootAtMsec = millis() + 3000UL;
        sendTextReply(mp.from, mp.channel, "RADIO RESET: reboot scheduled; mesh radio state refreshed, configuration retained");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "LAST RESORT")) {
        // This is an intentionally one-step destructive emergency recovery,
        // not a request/confirmation handshake. The operator may never hear
        // this reply after the node's channels, bonds and keys are erased.
        if (!nrf52MarkRebootPacket(mp.from, mp.id))
            return ProcessMessage::CONTINUE;
        sendTextReply(mp.from, mp.channel,
                      "LAST RESORT executing: erase settings + bonds; US LongFast slot20; reboot");
        nrf52FieldDiagEvent(4, 4, 0);
        if (!nodeDB->factoryReset(true)) {
            LOG_ERROR("LAST RESORT factory reset failed; no reboot scheduled");
            return ProcessMessage::CONTINUE;
        }
        config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_US;
        config.lora.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
        config.lora.channel_num = 20;
        config.lora.hop_limit = 7;
        if (!nodeDB->saveToDisk()) {
            LOG_ERROR("LAST RESORT: US/LongFast/slot20 configuration save failed");
            // Do not reboot to an unset region and strand the remote node.
            return ProcessMessage::CONTINUE;
        }
        commandRecoveryCount++;
        rebootAtMsec = millis() + 3000UL;
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "SOFT REBOOT")) {
        if (!nrf52MarkRebootPacket(mp.from, mp.id))
            return ProcessMessage::CONTINUE;
        commandRecoveryCount++;
        if (rebootAtMsec == 0)
            rebootAtMsec = millis() + 3000UL;
        sendTextReply(mp.from, mp.channel,
                      "SOFT REBOOT: CPU reset in 3 sec; flash/settings preserved.");
        return ProcessMessage::CONTINUE;
    }

    if (isCommand(payload, payloadSize, "RECOVER") || isCommand(payload, payloadSize, "REBOOT")) {
        // Mesh ACK/retries can replay a command just after reboot. RAM in .noinit
        // retains this fingerprint across a CPU or watchdog reset.
        if (!nrf52MarkRebootPacket(mp.from, mp.id))
            return ProcessMessage::CONTINUE;
        if (!nrf52FieldWatchdogIsArmed()) {
            sendTextReply(mp.from, mp.channel, "REBOOT: hardware watchdog unavailable; no reset scheduled.");
            return ProcessMessage::CONTINUE;
        }
        if (nrf52ScheduleWatchdogReset(3000UL)) {
            commandRecoveryCount++;
            sendTextReply(mp.from, mp.channel,
                          "REBOOT: watchdog hardware reset armed in 3 sec; "
                          "WDT may take up to 90 sec. Settings/calibration saved.");
        } else {
            sendTextReply(mp.from, mp.channel, "REBOOT: watchdog reset already scheduled.");
        }
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

    // Hourly in-RAM trend; no flash wear and no invented battery-temperature
    // measurements. History starts from boot and resets after power loss.
    if (powerStatus && (batterySampleCount == 0 || millis() - batteryLastSampleMs >= 3600000UL)) {
        const uint32_t mv = powerStatus->getBatteryVoltageMv();
        if (mv > 0) {
            if (batterySampleCount == 0) {
                batteryFirstMv = mv;
                lastCharging = powerStatus->getIsCharging();
            } else if (lastCharging != powerStatus->getIsCharging()) {
                batteryLastChargeTransitions++;
                lastCharging = powerStatus->getIsCharging();
            }
            if (mv < batteryMinMv)
                batteryMinMv = mv;
            if (mv > batteryMaxMv)
                batteryMaxMv = mv;
            batteryLastMv = mv;
            batterySampleCount++;
            batteryLastSampleMs = millis();
        }
    }
    return SUPERVISOR_INTERVAL_MS;
}

#endif
