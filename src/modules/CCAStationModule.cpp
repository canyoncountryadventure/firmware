#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "CCAStationModule.h"

#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "SPILock.h"
#include "main.h"

#include <Arduino.h>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

CCAStationModule *ccaStationModule;

namespace
{

constexpr char CCA_FW_NAME[] = "CCA-MX-PIR";
constexpr char CCA_FW_VERSION[] = "1.0.7";
constexpr uint8_t CCA_SCHEMA_VERSION = 1;
constexpr char MESHTASTIC_BASE_VERSION[] = "2.7.26";

constexpr uint8_t PIR_PIN = D6;
constexpr uint32_t POLL_INTERVAL_MS = 100;
constexpr uint32_t BOOT_ALERT_DELAY_MS = 30000;
constexpr uint32_t POWER_SAMPLE_INTERVAL_MS = 10UL * 60UL * 1000UL;
constexpr uint16_t POWER_LOW_MV = 3600;
constexpr uint16_t POWER_CRITICAL_MV = 3450;
constexpr uint16_t POWER_RECOVERY_MV = 3650;
constexpr uint16_t POWER_TREND_STABLE_MV = 30;
constexpr char STATE_FILE_PATH[] = "/prefs/cca_mx_pir.bin";
constexpr char ALERT_DEST_FILE_PATH[] = "/prefs/cca_alert_dest.bin";

constexpr uint8_t FLAG_PIR_ENABLED = 0x01;
constexpr uint8_t FLAG_PIR_TX_ENABLED = 0x02;

struct __attribute__((packed)) PersistentState
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t flags;
    uint32_t bootCount;
    uint32_t pirTotalCount;
    uint8_t checksum;
};

struct __attribute__((packed)) AlertDestinationRecord
{
    uint8_t magic[4];
    uint8_t version;
    uint32_t nodeNum;
    uint8_t checksum;
};

PersistentState persisted = {{'C', 'C', 'A', '1'}, 1, FLAG_PIR_ENABLED | FLAG_PIR_TX_ENABLED, 0, 0, 0};

bool stateLoaded = false;
bool moduleInitialized = false;
bool pirArmed = false;
bool pirLastState = false;
bool debugEnabled = false;
bool bootAlertSent = false;
bool pendingInitialPowerAlert = false;
uint32_t pirBootCount = 0;
uint32_t pirLastDetectionMs = 0;
uint32_t alertDestinationNode = 0;

struct PowerSample
{
    uint32_t atMs;
    uint16_t millivolts;
};

constexpr size_t POWER_HISTORY_CAPACITY = 145; // 24 h at 10-minute samples plus one
PowerSample powerHistory[POWER_HISTORY_CAPACITY] = {};
size_t powerHistoryCount = 0;
size_t powerHistoryNext = 0;
uint32_t lastPowerSampleMs = 0;
uint16_t powerMinMv = 0;
uint16_t powerMaxMv = 0;

enum class PowerAlertState : uint8_t
{
    UNKNOWN = 0,
    NORMAL,
    LOW,
    CRITICAL
};

PowerAlertState powerAlertState = PowerAlertState::UNKNOWN;

uint8_t checksumBytes(const uint8_t *bytes, size_t length)
{
    uint8_t value = 0;
    for (size_t i = 0; i < length; ++i)
        value ^= bytes[i];
    return value;
}

void resetPersistentDefaults()
{
    memset(&persisted, 0, sizeof(persisted));
    persisted.magic[0] = 'C';
    persisted.magic[1] = 'C';
    persisted.magic[2] = 'A';
    persisted.magic[3] = '1';
    persisted.version = 1;
    persisted.flags = FLAG_PIR_ENABLED | FLAG_PIR_TX_ENABLED;
}

bool persistentRecordValid(const PersistentState &record)
{
    if (record.magic[0] != 'C' || record.magic[1] != 'C' ||
        record.magic[2] != 'A' || record.magic[3] != '1' ||
        record.version != 1) {
        return false;
    }

    return record.checksum ==
           checksumBytes(reinterpret_cast<const uint8_t *>(&record), sizeof(record) - 1);
}

bool alertDestinationRecordValid(const AlertDestinationRecord &record)
{
    if (record.magic[0] != 'C' || record.magic[1] != 'C' ||
        record.magic[2] != 'A' || record.magic[3] != 'D' ||
        record.version != 1) {
        return false;
    }

    return record.checksum ==
           checksumBytes(reinterpret_cast<const uint8_t *>(&record), sizeof(record) - 1);
}

