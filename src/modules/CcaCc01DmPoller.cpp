#include "HoboHttpGatewayModule.h"
#include "CcaCc01DmParser.h"

#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED

#include "MeshService.h"
#include "Router.h"
#include "NodeDB.h"
#include "main.h"
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
    if (cc01PollState == Cc01PollState::IDLE) {
        const bool due = cc01LastPollMs == 0 ? now >= 120000UL :
                         static_cast<uint32_t>(now - cc01LastPollMs) >= CC01_POLL_INTERVAL_MS;
        if (!due)
            return;
        cc01LastPollMs = now;
        if (sendCc01Dm("WATER READ")) {
            cc01PollState = Cc01PollState::WAIT_WATER;
            cc01RequestMs = now;
        }
        return;
    }
    if (cc01PollState == Cc01PollState::WAIT_WATER &&
        static_cast<uint32_t>(now - cc01RequestMs) >= CC01_REPLY_TIMEOUT_MS) {
        LOG_WARN("CCA CC01 DM poll: WATER READ reply timeout, continuing to READ");
        cc01PollState = Cc01PollState::SEND_HOBO;
        cc01RequestMs = now;
    }
    if (cc01PollState == Cc01PollState::SEND_HOBO &&
        static_cast<uint32_t>(now - cc01RequestMs) >= 3000UL) {
        if (sendCc01Dm("READ")) {
            cc01PollState = Cc01PollState::WAIT_HOBO;
            cc01RequestMs = now;
        } else {
            LOG_WARN("CCA CC01 DM poll: READ could not be queued");
            cc01PollState = Cc01PollState::IDLE;
        }
    }
    if (cc01PollState == Cc01PollState::WAIT_HOBO &&
        static_cast<uint32_t>(now - cc01RequestMs) >= CC01_REPLY_TIMEOUT_MS) {
        LOG_WARN("CCA CC01 DM poll: READ reply timeout");
        cc01PollState = Cc01PollState::IDLE;
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
            LOG_INFO("CCA CC01 DM poll: queued WATER READ stage %.3f ft raw %lu mm",
                     reading.stageFeet, static_cast<unsigned long>(reading.distanceMm));
            setIntervalFromNow(0);
        } else {
            ++cc01QueueFull;
            LOG_WARN("CCA CC01 DM poll: upload queue full for WATER READ");
        }
        if (cc01PollState == Cc01PollState::WAIT_WATER) {
            cc01PollState = Cc01PollState::SEND_HOBO;
            cc01RequestMs = millis();
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
            LOG_INFO("CCA CC01 DM poll: queued READ stage %.2f ft temp %.1f F",
                     reading.levelFeet, reading.temperatureF);
            setIntervalFromNow(0);
        } else {
            ++cc01QueueFull;
            LOG_WARN("CCA CC01 DM poll: upload queue full for READ");
        }
        if (cc01PollState == Cc01PollState::WAIT_HOBO)
            cc01PollState = Cc01PollState::IDLE;
    } else {
        ++cc01Rejected;
        LOG_WARN("CCA CC01 DM poll: sensor reply format not accepted (%u bytes), state=%u",
                 static_cast<unsigned>(n), static_cast<unsigned>(cc01PollState));
    }
}

#endif // ARCH_ESP32 && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
