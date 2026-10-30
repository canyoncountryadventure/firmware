#include "HoboHttpGatewayModule.h"
#include "CcaCc01DmParser.h"
#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
#include "MeshService.h"
#include "Router.h"
#include "NodeDB.h"
#include "mesh/Throttle.h"
#include "main.h"
#include "gps/RTC.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace {
constexpr uint32_t CC01_POLL_NODE = 1695962694UL;
constexpr uint32_t MATHESON_POLL_NODE = 2004386937UL;

bool validPollIntervalMinutes(uint32_t minutes)
{
    switch (minutes) {
    case 5: case 10: case 15: case 20: case 30: case 60: case 120:
    case 180: case 360: case 720: case 1440:
        return true;
    default:
        return false;
    }
}
}
bool HoboHttpGatewayModule::sendCc01Text(uint32_t to, const char *command)
{
    if (!command || !nodeDB || !router || !service || to == 0) return false;
    const size_t len = strlen(command);
    if (!len || len > 240) return false;
    meshtastic_MeshPacket *packet = router->allocForSending();
    if (!packet) return false;
    memcpy(packet->decoded.payload.bytes, command, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = to;
    packet->channel = CC01_LONGFAST_CHANNEL;
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;
    if (to == CC01_POLL_NODE) ++cc01Requests;
    if (to == MATHESON_POLL_NODE) ++mathesonRequests;
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true; // Queued, not proof of RF reception.
}
bool HoboHttpGatewayModule::sendCc01Dm(const char *command)
{
    return sendCc01Text(CC01_POLL_NODE, command);
}
void HoboHttpGatewayModule::persistCc01Settings()
{
    Preferences prefs;
    if (prefs.begin("cca_cc01", false)) {
        prefs.putBool("fast", cc01FastMode);
        prefs.putBool("water", cc01WaterEnabled);
        prefs.putBool("hobo", cc01HoboEnabled);
        prefs.putUInt("admin", cc01AdminNode);
        prefs.putUInt("version", cc01ConfigVersion);
        prefs.putUShort("cc01_interval", cc01IntervalOverrideMinutes);
        prefs.putUShort("matheson_interval", mathesonIntervalMinutes);
        prefs.end();
    }
}
void HoboHttpGatewayModule::pollCc01()
{
    const uint32_t now = millis();
    const uint32_t interval = cc01IntervalOverrideMinutes ?
        static_cast<uint32_t>(cc01IntervalOverrideMinutes) * 60UL * 1000UL :
        (cc01FastMode ? CC01_FAST_INTERVAL_MS : CC01_NORMAL_INTERVAL_MS);
    const bool due = !cc01PollStarted ? now >= 120000UL :
                     !Throttle::isWithinTimespanMs(cc01LastPollMs, interval);
    if (due) {
        cc01PollStarted = true;
        cc01LastPollMs = now;
        cc01WaterPending = cc01WaterEnabled;
        cc01HoboPending = cc01HoboEnabled;
        cc01WaterAttempts = cc01HoboAttempts = 0;
        cc01Phase = cc01WaterPending ? Cc01Phase::Water :
                    cc01HoboPending ? Cc01Phase::Hobo : Cc01Phase::Done;
        LOG_INFO("CCA CC01 cycle: interval=%lu min water=%u hobo=%u",
                 static_cast<unsigned long>(interval/60000UL),
                 cc01WaterPending, cc01HoboPending);
    }
    if (!cc01PollStarted) return;
    if (cc01Phase == Cc01Phase::Water && (!cc01WaterPending || !cc01WaterEnabled)) {
        cc01WaterPending = false;
        cc01Phase = cc01HoboPending && cc01HoboEnabled ? Cc01Phase::Hobo : Cc01Phase::Done;
    }
    if (cc01Phase == Cc01Phase::Hobo && (!cc01HoboPending || !cc01HoboEnabled)) {
        cc01HoboPending = false;
        cc01Phase = Cc01Phase::Done;
    }
    if (cc01Phase == Cc01Phase::Done) return;
    if (cc01HasDmSent && Throttle::isWithinTimespanMs(cc01LastDmSendMs, CC01_DM_SPACING_MS)) return;
    if (cc01Phase == Cc01Phase::Water) {
        if (cc01WaterAttempts > 0 &&
            Throttle::isWithinTimespanMs(cc01WaterRequestMs, CC01_REPLY_TIMEOUT_MS)) return;
        if (cc01WaterAttempts >= CC01_ATTEMPT_LIMIT) {
            cc01WaterPending = false;
            cc01Phase = cc01HoboPending && cc01HoboEnabled ? Cc01Phase::Hobo : Cc01Phase::Done;
            return; // Next loop keeps command ordering and spacing.
        }
        if (sendCc01Dm("WATER READ")) {
            ++cc01WaterAttempts;
            if (cc01WaterAttempts > 1) ++cc01WaterRetries;
            cc01WaterRequestMs = now;
            cc01LastDmSendMs = now; cc01HasDmSent = true;
        }
        return;
    }
    if (cc01HoboAttempts > 0 &&
        Throttle::isWithinTimespanMs(cc01HoboRequestMs, CC01_REPLY_TIMEOUT_MS)) return;
    if (cc01HoboAttempts >= CC01_ATTEMPT_LIMIT) {
        cc01HoboPending = false; cc01Phase = Cc01Phase::Done;
        return;
    }
    if (sendCc01Dm("READ")) {
        ++cc01HoboAttempts;
        if (cc01HoboAttempts > 1) ++cc01HoboRetries;
        cc01HoboRequestMs = now;
        cc01LastDmSendMs = now; cc01HasDmSent = true;
    }
}

void HoboHttpGatewayModule::pollMatheson()
{
    if (!cc01PollStarted || cc01Phase != Cc01Phase::Done)
        return;

    const uint32_t now = millis();
    const uint32_t interval = static_cast<uint32_t>(mathesonIntervalMinutes) * 60UL * 1000UL;
    const bool due = !mathesonPollStarted ||
        !Throttle::isWithinTimespanMs(mathesonLastPollMs, interval);
    if (due) {
        mathesonPollStarted = true;
        mathesonLastPollMs = now;
        mathesonHoboPending = true;
        mathesonHoboAttempts = 0;
        mathesonPhase = MathesonPhase::Hobo;
        LOG_INFO("CCA Matheson cycle: interval=%u min after CC01", mathesonIntervalMinutes);
    }

    if (mathesonPhase != MathesonPhase::Hobo || !mathesonHoboPending)
        return;
    if (cc01HasDmSent && Throttle::isWithinTimespanMs(cc01LastDmSendMs, CC01_DM_SPACING_MS))
        return;
    if (mathesonHoboAttempts > 0 &&
        Throttle::isWithinTimespanMs(mathesonHoboRequestMs, MATHESON_REPLY_TIMEOUT_MS))
        return;
    if (mathesonHoboAttempts >= MATHESON_ATTEMPT_LIMIT) {
        mathesonHoboPending = false;
        mathesonPhase = MathesonPhase::Done;
        return;
    }
    if (sendCc01Text(MATHESON_POLL_NODE, "READ")) {
        ++mathesonHoboAttempts;
        mathesonHoboRequestMs = now;
        cc01LastDmSendMs = now;
        cc01HasDmSent = true;
    }
}
void HoboHttpGatewayModule::queueCc01SensorStatus(const meshtastic_MeshPacket &mp, const char *sensor, const char *reason)
{
    UploadJob job = {};
    job.type = JobType::SENSOR_STATUS;
    fillCommon(job, mp);
    job.dmPollResult = true;
    snprintf(job.sensorStatusType, sizeof(job.sensorStatusType), "%s", sensor);
    snprintf(job.sensorStatusReason, sizeof(job.sensorStatusReason), "%s", reason);
    if (uploadQueue.enqueue(job, 0)) setIntervalFromNow(0);
    else ++cc01QueueFull;
}
void HoboHttpGatewayModule::handleCc01Reply(const meshtastic_MeshPacket &mp)
{
    ++cc01Replies;
    cc01LastReplyMs = millis(); cc01LastReplyPacket = mp.id;
    const size_t n = mp.decoded.payload.size;
    if (!n || n >= 240) { ++cc01Rejected; return; }
    char reply[241] = {};
    memcpy(reply, mp.decoded.payload.bytes, n);
    String normalized(reply); normalized.toUpperCase();
    if (normalized.indexOf("HOBO UNAVAILABLE") >= 0) {
        if (cc01Phase == Cc01Phase::Hobo && cc01HoboPending && cc01HoboAttempts) {
            queueCc01SensorStatus(mp, "hobo", "ble_unavailable");
            cc01HoboPending = false; cc01Phase = Cc01Phase::Done;
        }
        return;
    }
    if (normalized.startsWith("READ FAILED:") && normalized.indexOf("TIMEOUT")>=0) {
        if (cc01Phase == Cc01Phase::Water && cc01WaterPending && cc01WaterAttempts) {
            queueCc01SensorStatus(mp,"water","timeout");
        }
        // Still honor the 30s separation between retry attempts.
        return;
    }
    if (normalized.startsWith("HOBO READ FAILED")) {
        if (cc01Phase == Cc01Phase::Hobo && cc01HoboPending && cc01HoboAttempts)
            queueCc01SensorStatus(mp,"hobo","read_failed");
        return;
    }
    cca_cc01_dm::WaterReading water;
    cca_cc01_dm::HoboReading hobo;
    if (cca_cc01_dm::parseWaterRead(reply, water)) {
        UploadJob job = {};
        job.type = JobType::WATER_DISTANCE;
        fillCommon(job,mp); job.dmPollResult=true; ++cc01Parsed;
        job.distanceValid = true; job.stageCalibrated = true;
        job.distanceSensorType = 5;
        job.distanceMm = water.distanceMm;
        job.stageMm = static_cast<int32_t>(lroundf(water.stageFeet*304.8f));
        if (uploadQueue.enqueue(job, 0)) {
            ++cc01Queued;
            cc01HaveStage = true;
            cc01LastStageFt = water.stageFeet; cc01LastStageMs = millis();
            const bool fast = water.stageFeet > 0.75f;
            if (fast != cc01FastMode) { cc01FastMode = fast; persistCc01Settings(); }
            if (cc01Phase == Cc01Phase::Water && cc01WaterPending) {
                cc01WaterPending = false;
                cc01Phase = cc01HoboPending && cc01HoboEnabled ? Cc01Phase::Hobo : Cc01Phase::Done;
            }
            setIntervalFromNow(0);
        } else ++cc01QueueFull;
    } else if (cca_cc01_dm::parseHoboRead(reply,hobo)) {
        UploadJob job = {};
        job.type = JobType::MX2001;
        fillCommon(job,mp); job.dmPollResult=true; ++cc01Parsed;
        job.waterLevelFt = hobo.levelFeet;
        job.temperatureF = hobo.temperatureF;
        job.temperatureC = (hobo.temperatureF-32.0f)*(5.0f/9.0f);
        job.bleRssi = static_cast<int8_t>(hobo.bleRssi);
        snprintf(job.loggerMac,sizeof(job.loggerMac),"%s",hobo.mac);
        snprintf(job.loggerModel,sizeof(job.loggerModel),"MX2001");
        if (uploadQueue.enqueue(job,0)) {
            ++cc01Queued;
            if (cc01Phase == Cc01Phase::Hobo && cc01HoboPending) {
                cc01HoboPending=false; cc01Phase=Cc01Phase::Done;
            }
            setIntervalFromNow(0);
        } else ++cc01QueueFull;
    } else {
        ++cc01Rejected;
        LOG_WARN("CCA CC01 reply rejected: %u bytes", static_cast<unsigned>(n));
    }
}

void HoboHttpGatewayModule::handleMathesonReply(const meshtastic_MeshPacket &mp)
{
    ++mathesonReplies;
    mathesonLastReplyMs = millis();
    const size_t n = mp.decoded.payload.size;
    if (!n || n >= 240) { ++mathesonRejected; return; }
    char reply[241] = {};
    memcpy(reply, mp.decoded.payload.bytes, n);
    String normalized(reply); normalized.toUpperCase();
    if (normalized.indexOf("HOBO UNAVAILABLE") >= 0) {
        if (mathesonPhase == MathesonPhase::Hobo && mathesonHoboPending) {
            queueCc01SensorStatus(mp, "hobo", "ble_unavailable");
            mathesonHoboPending = false;
            mathesonPhase = MathesonPhase::Done;
        }
        return;
    }
    cca_cc01_dm::HoboReading hobo;
    if (!cca_cc01_dm::parseHoboRead(reply, hobo)) { ++mathesonRejected; return; }

    UploadJob job = {};
    job.type = JobType::MX2001;
    fillCommon(job, mp);
    job.dmPollResult = true;
    job.waterLevelFt = hobo.levelFeet;
    job.temperatureF = hobo.temperatureF;
    job.temperatureC = (hobo.temperatureF - 32.0f) * (5.0f / 9.0f);
    job.bleRssi = static_cast<int8_t>(hobo.bleRssi);
    snprintf(job.loggerMac, sizeof(job.loggerMac), "%s", hobo.mac);
    snprintf(job.loggerModel, sizeof(job.loggerModel), "MX2001");
    if (!uploadQueue.enqueue(job, 0)) { ++cc01QueueFull; return; }
    ++mathesonParsed;
    ++mathesonQueued;
    mathesonHoboPending = false;
    mathesonPhase = MathesonPhase::Done;
    setIntervalFromNow(0);
}
void HoboHttpGatewayModule::handleCc01ControlDm(const meshtastic_MeshPacket &mp)
{
    if (!cc01AdminNode || getFrom(&mp)!=cc01AdminNode) return;
    const size_t n=mp.decoded.payload.size;
    if (!n || n>80) return;
    char command[81]={};
    memcpy(command,mp.decoded.payload.bytes,n);
    String text(command); text.trim(); text.toUpperCase();
    bool known=true;
    if (text=="PAUSE HOBO") cc01HoboEnabled=false;
    else if (text=="RESUME HOBO") cc01HoboEnabled=true;
    else if (text=="PAUSE SENSOR") cc01WaterEnabled=false;
    else if (text=="RESUME SENSOR") cc01WaterEnabled=true;
    else if (text=="POLL COTTONWOOD AUTO" || text=="POLL CC01 AUTO") {
        cc01IntervalOverrideMinutes=0;
    } else if (text=="POLL STATUS") { /* query only */ }
    else {
        char target[16] = {};
        unsigned long minutes = 0;
        if (sscanf(text.c_str(), "POLL %15s %lu", target, &minutes) == 2 &&
            validPollIntervalMinutes(minutes)) {
            String node(target); node.toUpperCase();
            if (node == "COTTONWOOD" || node == "CC01")
                cc01IntervalOverrideMinutes = static_cast<uint16_t>(minutes);
            else if (node == "MATHESON")
                mathesonIntervalMinutes = static_cast<uint16_t>(minutes);
            else
                known = false;
        } else {
            known = false;
        }
    }
    if (!known) return;
    if (text!="POLL STATUS") persistCc01Settings();
    String result = String("Polling: Cottonwood ") +
                    (cc01IntervalOverrideMinutes ? String(cc01IntervalOverrideMinutes) : String(cc01FastMode ? 15 : 60)) +
                    " min" + (cc01IntervalOverrideMinutes ? " fixed" : " auto") +
                    ", Matheson " + String(mathesonIntervalMinutes) + " min; sensor " +
                    (cc01WaterEnabled?"ON":"PAUSED") +
                    ", HOBO " + (cc01HoboEnabled?"ON":"PAUSED") +
                    ", config v" + String(cc01ConfigVersion);
    sendCc01Text(getFrom(&mp),result.c_str());
}
void HoboHttpGatewayModule::refreshCc01CloudControl()
{
    const uint32_t now=millis();
    if (!WiFi.isConnected() || !strlen(HOBO_HTTP_GATEWAY_INGEST_KEY)) return;
    if (cc01NextConfigMs && static_cast<int32_t>(now-cc01NextConfigMs)<0) return;
    cc01NextConfigMs=now+120000UL;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(4000);
    if (!http.begin(client,"https://meshtastic-ecru.vercel.app/api/ingest?control=cc01")) return;
    http.addHeader("X-Ingest-Key",HOBO_HTTP_GATEWAY_INGEST_KEY);
    const int status=http.GET();
    if (status==200) {
        String body=http.getString();
        const int index=body.indexOf("\"version\":");
        if (index>=0) {
            uint32_t version=static_cast<uint32_t>(strtoul(body.c_str()+index+10,nullptr,10));
            if (version>cc01ConfigVersion) {
                cc01WaterEnabled=body.indexOf("\"water_enabled\":true")>=0;
                cc01HoboEnabled=body.indexOf("\"hobo_enabled\":true")>=0;
                const int p=body.indexOf("\"authorized_admin_node\":");
                if (p>=0) cc01AdminNode=static_cast<uint32_t>(strtoul(body.c_str()+p+24,nullptr,10));
                cc01ConfigVersion=version;
                persistCc01Settings();
                LOG_INFO("CCA CC01 cloud control applied version %lu",static_cast<unsigned long>(version));
            }
        }
    }
    http.end();
    HTTPClient report;
    if (!report.begin(client,"https://meshtastic-ecru.vercel.app/api/ingest?control=cc01")) return;
    report.addHeader("X-Ingest-Key",HOBO_HTTP_GATEWAY_INGEST_KEY);
    report.addHeader("Content-Type","application/json");
    const String json=String("{\"type\":\"report\",\"water_enabled\":") +
      (cc01WaterEnabled?"true":"false") + ",\"hobo_enabled\":" +
      (cc01HoboEnabled?"true":"false") + ",\"version\":" + String(cc01ConfigVersion) + "}";
    report.POST(json);
    report.end();
}
#endif