bool savePersistentState()
{
    persisted.checksum =
        checksumBytes(reinterpret_cast<const uint8_t *>(&persisted), sizeof(persisted) - 1);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(STATE_FILE_PATH, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("CCA: failed to open persistent state for write");
        return false;
    }

    const size_t written = file.write(
        reinterpret_cast<const uint8_t *>(&persisted), sizeof(persisted));
    file.flush();
    file.close();

    if (written != sizeof(persisted)) {
        LOG_WARN("CCA: persistent state short write");
        return false;
    }

    return true;
}

bool saveAlertDestination()
{
    AlertDestinationRecord record = {{'C', 'C', 'A', 'D'}, 1, alertDestinationNode, 0};
    record.checksum = checksumBytes(reinterpret_cast<const uint8_t *>(&record), sizeof(record) - 1);

    concurrency::LockGuard g(spiLock);
    File file = FSCom.open(ALERT_DEST_FILE_PATH, FILE_O_WRITE);
    if (!file) {
        LOG_WARN("CCA: failed to open alert destination for write");
        return false;
    }

    const size_t written = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
    file.flush();
    file.close();

    if (written != sizeof(record)) {
        LOG_WARN("CCA: alert destination short write");
        return false;
    }

    return true;
}

void loadPersistentState()
{
    resetPersistentDefaults();

    PersistentState candidate = {};
    size_t readLength = 0;

    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(STATE_FILE_PATH, FILE_O_READ);
        if (file) {
            readLength = file.read(
                reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
            file.close();
        }
    }

    if (readLength == sizeof(candidate) && persistentRecordValid(candidate)) {
        persisted = candidate;
        stateLoaded = true;
    } else {
        stateLoaded = false;
    }

    persisted.bootCount++;
    savePersistentState();
}

void loadAlertDestination()
{
    alertDestinationNode = 0;

    AlertDestinationRecord candidate = {};
    size_t readLength = 0;

    {
        concurrency::LockGuard g(spiLock);
        File file = FSCom.open(ALERT_DEST_FILE_PATH, FILE_O_READ);
        if (file) {
            readLength = file.read(reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
            file.close();
        }
    }

    if (readLength == sizeof(candidate) && alertDestinationRecordValid(candidate))
        alertDestinationNode = candidate.nodeNum;
}

bool pirEnabled()
{
    return (persisted.flags & FLAG_PIR_ENABLED) != 0;
}

bool pirTxEnabled()
{
    return (persisted.flags & FLAG_PIR_TX_ENABLED) != 0;
}

void setPirEnabled(bool enabled)
{
    if (enabled)
        persisted.flags |= FLAG_PIR_ENABLED;
    else
        persisted.flags &= static_cast<uint8_t>(~FLAG_PIR_ENABLED);
    savePersistentState();
}

void setPirTxEnabled(bool enabled)
{
    if (enabled)
        persisted.flags |= FLAG_PIR_TX_ENABLED;
    else
        persisted.flags &= static_cast<uint8_t>(~FLAG_PIR_TX_ENABLED);
    savePersistentState();
}

uint16_t currentBatteryMv()
{
    if (powerStatus == nullptr)
        return 0;

    const uint32_t mv = powerStatus->getBatteryVoltageMv();
    return mv > 65535U ? 65535U : static_cast<uint16_t>(mv);
}

uint8_t currentBatteryPercent()
{
    if (powerStatus == nullptr || !powerStatus->getHasBattery())
        return 0;
    return powerStatus->getBatteryChargePercent();
}

const char *chargingText()
{
    if (powerStatus == nullptr)
        return "UNKNOWN";
    return powerStatus->getIsCharging() ? "YES" : "NO";
}

uint32_t uptimeSeconds()
{
    return millis() / 1000UL;
}

void formatDuration(uint32_t seconds, char *out, size_t outSize)
{
    const uint32_t days = seconds / 86400UL;
    seconds %= 86400UL;
    const uint32_t hours = seconds / 3600UL;
    seconds %= 3600UL;
    const uint32_t minutes = seconds / 60UL;

    if (days > 0)
        snprintf(out, outSize, "%lud %luh %lum", (unsigned long)days, (unsigned long)hours, (unsigned long)minutes);
    else if (hours > 0)
        snprintf(out, outSize, "%luh %lum", (unsigned long)hours, (unsigned long)minutes);
    else
        snprintf(out, outSize, "%lum", (unsigned long)minutes);
}

void normalizeCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize)
{
    if (outSize == 0)
        return;

    out[0] = '\0';
    if (bytes == nullptr || size == 0)
        return;

    size_t begin = 0;
    while (begin < size && std::isspace(static_cast<unsigned char>(bytes[begin])))
        ++begin;
    if (begin < size && bytes[begin] == '/')
        ++begin;

    size_t end = size;
    while (end > begin && std::isspace(static_cast<unsigned char>(bytes[end - 1])))
        --end;

    size_t j = 0;
    bool previousSpace = false;
    for (size_t i = begin; i < end && j + 1 < outSize; ++i) {
        unsigned char c = bytes[i];
        if (std::isspace(c)) {
            if (!previousSpace && j > 0) {
                out[j++] = ' ';
                previousSpace = true;
            }
        } else {
            out[j++] = static_cast<char>(std::toupper(c));
            previousSpace = false;
        }
    }

    if (j > 0 && out[j - 1] == ' ')
        --j;
    out[j] = '\0';
}

