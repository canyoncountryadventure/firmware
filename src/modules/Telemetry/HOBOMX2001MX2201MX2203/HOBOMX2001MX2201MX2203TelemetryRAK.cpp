#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Reuse the hardware-proven universal HOBO protocol implementation, then add
// the RAK deployment policy: automatic HOBO reads and standard Meshtastic
// environmental telemetry broadcasts. There is intentionally no PIR/trail
// counter integration in this branch.
#include "HOBOMX2001MX2201MX2203TelemetryRAK.h"
#include "MeshService.h"
#include "NodeDB.h"
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

#ifndef CCA_HOBO_AUTO_READ_INTERVAL_MS
#define CCA_HOBO_AUTO_READ_INTERVAL_MS (60UL * 60UL * 1000UL)
#endif

#ifndef CCA_HOBO_AUTO_RETRY_MS
#define CCA_HOBO_AUTO_RETRY_MS (60UL * 1000UL)
#endif

RAKHoboAutoTelemetryModule::RAKHoboAutoTelemetryModule()
    : HOBOMX2001MX2201MX2203TelemetryModule()
{
    LOG_INFO(
        "RAK HOBO mesh: automatic reads enabled interval=%lu ms; PIR disabled",
        static_cast<unsigned long>(CCA_HOBO_AUTO_READ_INTERVAL_MS));
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

    if (!connected) {
        nextAutomaticReadMs = 0;
        automaticReadInProgress = false;
        return baseDelay;
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
            nextAutomaticReadMs = now +
                (sent ? CCA_HOBO_AUTO_READ_INTERVAL_MS : CCA_HOBO_AUTO_RETRY_MS);
            automaticReadInProgress = false;
        }
    }

    if (universalState == UniversalState::READY &&
        !readRequestPending && !readRequestInProgress) {
        if (nextAutomaticReadMs == 0)
            nextAutomaticReadMs = now + CCA_HOBO_AUTO_READ_INTERVAL_MS;

        if (reached(now, nextAutomaticReadMs)) {
            automaticReadInProgress = true;
            universalState = UniversalState::SEND_READ;
            stateDueMs = now;
            LOG_INFO("RAK HOBO mesh: automatic live read triggered");
            return 10;
        }
    }

    return baseDelay;
}

#endif
