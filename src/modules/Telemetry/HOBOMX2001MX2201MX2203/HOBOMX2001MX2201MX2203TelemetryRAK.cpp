#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Reuse the hardware-proven universal HOBO protocol implementation, then add
// the RAK deployment policy: automatic HOBO reads and standard Meshtastic
// environmental telemetry broadcasts. There is intentionally no PIR/trail
// counter integration in this branch.
#include "HOBOMX2001MX2201MX2203TelemetryRAK.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "gps/RTC.h"
#include "main.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include "pb_encode.h"
#include <bluefruit.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// The shared implementation is compile-gated by the Seeed target. For this
// RAK-only wrapper, expose it after the real RAK headers/configuration have
// already been loaded. No Seeed board configuration is imported.
#define SEEED_XIAO_NRF52840_KIT 1
#include "HOBOMX2001MX2201MX2203Telemetry.cpp"
#undef SEEED_XIAO_NRF52840_KIT

#ifndef CCA_HOBO_AUTO_RETRY_MS
#define CCA_HOBO_AUTO_RETRY_MS (60UL * 1000UL)
#endif

#ifndef CCA_HOBO_INTERVAL_QUERY_RETRY_MS
#define CCA_HOBO_INTERVAL_QUERY_RETRY_MS (10UL * 1000UL)
#endif

#ifndef CCA_HOBO_AUTO_FALLBACK_INTERVAL_MS
#define CCA_HOBO_AUTO_FALLBACK_INTERVAL_MS (60UL * 1000UL)
#endif

namespace
{

// Same Onset status request used by the earlier interval-aware MX2201 build.
// The response carries the current write pointer and logger recording interval.
static const uint8_t CMD_RAK_STATUS[] = {
    0x01, 0x01, 0x08, 0x04, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static constexpr uint32_t RAK_STATUS_TIMEOUT_MS = 3000;
static constexpr uint32_t MIN_MESH_INTERVAL_SECONDS = 60;

bool rakStatusCaptureActive = false;
bool rakStatusReady = false;
uint32_t rakLoggerWritePointer = 0;
uint16_t rakLoggerIntervalSeconds = 0;

void rakNotifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    // Status response layout, hardware-proven in the earlier HOBO build:
    // 01 02 04 05 .... [write pointer BE32] [logging interval BE16] ...
    // bytes 8..11 = write pointer; bytes 12..13 = logging interval seconds.
    if (rakStatusCaptureActive && data != nullptr && len >= 14 &&
        data[0] == 0x01 && data[1] == 0x02 &&
        data[2] == 0x04 && data[3] == 0x05) {

        rakLoggerWritePointer = readBE32(&data[8]);
        rakLoggerIntervalSeconds = static_cast<uint16_t>(
            (static_cast<uint16_t>(data[12]) << 8) |
            static_cast<uint16_t>(data[13]));

        rakStatusCaptureActive = false;
        rakStatusReady = true;

        LOG_INFO(
            "RAK HOBO mesh: logger status pointer=%lu recording interval=%u s",
            static_cast<unsigned long>(rakLoggerWritePointer),
            static_cast<unsigned int>(rakLoggerIntervalSeconds));
        return;
    }

    // Preserve all normal universal-reader notification handling.
    notifyCallback(characteristic, data, len);
}

} // namespace

RAKHoboAutoTelemetryModule::RAKHoboAutoTelemetryModule()
    : HOBOMX2001MX2201MX2203TelemetryModule()
{
    LOG_INFO(
        "RAK HOBO mesh: automatic reads follow HOBO recording interval; PIR disabled");
}