void recordPowerSample(uint32_t now, uint16_t mv)
{
    if (mv == 0)
        return;

    powerHistory[powerHistoryNext] = {now, mv};
    powerHistoryNext = (powerHistoryNext + 1) % POWER_HISTORY_CAPACITY;
    if (powerHistoryCount < POWER_HISTORY_CAPACITY)
        powerHistoryCount++;

    if (powerMinMv == 0 || mv < powerMinMv)
        powerMinMv = mv;
    if (mv > powerMaxMv)
        powerMaxMv = mv;

    lastPowerSampleMs = now;
}

bool voltageAtAge(uint32_t now, uint32_t targetAgeMs, uint16_t &result)
{
    if (powerHistoryCount == 0)
        return false;

    uint32_t bestDifference = UINT32_MAX;
    uint16_t bestVoltage = 0;

    for (size_t i = 0; i < powerHistoryCount; ++i) {
        const uint32_t age = now - powerHistory[i].atMs;
        const uint32_t difference =
            age > targetAgeMs ? age - targetAgeMs : targetAgeMs - age;
        if (difference < bestDifference) {
            bestDifference = difference;
            bestVoltage = powerHistory[i].millivolts;
        }
    }

    // Do not pretend a young node has a 6 h or 24 h reference point.
    const uint32_t tolerance = POWER_SAMPLE_INTERVAL_MS * 2UL;
    if (bestDifference > tolerance)
        return false;

    result = bestVoltage;
    return true;
}

const char *trendFromDelta(int32_t deltaMv)
{
    if (deltaMv > static_cast<int32_t>(POWER_TREND_STABLE_MV))
        return "RISING";
    if (deltaMv < -static_cast<int32_t>(POWER_TREND_STABLE_MV))
        return "FALLING";
    return "STABLE";
}

PowerAlertState classifyPowerState(uint16_t mv)
{
    if (mv == 0)
        return PowerAlertState::UNKNOWN;
    if (mv < POWER_CRITICAL_MV)
        return PowerAlertState::CRITICAL;
    if (mv < POWER_LOW_MV)
        return PowerAlertState::LOW;
    return PowerAlertState::NORMAL;
}

} // namespace

CCAStationModule::CCAStationModule()
    : SinglePortModule("CCAStation", meshtastic_PortNum_PRIVATE_APP),
      concurrency::OSThread("CCAStation")
{
    isPromiscuous = true;
    setIntervalFromNow(500);
}

bool CCAStationModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

bool CCAStationModule::sendText(uint32_t destination, uint8_t channel, const char *text, bool wantAck)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("CCA: text packet allocation failed");
        return false;
    }

    size_t length = strlen(text);
    if (length > sizeof(packet->decoded.payload.bytes))
        length = sizeof(packet->decoded.payload.bytes);

    memcpy(packet->decoded.payload.bytes, text, length);
    packet->decoded.payload.size = length;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = wantAck;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

bool CCAStationModule::sendAutomaticAlert(const char *text)
{
    if (alertDestinationNode == 0) {
        LOG_WARN("CCA alert suppressed: no private destination; DM ALERTS HERE from the receiver radio");
        return false;
    }

    return sendText(alertDestinationNode, 0, text, true);
}

