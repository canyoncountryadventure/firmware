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
constexpr uint32_t MILL_CREEK_POLL_NODE = 1683604927UL;
constexpr uint32_t PACK_CREEK_POLL_NODE = 1949224949UL;

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
uint32_t pollNodeForName(const String &name)
{
    if (name == "COTTONWOOD" || name == "COTTONWOOD CREEK" || name == "CC01") return CC01_POLL_NODE;
    if (name == "MATHESON" || name == "MATHESON GROUNDWATER") return MATHESON_POLL_NODE;
    if (name == "MILL CREEK" || name == "MILL") return MILL_CREEK_POLL_NODE;
    if (name == "PACK CREEK" || name == "PACK") return PACK_CREEK_POLL_NODE;

    const char *number = name.c_str();
    int base = 10;
    if (*number == '!') {
        ++number;
        base = 16;
    } else if (number[0] == '0' && number[1] == 'X') {
        number += 2;
        base = 16;
    }
    if (!*number) return 0;
    char *end = nullptr;
    const unsigned long parsed = strtoul(number, &end, base);
    return end != number && *end == '\0' && parsed != 0 && parsed <= UINT32_MAX
        ? static_cast<uint32_t>(parsed) : 0;
}

String popPollToken(String &value)
{
    value.trim();
    const int split = value.lastIndexOf(' ');
    if (split < 0) {
        String token = value;
        value = "";
        return token;
    }
    String token = value.substring(split + 1);
    value = value.substring(0, split);
    value.trim();
    return token;
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
    if (configuredPollActive >= 0 &&
        to == configuredPolls[configuredPollActive].node) ++configuredPollRequests;
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
        for (uint8_t i = 0; i < CONFIGURED_POLL_PERSISTENT_SLOTS; ++i) {
            char key[18] = {};
            snprintf(key, sizeof(key), "poll%u_node", i);
            prefs.putUInt(key, configuredPolls[i].node);
            snprintf(key, sizeof(key), "poll%u_interval", i);
            prefs.putUShort(key, configuredPolls[i].intervalMinutes);
            snprintf(key, sizeof(key), "poll%u_mode", i);
            prefs.putUChar(key, static_cast<uint8_t>(configuredPolls[i].mode));
        }
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
bool HoboHttpGatewayModule::isConfiguredPollReply(uint32_t from) const
{
    return configuredPollActive >= 0 && configuredPollActive < CONFIGURED_POLL_SLOTS &&
        configuredPolls[configuredPollActive].node == from &&
        configuredPolls[configuredPollActive].phase != ConfiguredPollPhase::Done;
}

void HoboHttpGatewayModule::pollConfiguredStations()
{
    // V8: failed-auto rescue uses the final one-shot slot. Dedicated V7
    // unconditional pollers are retired, so this scheduler stands alone.
    const uint32_t now = millis();
    if (configuredPollActive < 0) {
        for (uint8_t order = 0; order < CONFIGURED_POLL_SLOTS; ++order) {
            const uint8_t i = order == 0 ? AUTO_FALLBACK_POLL_SLOT : order - 1;
            ConfiguredPoll &candidate = configuredPolls[i];
            if (!candidate.node || (!candidate.oneShot && !candidate.intervalMinutes))
                continue;
            const uint32_t interval = static_cast<uint32_t>(candidate.intervalMinutes) * 60UL * 1000UL;
            if (candidate.oneShot || !candidate.started ||
                !Throttle::isWithinTimespanMs(candidate.lastPollMs, interval)) {
                candidate.started = true;
                candidate.lastPollMs = now;
                candidate.attempts = 0;
                candidate.phase = candidate.mode == ConfiguredPollMode::Water ||
                    candidate.mode == ConfiguredPollMode::Both ? ConfiguredPollPhase::Water : ConfiguredPollPhase::Read;
                configuredPollActive = static_cast<int8_t>(i);
                LOG_INFO("CCA configured poll: node 0x%08lx mode=%u", static_cast<unsigned long>(candidate.node),
                         static_cast<unsigned>(candidate.mode));
                break;
            }
        }
    }
    if (configuredPollActive < 0) return;

    ConfiguredPoll &poll = configuredPolls[configuredPollActive];
    if (poll.phase == ConfiguredPollPhase::Done) {
        if (poll.oneShot) poll = ConfiguredPoll{};
        configuredPollActive = -1;
        return;
    }
    if (cc01HasDmSent && Throttle::isWithinTimespanMs(cc01LastDmSendMs, CC01_DM_SPACING_MS)) return;
    if (poll.attempts > 0 && Throttle::isWithinTimespanMs(poll.requestMs, CC01_REPLY_TIMEOUT_MS)) return;
    if (poll.attempts >= CC01_ATTEMPT_LIMIT) {
        if (poll.phase == ConfiguredPollPhase::Water && poll.mode == ConfiguredPollMode::Both) {
            poll.phase = ConfiguredPollPhase::Read;
            poll.attempts = 0;
        } else {
            poll.phase = ConfiguredPollPhase::Done;
        }
        return;
    }
    const char *command = poll.phase == ConfiguredPollPhase::Water ? "WATER READ" : "READ";
    if (sendCc01Text(poll.node, command)) {
        ++poll.attempts;
        poll.requestMs = now;
        cc01LastDmSendMs = now;
        cc01HasDmSent = true;
    }
}

void HoboHttpGatewayModule::noteAutomaticSensor(uint32_t node, cca_auto_fallback::Kind kind)
{
    if (!autoFallback.receivedAuto(node, kind, millis())) return;
    // A recovered automatic packet cancels an unsent fallback for exactly
    // this sensor, but never disables a different sensor on the same node.
    ConfiguredPoll &fallback = configuredPolls[AUTO_FALLBACK_POLL_SLOT];
    const ConfiguredPollMode mode = kind == cca_auto_fallback::Kind::Water ?
        ConfiguredPollMode::Water : ConfiguredPollMode::Read;
    if (fallback.node == node && fallback.mode == mode) {
        fallback = ConfiguredPoll{};
        if (configuredPollActive == AUTO_FALLBACK_POLL_SLOT) configuredPollActive = -1;
    }
}
void HoboHttpGatewayModule::pollMissingAutomatic()
{
    if (configuredPolls[AUTO_FALLBACK_POLL_SLOT].node) return;
    const uint32_t now = millis();
    for (size_t i = 0; i < autoFallback.watches.size(); ++i) {
        const auto &watch = autoFallback.watches[i];
        if (!autoFallback.due(i, now)) continue;
        if (watch.node == CC01_POLL_NODE && watch.kind == cca_auto_fallback::Kind::Water &&
            !cc01WaterEnabled) continue;

        ConfiguredPoll &rescue = configuredPolls[AUTO_FALLBACK_POLL_SLOT];
        rescue = ConfiguredPoll{};
        rescue.node = watch.node;
        rescue.mode = watch.kind == cca_auto_fallback::Kind::Water ?
            ConfiguredPollMode::Water : ConfiguredPollMode::Read;
        rescue.oneShot = true;
        autoFallback.attemptedAt(i, now);
        ++autoFallbackQueued;
        LOG_INFO("CCA V8 missing-auto rescue node=0x%08lx kind=%u age=%lu sec",
                 static_cast<unsigned long>(watch.node),
                 static_cast<unsigned>(watch.kind),
                 static_cast<unsigned long>((now - watch.lastAutoMs) / 1000UL));
        setIntervalFromNow(0);
        break;
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
void HoboHttpGatewayModule::handleConfiguredPollReply(const meshtastic_MeshPacket &mp)
{
    if (!isConfiguredPollReply(getFrom(&mp))) return;
    ++configuredPollReplies;
    const size_t n = mp.decoded.payload.size;
    if (!n || n >= 240) { ++configuredPollRejected; return; }
    char reply[241] = {};
    memcpy(reply, mp.decoded.payload.bytes, n);
    ConfiguredPoll &poll = configuredPolls[configuredPollActive];
    String normalized(reply); normalized.toUpperCase();
    if (normalized.indexOf("HOBO UNAVAILABLE") >= 0) {
        // The legacy sensor-event endpoint is Cottonwood-specific. Never
        // attribute another station's missing HOBO to South Cottonwood.
        if (getFrom(&mp) == CC01_POLL_NODE)
            queueCc01SensorStatus(mp, "hobo", "ble_unavailable");
        poll.phase = ConfiguredPollPhase::Done;
        return;
    }
    if (normalized.startsWith("READ FAILED:")) {
        // Preserve the retry timer and wait for the bounded re-request.
        return;
    }

    cca_cc01_dm::WaterReading water;
    cca_cc01_dm::HoboReading hobo;
    cca_cc01_dm::HoboTemperatureReading hoboTemp;
    UploadJob job = {};
    const ConfiguredPollPhase originalPhase = poll.phase;
    const uint8_t originalAttempts = poll.attempts;
    if (poll.phase == ConfiguredPollPhase::Water && cca_cc01_dm::parseWaterRead(reply, water)) {
        job.type = JobType::WATER_DISTANCE;
        fillCommon(job, mp);
        job.dmPollResult = true;
        job.distanceValid = true;
        job.stageCalibrated = true;
        job.distanceSensorType = getFrom(&mp) == CC01_POLL_NODE ? 5 :
            getFrom(&mp) == PACK_CREEK_POLL_NODE ? 3 : 0;
        job.distanceMm = water.distanceMm;
        job.stageMm = static_cast<int32_t>(lroundf(water.stageFeet * 304.8f));
        if (poll.phase == ConfiguredPollPhase::Water && poll.mode == ConfiguredPollMode::Both) {
            poll.phase = ConfiguredPollPhase::Read;
            poll.attempts = 0;
        } else {
            poll.phase = ConfiguredPollPhase::Done;
        }
    } else if (poll.phase == ConfiguredPollPhase::Read && cca_cc01_dm::parseHoboRead(reply, hobo)) {
        // A temperature-only station can carry an MX2001 despite having no
        // groundwater stage configured on its station dashboard. Preserve
        // its rescue temperature as environmental telemetry in that case.
        bool temperatureOnly = false;
        if (configuredPollActive == AUTO_FALLBACK_POLL_SLOT) {
            for (const auto &watch : autoFallback.watches) {
                if (watch.node == getFrom(&mp) && watch.kind == cca_auto_fallback::Kind::Temperature) {
                    temperatureOnly = true;
                    break;
                }
            }
        }
        job.type = temperatureOnly ? JobType::ENVIRONMENT : JobType::MX2001;
        fillCommon(job, mp);
        job.dmPollResult = true;
        job.waterLevelFt = hobo.levelFeet;
        job.temperatureF = hobo.temperatureF;
        job.temperatureC = (hobo.temperatureF - 32.0f) * (5.0f / 9.0f);
        job.bleRssi = static_cast<int8_t>(hobo.bleRssi);
        snprintf(job.loggerMac, sizeof(job.loggerMac), "%s", hobo.mac);
        snprintf(job.loggerModel, sizeof(job.loggerModel), "MX2001");
        poll.phase = ConfiguredPollPhase::Done;
    } else if (poll.phase == ConfiguredPollPhase::Read && cca_cc01_dm::parseHoboTemperatureRead(reply, hoboTemp)) {
        job.type = JobType::ENVIRONMENT;
        fillCommon(job, mp);
        job.dmPollResult = true;
        job.temperatureC = (hoboTemp.temperatureF - 32.0f) * (5.0f / 9.0f);
        poll.phase = ConfiguredPollPhase::Done;
    } else {
        ++configuredPollRejected;
        LOG_WARN("CCA configured poll reply rejected: node=0x%08lx bytes=%u",
                 static_cast<unsigned long>(getFrom(&mp)), static_cast<unsigned>(n));
        return;
    }
    if (!uploadQueue.enqueue(job, 0)) {
        poll.phase = originalPhase;
        poll.attempts = originalAttempts;
        ++cc01QueueFull;
        return;
    }
    if (configuredPollActive == AUTO_FALLBACK_POLL_SLOT) ++autoFallbackReplies;
    ++configuredPollParsed;
    ++configuredPollQueued;
    setIntervalFromNow(0);
}
void HoboHttpGatewayModule::handleCc01ControlDm(const meshtastic_MeshPacket &mp)
{
    if (!cc01AdminNode || getFrom(&mp)!=cc01AdminNode) return;
    const size_t n=mp.decoded.payload.size;
    if (!n || n>120) return;
    char command[121]={};
    memcpy(command,mp.decoded.payload.bytes,n);
    String text(command); text.trim(); text.toUpperCase();
    bool known=true, changed=false;
    String action;
    if (text=="PAUSE HOBO") cc01HoboEnabled=false;
    else if (text=="RESUME HOBO") cc01HoboEnabled=true;
    else if (text=="PAUSE SENSOR") cc01WaterEnabled=false;
    else if (text=="RESUME SENSOR") cc01WaterEnabled=true;
    else if (text=="POLL COTTONWOOD AUTO" || text=="POLL CC01 AUTO") {
        cc01IntervalOverrideMinutes=0;
        changed=true;
    } else if (text=="POLL STATUS") { /* query only */ }
    else {
        if (!text.startsWith("POLL ")) {
            known=false;
        } else {
            String request=text.substring(5); request.trim();
            String working=request;
            String last=popPollToken(working);
            ConfiguredPollMode mode=ConfiguredPollMode::Read;
            bool explicitMode=false;
            if (last=="READ" || last=="WATER" || last=="BOTH") {
                explicitMode=true;
                mode=last=="WATER" ? ConfiguredPollMode::Water :
                    last=="BOTH" ? ConfiguredPollMode::Both : ConfiguredPollMode::Read;
                last=popPollToken(working);
            }
            const bool disable=last=="OFF";
            char *end=nullptr;
            const unsigned long minutes=strtoul(last.c_str(),&end,10);
            const bool recurring=!disable && end!=last.c_str() && *end=='\0' && validPollIntervalMinutes(minutes);
            String target=disable || recurring ? working :
                (explicitMode ? working + " " + last : request);
            target.trim();
            const uint32_t node=pollNodeForName(target);
            if (!node) {
                known=false;
            } else if (node==CC01_POLL_NODE) {
                if (!recurring) known=false;
                else { cc01IntervalOverrideMinutes=static_cast<uint16_t>(minutes); changed=true; }
            } else if (node==MATHESON_POLL_NODE) {
                if (!recurring) known=false;
                else { mathesonIntervalMinutes=static_cast<uint16_t>(minutes); changed=true; }
            } else if (disable) {
                bool found=false;
                for (uint8_t i=0;i<CONFIGURED_POLL_PERSISTENT_SLOTS;++i) {
                    if (configuredPolls[i].node==node) {
                        configuredPolls[i]=ConfiguredPoll{};
                        if (configuredPollActive==static_cast<int8_t>(i)) configuredPollActive=-1;
                        found=true;
                    }
                }
                known=found;
                changed=found;
                action=found ? "disabled" : "was not scheduled";
            } else if (!recurring) {
                if (configuredPollActive>=0) {
                    known=false;
                    action="busy; wait for current poll";
                } else {
                    ConfiguredPoll &manual=configuredPolls[CONFIGURED_POLL_PERSISTENT_SLOTS];
                    manual=ConfiguredPoll{};
                    manual.node=node;
                    manual.mode=mode;
                    manual.oneShot=true;
                    action="one-time " + String(mode==ConfiguredPollMode::Water ? "WATER READ" :
                        mode==ConfiguredPollMode::Both ? "WATER READ then READ" : "READ") + " queued";
                }
            } else {
                int8_t slot=-1;
                for (uint8_t i=0;i<CONFIGURED_POLL_PERSISTENT_SLOTS;++i)
                    if (configuredPolls[i].node==node) { slot=static_cast<int8_t>(i); break; }
                if (slot<0) for (uint8_t i=0;i<CONFIGURED_POLL_PERSISTENT_SLOTS;++i)
                    if (!configuredPolls[i].node) { slot=static_cast<int8_t>(i); break; }
                if (slot<0) {
                    known=false;
                    action="table full; disable one with POLL <name> OFF";
                } else {
                    ConfiguredPoll &entry=configuredPolls[slot];
                    entry=ConfiguredPoll{};
                    entry.node=node;
                    entry.intervalMinutes=static_cast<uint16_t>(minutes);
                    entry.mode=mode;
                    action="every " + String(minutes) + " min " +
                        String(mode==ConfiguredPollMode::Water ? "WATER READ" :
                        mode==ConfiguredPollMode::Both ? "WATER+READ" : "READ");
                    changed=true;
                }
            }
        }
    }
    if (!known) {
        if (!action.length()) action="syntax: POLL <name|node> [minutes] [READ|WATER|BOTH]";
        sendCc01Text(getFrom(&mp), (String("Poll not changed: ")+action).c_str());
        return;
    }
    if (text!="POLL STATUS" && (changed || text.startsWith("PAUSE") || text.startsWith("RESUME"))) persistCc01Settings();
    String result = String("V8 auto fallback after 60m; legacy Cottonwood ") +
                    (cc01IntervalOverrideMinutes ? String(cc01IntervalOverrideMinutes) : String(cc01FastMode ? 15 : 60)) +
                    " min" + (cc01IntervalOverrideMinutes ? " fixed" : " auto") +
                    ", Matheson " + String(mathesonIntervalMinutes) + " min; sensor " +
                    (cc01WaterEnabled?"ON":"PAUSED") +
                    ", HOBO " + (cc01HoboEnabled?"ON":"PAUSED") +
                    ", custom " + action + ", config v" + String(cc01ConfigVersion);
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