uint32_t RAKHoboAutoTelemetryModule::getAutomaticIntervalMs() const
{
    uint32_t intervalSeconds = rakLoggerIntervalSeconds;

    if (intervalSeconds == 0)
        return CCA_HOBO_AUTO_FALLBACK_INTERVAL_MS;

    // Match the earlier interval-aware HOBO build: field logging intervals of
    // 60 seconds or longer are followed exactly, while very short bench
    // intervals are prevented from flooding LoRa faster than once per minute.
    if (intervalSeconds < MIN_MESH_INTERVAL_SECONDS)
        intervalSeconds = MIN_MESH_INTERVAL_SECONDS;

    uint64_t intervalMs = static_cast<uint64_t>(intervalSeconds) * 1000ULL;
    if (intervalMs > 0xFFFFFFFFULL)
        intervalMs = 0xFFFFFFFFULL;

    return static_cast<uint32_t>(intervalMs);
}

bool RAKHoboAutoTelemetryModule::requestLoggerInterval()
{
    if (!connected || universalState != UniversalState::READY)
        return false;

    rakStatusReady = false;
    rakStatusCaptureActive = true;

    if (!sendCommand(
            CMD_RAK_STATUS,
            sizeof(CMD_RAK_STATUS),
            "STATUS/logging interval")) {
        rakStatusCaptureActive = false;
        return false;
    }

    intervalQueryInProgress = true;
    statusDeadlineMs = millis() + RAK_STATUS_TIMEOUT_MS;

    LOG_INFO("RAK HOBO mesh: determining HOBO recording interval");
    return true;
}

bool RAKHoboAutoTelemetryModule::sendEnvironmentTelemetry()
{
    if (!std::isfinite(latestTemperatureC)) {
        LOG_WARN("RAK HOBO mesh: no valid temperature to broadcast");
        return false;
    }

    meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
    telemetry.time = getTime();
    telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
    telemetry.variant.environment_metrics = meshtastic_EnvironmentMetrics_init_zero;
    telemetry.variant.environment_metrics.has_temperature = true;
    telemetry.variant.environment_metrics.temperature = latestTemperatureC;

    // Meshtastic environmental distance is millimetres. For MX2001 this
    // carries the live water-level/stage reading decoded by the HOBO bridge.
    if (loggerType == LoggerType::MX2001 && measurementHasStage &&
        std::isfinite(latestStageMeters)) {
        telemetry.variant.environment_metrics.distance = latestStageMeters * 1000.0f;
    }

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("RAK HOBO mesh: telemetry packet allocation failed");
        return false;
    }

    packet->decoded.payload.size = pb_encode_to_bytes(
        packet->decoded.payload.bytes,
        sizeof(packet->decoded.payload.bytes),
        &meshtastic_Telemetry_msg,
        &telemetry);

    if (packet->decoded.payload.size == 0) {
        LOG_WARN("RAK HOBO mesh: telemetry protobuf encode failed");
        packetPool.release(packet);
        return false;
    }

    packet->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
    packet->decoded.want_response = false;
    packet->to = NODENUM_BROADCAST;
    packet->channel = 0;
    packet->want_ack = false;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);

    if (loggerType == LoggerType::MX2001 && measurementHasStage &&
        std::isfinite(latestStageMeters)) {
        LOG_INFO(
            "RAK HOBO mesh: auto broadcast model=%s temp=%.2f C stage=%.0f mm",
            loggerTypeName(loggerType),
            latestTemperatureC,
            latestStageMeters * 1000.0f);
    } else {
        LOG_INFO(
            "RAK HOBO mesh: auto broadcast model=%s temp=%.2f C",
            loggerTypeName(loggerType),
            latestTemperatureC);
    }

    return true;
}