ProcessMessage CCAStationModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP || nodeDB == nullptr)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    char command[48] = {};
    normalizeCommand(mp.decoded.payload.bytes, mp.decoded.payload.size, command, sizeof(command));
    if (command[0] == '\0')
        return ProcessMessage::CONTINUE;

    const uint32_t now = millis();
    const uint16_t mv = currentBatteryMv();
    const uint8_t pct = currentBatteryPercent();
    char reply[230] = {};

    if (strcmp(command, "VERSION") == 0) {
        snprintf(reply, sizeof(reply),
                 "FW: %s %s\nSchema: %u\nMeshtastic: %s\nBuild: SEEED XIAO + Wio-SX1262",
                 CCA_FW_NAME, CCA_FW_VERSION, CCA_SCHEMA_VERSION, MESHTASTIC_BASE_VERSION);
    } else if (strcmp(command, "STATUS") == 0) {
        char up[32] = {};
        formatDuration(uptimeSeconds(), up, sizeof(up));
        char alertDest[28] = "NOT SET";
        if (alertDestinationNode != 0)
            snprintf(alertDest, sizeof(alertDest), "!%08lx", (unsigned long)alertDestinationNode);
        snprintf(reply, sizeof(reply),
                 "%s %s\nUptime: %s\nBattery: %.3f V / %u%%\nCharging: %s\nPIR: %s | TX %s\nPIR Total: %lu | Boot: %lu\nAlerts: PRIVATE %s\nHOBO: use LOGGER",
                 CCA_FW_NAME, CCA_FW_VERSION, up, mv / 1000.0f, pct, chargingText(),
                 pirEnabled() ? "ON" : "OFF", pirTxEnabled() ? "ON" : "OFF",
                 (unsigned long)persisted.pirTotalCount, (unsigned long)pirBootCount, alertDest);
    } else if (strcmp(command, "ALERTS HERE") == 0) {
        if (alertDestinationNode == 0 || alertDestinationNode == mp.from) {
            alertDestinationNode = mp.from;
            const bool saved = saveAlertDestination();
            snprintf(reply, sizeof(reply),
                     "ALERTS PRIVATE\nDestination: !%08lx\nSaved: %s\nPIR/POWER/BOOT will DM this node",
                     (unsigned long)alertDestinationNode, saved ? "YES" : "NO");
        } else {
            snprintf(reply, sizeof(reply),
                     "ALERT DEST LOCKED\nCurrent: !%08lx\nClear it from the current receiver first",
                     (unsigned long)alertDestinationNode);
        }
    } else if (strcmp(command, "ALERTS STATUS") == 0) {
        if (alertDestinationNode == 0)
            snprintf(reply, sizeof(reply), "Alerts: PRIVATE\nDestination: NOT SET\nSend ALERTS HERE from receiver radio");
        else
            snprintf(reply, sizeof(reply), "Alerts: PRIVATE DM ONLY\nDestination: !%08lx\nPublic fallback: DISABLED",
                     (unsigned long)alertDestinationNode);
    } else if (strcmp(command, "ALERTS CLEAR") == 0) {
        if (alertDestinationNode == 0) {
            snprintf(reply, sizeof(reply), "ALERT DESTINATION ALREADY CLEAR");
        } else if (mp.from != alertDestinationNode) {
            snprintf(reply, sizeof(reply), "ALERTS CLEAR DENIED\nOnly current destination !%08lx may clear it",
                     (unsigned long)alertDestinationNode);
        } else {
            alertDestinationNode = 0;
            const bool saved = saveAlertDestination();
            snprintf(reply, sizeof(reply), "ALERT DESTINATION CLEARED\nSaved: %s\nAutomatic CCA alerts now suppressed", saved ? "YES" : "NO");
        }
    } else if (strcmp(command, "PIR") == 0 || strcmp(command, "PIR STATUS") == 0) {
        char last[40] = "never";
        if (pirLastDetectionMs != 0)
            formatDuration((now - pirLastDetectionMs) / 1000UL, last, sizeof(last));
        char alertDest[28] = "NOT SET";
        if (alertDestinationNode != 0)
            snprintf(alertDest, sizeof(alertDest), "!%08lx", (unsigned long)alertDestinationNode);
        snprintf(reply, sizeof(reply),
                 "PIR: %s\nSensor: %s\nTotal Detections: %lu\nSince Boot: %lu\nLast Detection: %s\nTX Alerts: %s\nAlert DM: %s\nPin: D6",
                 pirEnabled() ? "ON" : "OFF", pirLastState ? "MOTION" : "CLEAR",
                 (unsigned long)persisted.pirTotalCount, (unsigned long)pirBootCount,
                 last, pirTxEnabled() ? "ON" : "OFF", alertDest);
    } else if (strcmp(command, "PIR COUNT") == 0) {
        snprintf(reply, sizeof(reply), "PIR Total: %lu\nSince Boot: %lu",
                 (unsigned long)persisted.pirTotalCount, (unsigned long)pirBootCount);
    } else if (strcmp(command, "PIR LAST") == 0) {
        if (pirLastDetectionMs == 0) {
            snprintf(reply, sizeof(reply), "PIR Last: never since boot");
        } else {
            char age[40] = {};
            formatDuration((now - pirLastDetectionMs) / 1000UL, age, sizeof(age));
            snprintf(reply, sizeof(reply), "PIR Last: %s ago", age);
        }
    } else if (strcmp(command, "PIR RESET") == 0) {
        persisted.pirTotalCount = 0;
        pirBootCount = 0;
        pirLastDetectionMs = 0;
        savePersistentState();
        snprintf(reply, sizeof(reply), "PIR COUNT RESET\nTotal: 0\nSince Boot: 0");
    } else if (strcmp(command, "PIR ON") == 0) {
        setPirEnabled(true);
        pirArmed = !pirLastState;
        snprintf(reply, sizeof(reply), "PIR ON\nMonitoring D6\nTX Alerts: %s", pirTxEnabled() ? "ON" : "OFF");
    } else if (strcmp(command, "PIR OFF") == 0) {
        setPirEnabled(false);
        snprintf(reply, sizeof(reply), "PIR OFF\nMonitoring disabled\nSetting persists after reboot");
    } else if (strcmp(command, "PIR TX ON") == 0) {
        setPirTxEnabled(true);
        if (alertDestinationNode == 0)
            snprintf(reply, sizeof(reply), "PIR TX ON\nPrivate destination NOT SET\nSend ALERTS HERE from receiver radio");
        else
            snprintf(reply, sizeof(reply), "PIR TX ON\nNew detections DM !%08lx immediately",
                     (unsigned long)alertDestinationNode);
    } else if (strcmp(command, "PIR TX OFF") == 0) {
        setPirTxEnabled(false);
        snprintf(reply, sizeof(reply), "PIR TX OFF\nDetections still count locally; alerts are silent");
    } else if (strcmp(command, "POWER") == 0 || strcmp(command, "POWER STATUS") == 0) {
        uint16_t sixHour = 0;
        const bool haveSix = voltageAtAge(now, 6UL * 60UL * 60UL * 1000UL, sixHour);
        const int32_t delta = haveSix ? static_cast<int32_t>(mv) - sixHour : 0;
        snprintf(reply, sizeof(reply),
                 "Battery: %.3f V / %u%%\nCharging: %s\nTrend: %s%s\nMin: %.3f V\nMax: %.3f V\nSamples: %u",
                 mv / 1000.0f, pct, chargingText(),
                 haveSix ? trendFromDelta(delta) : "LEARNING",
                 haveSix ? " (6h)" : "",
                 powerMinMv / 1000.0f, powerMaxMv / 1000.0f,
                 (unsigned)powerHistoryCount);
    } else if (strcmp(command, "POWER VOLTAGE") == 0) {
        snprintf(reply, sizeof(reply), "Battery: %.3f V\nMesh estimate: %u%%\nCharging: %s",
                 mv / 1000.0f, pct, chargingText());
    } else if (strcmp(command, "POWER MINMAX") == 0) {
        snprintf(reply, sizeof(reply), "Since Boot\nMin: %.3f V\nMax: %.3f V\nNow: %.3f V",
                 powerMinMv / 1000.0f, powerMaxMv / 1000.0f, mv / 1000.0f);
    } else if (strcmp(command, "POWER TREND") == 0) {
        uint16_t oneHour = 0, sixHour = 0, day = 0;
        const bool have1 = voltageAtAge(now, 60UL * 60UL * 1000UL, oneHour);
        const bool have6 = voltageAtAge(now, 6UL * 60UL * 60UL * 1000UL, sixHour);
        const bool have24 = voltageAtAge(now, 24UL * 60UL * 60UL * 1000UL, day);
        snprintf(reply, sizeof(reply),
                 "Now: %.3f V\n1h: %s%.3f\n6h: %s%.3f\n24h: %s%.3f\nTrend: %s",
                 mv / 1000.0f,
                 have1 ? "" : "-- ", have1 ? oneHour / 1000.0f : 0.0f,
                 have6 ? "" : "-- ", have6 ? sixHour / 1000.0f : 0.0f,
                 have24 ? "" : "-- ", have24 ? day / 1000.0f : 0.0f,
                 have6 ? trendFromDelta(static_cast<int32_t>(mv) - sixHour) : "LEARNING");
    } else if (strcmp(command, "POWER HISTORY") == 0) {
        uint16_t oneHour = 0, sixHour = 0, twelveHour = 0, day = 0;
        const bool have1 = voltageAtAge(now, 60UL * 60UL * 1000UL, oneHour);
        const bool have6 = voltageAtAge(now, 6UL * 60UL * 60UL * 1000UL, sixHour);
        const bool have12 = voltageAtAge(now, 12UL * 60UL * 60UL * 1000UL, twelveHour);
        const bool have24 = voltageAtAge(now, 24UL * 60UL * 60UL * 1000UL, day);
        snprintf(reply, sizeof(reply),
                 "Battery History\nNow: %.3f V\n1h: %s%.3f\n6h: %s%.3f\n12h: %s%.3f\n24h: %s%.3f\nMin/Max: %.3f / %.3f V",
                 mv / 1000.0f,
                 have1 ? "" : "-- ", have1 ? oneHour / 1000.0f : 0.0f,
                 have6 ? "" : "-- ", have6 ? sixHour / 1000.0f : 0.0f,
                 have12 ? "" : "-- ", have12 ? twelveHour / 1000.0f : 0.0f,
                 have24 ? "" : "-- ", have24 ? day / 1000.0f : 0.0f,
                 powerMinMv / 1000.0f, powerMaxMv / 1000.0f);
    } else if (strcmp(command, "POWER RESET") == 0) {
        memset(powerHistory, 0, sizeof(powerHistory));
        powerHistoryCount = 0;
        powerHistoryNext = 0;
        powerMinMv = mv;
        powerMaxMv = mv;
        recordPowerSample(now, mv);
        snprintf(reply, sizeof(reply), "POWER HISTORY RESET\nNow: %.3f V", mv / 1000.0f);
    } else if (strcmp(command, "POWER UPTIME") == 0 || strcmp(command, "UPTIME") == 0) {
        char up[40] = {};
        formatDuration(uptimeSeconds(), up, sizeof(up));
        snprintf(reply, sizeof(reply), "Uptime: %s\nBoot Count: %lu", up, (unsigned long)persisted.bootCount);
    } else if (strcmp(command, "BOOT") == 0) {
        snprintf(reply, sizeof(reply), "Boot Count: %lu\nFW: %s %s",
                 (unsigned long)persisted.bootCount, CCA_FW_NAME, CCA_FW_VERSION);
    } else if (strcmp(command, "DEBUG ON") == 0) {
        debugEnabled = true;
        snprintf(reply, sizeof(reply), "CCA DEBUG ON\nVerbose CCA serial diagnostics enabled until reboot");
    } else if (strcmp(command, "DEBUG OFF") == 0) {
        debugEnabled = false;
        snprintf(reply, sizeof(reply), "CCA DEBUG OFF");
    } else {
        return ProcessMessage::CONTINUE;
    }

    sendText(mp.from, mp.channel, reply, true);
    return ProcessMessage::CONTINUE;
}

