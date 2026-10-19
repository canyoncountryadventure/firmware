#include "HoboHttpGatewayModule.h"
#include "CcaCc01DmParser.h"

#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED

#include "MeshService.h"
#include "Router.h"
#include "NodeDB.h"
#include "main.h"
#include "mesh/Throttle.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t CC01_POLL_NODE = 1695962694UL; // !65165646
}

bool HoboHttpGatewayModule::sendCc01Dm(const char *command)
{
    if (command == nullptr || nodeDB == nullptr || router == nullptr || service == nullptr)
        return false;
    const size_t len = strlen(command);
    if (len == 0 || len > 240)
        return false;

    meshtastic_MeshPacket *packet = router->allocForSending();
    if (!packet)
        return false;
    memcpy(packet->decoded.payload.bytes, command, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = CC01_POLL_NODE;
    packet->channel = CC01_LONGFAST_CHANNEL; // Local channel 1 on Heltec
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    const uint32_t packetId = packet->id;
    ++cc01Requests;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    // sendToMesh() returns void; this is only a send attempt. Actual RF
    // delivery must be confirmed by an ACK / valid CC01 sensor reply.
    LOG_INFO("CCA CC01 DM poll: sent request %s to 0x%08lx packet=0x%08lx (delivery unconfirmed)",
             command, static_cast<unsigned long>(CC01_POLL_NODE),
             static_cast<unsigned long>(packetId));
    return true; // Not proof of RF delivery; network errors have their own log.
}

void HoboHttpGatewayModule::pollCc01()
{
    const uint32_t now = millis();
    // The 30-minute cycle re-arms only readings already received. A reading
    // that is still missing retains its own timer and keeps retrying.
    const bool due = !cc01PollStarted ? now >= 120000UL :
                     static_cast<uint32_t>(now - cc01LastPollMs) >= CC01_POLL_INTERVAL_MS;
    if (due) {
        cc01PollStarted = true;
        cc01LastPollMs = now;
        if (!cc01WaterPending) {
            cc01WaterPending = true;
            cc01WaterSent = false;
        }
        if (!cc01HoboPending) {
            cc01HoboPending = true;
            cc01HoboSent = false;
            cc01ReadReady = false;
        }
        LOG_INFO("CCA CC01 DM poll: cycle due; WATER pending=%u READ pending=%u",
                 cc01WaterPending, cc01HoboPending);
    }
    if (!cc01PollStarted)
        return;

    // Preserve the original initial sequence: WATER READ first, then READ
    // after a valid water result or the first 120-second water timeout.
    // Subsequent retries are independent, regardless of either result.
    if (cc01HoboPending && !cc01HoboSent && !cc01ReadReady &&
        cc01WaterSent &&
        static_cast<uint32_t>(now - cc01WaterRequestMs) >= CC01_REPLY_TIMEOUT_MS) {
        cc01ReadReady = true;
        cc01ReadReadyMs = now;
        LOG_WARN("CCA CC01 DM poll: WATER READ timeout; retrying WATER while starting READ");
    }

    // At least 3 seconds between ALL outbound CC01 requests, including
    // simultaneous retry deadlines, to avoid needless radio collisions.
    if (cc01HasDmSent && static_cast<uint32_t>(now - cc01LastDmSendMs) < CC01_DM_SPACING_MS)
        return;

    const bool waterDue = cc01WaterPending &&
                          (!cc01WaterSent ||
                           static_cast<uint32_t>(now - cc01WaterRequestMs) >= CC01_REPLY_TIMEOUT_MS);
    if (waterDue) {
        if (sendCc01Dm("WATER READ")) {
            if (cc01WaterSent) {
                ++cc01WaterRetries;
                LOG_WARN("CCA CC01 DM poll: WATER READ retry %lu",
                         static_cast<unsigned long>(cc01WaterRetries));
            }
            cc01WaterSent = true;
            cc01WaterRequestMs = now;
            cc01LastDmSendMs = now;
            cc01HasDmSent = true;
        }
        return;
    }

    const bool hoboDue = cc01HoboPending &&
                         (cc01HoboSent ?
                          static_cast<uint32_t>(now - cc01HoboRequestMs) >= CC01_REPLY_TIMEOUT_MS :
                          cc01ReadReady &&
                          static_cast<uint32_t>(now - cc01ReadReadyMs) >= CC01_DM_SPACING_MS);
    if (hoboDue && sendCc01Dm("READ")) {
        if (cc01HoboSent) {
            ++cc01HoboRetries;
            LOG_WARN("CCA CC01 DM poll: READ retry %lu",
                     static_cast<unsigned long>(cc01HoboRetries));
        }
        cc01HoboSent = true;
        cc01HoboRequestMs = now;
        cc01LastDmSendMs = now;
        cc01HasDmSent = true;
    }
}

void HoboHttpGatewayModule::handleCc01Reply(const meshtastic_MeshPacket &mp)
{
    // Sender, destination and channel were checked by wantPacket().
    // Sensor replies remain useful after a timeout or a manual request.
    ++cc01Replies;
    cc01LastReplyMs = millis();
    cc01LastReplyPacket = mp.id;
    const size_t n = mp.decoded.payload.size;
    if (n == 0 || n >= 240) {
        ++cc01Rejected;
        return;
    }
    char reply[241] = {};
    memcpy(reply, mp.decoded.payload.bytes, n);

    cca_cc01_dm::WaterReading water;
    cca_cc01_dm::HoboReading hobo;
    if (cca_cc01_dm::parseWaterRead(reply, water)) {
        const auto &reading = water;
        UploadJob job = {};
        job.type = JobType::WATER_DISTANCE;
        fillCommon(job, mp);
        job.dmPollResult = true;
        ++cc01Parsed;
        job.distanceValid = true;
        job.stageCalibrated = true;
        job.distanceSensorType = 5; // CC01's installed MB7388 driver
        job.distanceMm = reading.distanceMm;
        job.stageMm = static_cast<int32_t>(lroundf(reading.stageFeet * 304.8f));
        if (uploadQueue.enqueue(job, 0)) {
            ++cc01Queued;
            if (cc01WaterPending) {
                cc01WaterPending = false;
                if (cc01HoboPending && !cc01HoboSent) {
                    cc01ReadReady = true;
                    cc01ReadReadyMs = millis();
                }
            }
            LOG_INFO("CCA CC01 DM poll: queued WATER READ stage %.3f ft raw %lu mm",
                     reading.stageFeet, static_cast<unsigned long>(reading.distanceMm));
            setIntervalFromNow(0);
        } else {
            ++cc01QueueFull;
            LOG_WARN("CCA CC01 DM poll: upload queue full for WATER READ");
        }
    } else if (cca_cc01_dm::parseHoboRead(reply, hobo)) {
        const auto &reading = hobo;
        UploadJob job = {};
        job.type = JobType::MX2001;
        fillCommon(job, mp);
        job.dmPollResult = true;
        ++cc01Parsed;
        job.waterLevelFt = reading.levelFeet;
        job.temperatureF = reading.temperatureF;
        job.temperatureC = (reading.temperatureF - 32.0f) * (5.0f / 9.0f);
        job.bleRssi = static_cast<int8_t>(reading.bleRssi);
        snprintf(job.loggerMac, sizeof(job.loggerMac), "%s", reading.mac);
        snprintf(job.loggerModel, sizeof(job.loggerModel), "MX2001");
        if (uploadQueue.enqueue(job, 0)) {
            ++cc01Queued;
            // A valid MX2001 reply ends only READ retries. The ultrasonic
            // poll continues until its own valid result is queued.
            cc01HoboPending = false;
            LOG_INFO("CCA CC01 DM poll: queued READ stage %.2f ft temp %.1f F",
                     reading.levelFeet, reading.temperatureF);
            setIntervalFromNow(0);
        } else {
            ++cc01QueueFull;
            LOG_WARN("CCA CC01 DM poll: upload queue full for READ");
        }
    } else {
        ++cc01Rejected;
        LOG_WARN("CCA CC01 DM poll: sensor reply format not accepted (%u bytes), state=%u",
                 static_cast<unsigned>(n), static_cast<unsigned>(cc01WaterPending) +
                 (static_cast<unsigned>(cc01HoboPending) << 1));
    }
}

#endif // ARCH_ESP32 && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