int32_t RAKHoboAutoTelemetryModule::runOnce()
{
    const UniversalState stateBefore = universalState;
    const bool directReadBefore = readRequestPending || readRequestInProgress;

    const int32_t baseDelay =
        HOBOMX2001MX2201MX2203TelemetryModule::runOnce();

    const uint32_t now = millis();

    // initializeClient() installs the shared callback. Replace it once with a
    // wrapper that recognizes STATUS replies, then delegates every other BLE
    // notification back to the universal reader unchanged.
    if (initialized && !statusCallbackInstalled) {
        hoboCharacteristic.setNotifyCallback(rakNotifyCallback);
        statusCallbackInstalled = true;
    }

    if (!connected) {
        nextAutomaticReadMs = 0;
        nextIntervalQueryMs = 0;
        statusDeadlineMs = 0;
        automaticReadInProgress = false;
        intervalQueryInProgress = false;
        intervalQueryNeeded = true;
        rakStatusCaptureActive = false;
        rakStatusReady = false;
        rakLoggerWritePointer = 0;
        rakLoggerIntervalSeconds = 0;
        return baseDelay;
    }

    if (intervalQueryInProgress) {
        if (rakStatusReady) {
            intervalQueryInProgress = false;
            intervalQueryNeeded = false;
            rakStatusReady = false;

            if (rakLoggerIntervalSeconds == 0) {
                LOG_WARN(
                    "RAK HOBO mesh: logger reported zero recording interval; using temporary fallback and retrying status");
                intervalQueryNeeded = true;
                nextIntervalQueryMs = now + CCA_HOBO_INTERVAL_QUERY_RETRY_MS;
            } else {
                const uint32_t intervalMs = getAutomaticIntervalMs();
                nextAutomaticReadMs = now + intervalMs;
                LOG_INFO(
                    "RAK HOBO mesh: automatic telemetry cadence=%lu ms from HOBO interval=%u s",
                    static_cast<unsigned long>(intervalMs),
                    static_cast<unsigned int>(rakLoggerIntervalSeconds));
            }
        } else if (reached(now, statusDeadlineMs)) {
            intervalQueryInProgress = false;
            intervalQueryNeeded = true;
            rakStatusCaptureActive = false;
            nextIntervalQueryMs = now + CCA_HOBO_INTERVAL_QUERY_RETRY_MS;
            LOG_WARN(
                "RAK HOBO mesh: logging-interval query timed out; will retry");
        }
    }

    // A transition from WAIT_READ to READY means a live read attempt ended.
    // Broadcast startup/automatic reads, but leave explicit DM READ replies as
    // direct replies so a manual query does not create a duplicate broadcast.
    if (stateBefore == UniversalState::WAIT_READ &&
        universalState == UniversalState::READY) {
        const bool completedInitialRead =
            !directReadBefore && !automaticReadInProgress &&
            nextAutomaticReadMs == 0;

        if (automaticReadInProgress || completedInitialRead) {
            const bool sent = sendEnvironmentTelemetry();
            automaticReadInProgress = false;

            if (sent)
                nextAutomaticReadMs = now + getAutomaticIntervalMs();
            else
                nextAutomaticReadMs = now + CCA_HOBO_AUTO_RETRY_MS;

            // Re-read STATUS after every automatic measurement so a logger
            // interval changed in HOBOconnect is picked up without reflashing.
            intervalQueryNeeded = true;
            nextIntervalQueryMs = now;
        }
    }

    if (universalState == UniversalState::READY &&
        !readRequestPending && !readRequestInProgress) {

        if (intervalQueryNeeded && !intervalQueryInProgress &&
            reached(now, nextIntervalQueryMs)) {
            if (requestLoggerInterval())
                return 10;

            nextIntervalQueryMs = now + CCA_HOBO_INTERVAL_QUERY_RETRY_MS;
        }

        if (nextAutomaticReadMs == 0)
            nextAutomaticReadMs = now + getAutomaticIntervalMs();

        if (!intervalQueryInProgress && reached(now, nextAutomaticReadMs)) {
            automaticReadInProgress = true;
            universalState = UniversalState::SEND_READ;
            stateDueMs = now;
            LOG_INFO(
                "RAK HOBO mesh: automatic live read triggered at HOBO-derived cadence");
            return 10;
        }
    }

    return baseDelay;
}

#endif