int32_t CCAStationModule::runOnce()
{
    const uint32_t now = millis();

    if (!moduleInitialized) {
        loadPersistentState();
        loadAlertDestination();

        pinMode(PIR_PIN, INPUT);
        pirLastState = digitalRead(PIR_PIN);
        pirArmed = !pirLastState;

        const uint16_t mv = currentBatteryMv();
        recordPowerSample(now, mv);
        powerAlertState = classifyPowerState(mv);
        pendingInitialPowerAlert =
            powerAlertState == PowerAlertState::LOW || powerAlertState == PowerAlertState::CRITICAL;

        moduleInitialized = true;
        LOG_INFO("CCA-MX-PIR %s: D6 PIR, schema %u, persistent=%s", CCA_FW_VERSION,
                 CCA_SCHEMA_VERSION, stateLoaded ? "restored" : "new");
        LOG_INFO("CCA PIR: %s TX=%s startup=%s", pirEnabled() ? "ON" : "OFF",
                 pirTxEnabled() ? "ON" : "OFF", pirArmed ? "ARMED" : "WAITING-FOR-LOW");
        if (alertDestinationNode == 0)
            LOG_INFO("CCA automatic alerts: PRIVATE destination NOT SET; broadcasts disabled");
        else
            LOG_INFO("CCA automatic alerts: PRIVATE DM destination !%08lx", (unsigned long)alertDestinationNode);
        return POLL_INTERVAL_MS;
    }

    const bool state = digitalRead(PIR_PIN);

    if (!pirEnabled()) {
        pirLastState = state;
        pirArmed = !state;
    } else if (!pirArmed) {
        if (!state) {
            pirArmed = true;
            pirLastState = false;
            if (debugEnabled)
                LOG_INFO("CCA PIR: startup/high condition cleared; armed");
        } else {
            pirLastState = true;
        }
    } else {
        if (state && !pirLastState) {
            persisted.pirTotalCount++;
            pirBootCount++;
            pirLastDetectionMs = now;
            savePersistentState();

            LOG_INFO("CCA PIR ALERT total=%lu boot=%lu", (unsigned long)persisted.pirTotalCount,
                     (unsigned long)pirBootCount);

            if (pirTxEnabled()) {
                char alert[80] = {};
                snprintf(alert, sizeof(alert), "PIR|ALERT|COUNT=%lu", (unsigned long)persisted.pirTotalCount);
                sendAutomaticAlert(alert);
            }
        }

        if (!state && pirLastState && debugEnabled)
            LOG_INFO("CCA PIR: LOW; rearmed for next LOW->HIGH event");

        pirLastState = state;
    }

    if ((lastPowerSampleMs == 0) || (now - lastPowerSampleMs >= POWER_SAMPLE_INTERVAL_MS)) {
        const uint16_t mv = currentBatteryMv();
        recordPowerSample(now, mv);

        PowerAlertState newState = classifyPowerState(mv);
        if (powerAlertState == PowerAlertState::CRITICAL && mv < POWER_RECOVERY_MV && newState == PowerAlertState::NORMAL)
            newState = PowerAlertState::LOW;
        if (powerAlertState == PowerAlertState::LOW && mv < POWER_RECOVERY_MV && newState == PowerAlertState::NORMAL)
            newState = PowerAlertState::LOW;

        if (newState != PowerAlertState::UNKNOWN && newState != powerAlertState && now >= BOOT_ALERT_DELAY_MS) {
            char alert[80] = {};
            if (newState == PowerAlertState::CRITICAL)
                snprintf(alert, sizeof(alert), "POWER|CRITICAL|V=%.3f", mv / 1000.0f);
            else if (newState == PowerAlertState::LOW)
                snprintf(alert, sizeof(alert), "POWER|LOW|V=%.3f", mv / 1000.0f);
            else
                snprintf(alert, sizeof(alert), "POWER|RECOVERED|V=%.3f", mv / 1000.0f);

            sendAutomaticAlert(alert);
            powerAlertState = newState;
        } else if (newState != PowerAlertState::UNKNOWN) {
            powerAlertState = newState;
        }

        if (debugEnabled)
            LOG_INFO("CCA POWER sample=%u mV min=%u max=%u charging=%s", mv, powerMinMv, powerMaxMv, chargingText());
    }

    if (!bootAlertSent && now >= BOOT_ALERT_DELAY_MS) {
        char boot[100] = {};
        snprintf(boot, sizeof(boot), "SYS|BOOT|COUNT=%lu|FW=%s-%s",
                 (unsigned long)persisted.bootCount, CCA_FW_NAME, CCA_FW_VERSION);
        sendAutomaticAlert(boot);
        bootAlertSent = true;

        if (pendingInitialPowerAlert) {
            const uint16_t mv = currentBatteryMv();
            char alert[80] = {};
            if (powerAlertState == PowerAlertState::CRITICAL)
                snprintf(alert, sizeof(alert), "POWER|CRITICAL|V=%.3f", mv / 1000.0f);
            else
                snprintf(alert, sizeof(alert), "POWER|LOW|V=%.3f", mv / 1000.0f);
            sendAutomaticAlert(alert);
            pendingInitialPowerAlert = false;
        }
    }

    return POLL_INTERVAL_MS;
}

#endif // CCA_MX_PIR + Seeed XIAO nRF52840
