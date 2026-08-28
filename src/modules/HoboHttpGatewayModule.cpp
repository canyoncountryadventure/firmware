#include "HoboHttpGatewayModule.h"

#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED

#include "NodeDB.h"
#include "gps/RTC.h"
#include "main.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdio>
#include <cstring>

namespace
{

uint16_t readLE16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLE32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int16_t readLE16Signed(const uint8_t *p)
{
    return static_cast<int16_t>(readLE16(p));
}

String jsonQuoted(const char *text)
{
    String out;
    out.reserve(text ? strlen(text) + 8 : 2);
    out += '"';
    if (text != nullptr) {
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p < 0x20) {
                    char escaped[7] = {};
                    snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
                    out += escaped;
                } else {
                    out += static_cast<char>(*p);
                }
                break;
            }
        }
    }
    out += '"';
    return out;
}

uint8_t hopsAway(uint8_t hopStart, uint8_t hopLimit)
{
    return hopStart >= hopLimit ? hopStart - hopLimit : 0;
}

} // namespace

HoboHttpGatewayModule *hoboHttpGatewayModule = nullptr;

HoboHttpGatewayModule::HoboHttpGatewayModule()
    : MeshModule("cca_sensor_http_gateway"),
      concurrency::OSThread("cca_sensor_http_gateway"),
      uploadQueue(UPLOAD_QUEUE_SIZE)
{
    isPromiscuous = true;
    uploadQueue.setReader(this);
    setInterval(5000);
    LOG_INFO("CCA sensor gateway enabled: HOBO + moisture/PIR + environment + device -> %s", HOBO_HTTP_GATEWAY_URL);
#if HOBO_HTTP_GATEWAY_FAVORITES_ONLY
    LOG_INFO("CCA sensor gateway: favorite-nodes-only filtering enabled");
#endif
}

bool HoboHttpGatewayModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p == nullptr || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return false;
    if (p->decoded.portnum != meshtastic_PortNum_PRIVATE_APP &&
        p->decoded.portnum != meshtastic_PortNum_TELEMETRY_APP)
        return false;

    const uint32_t from = getFrom(p);
    if (nodeDB == nullptr || from == 0 || from == nodeDB->getNodeNum())
        return false;
#if HOBO_HTTP_GATEWAY_FAVORITES_ONLY
    if (!nodeDB->isFavorite(from))
        return false;
#endif
    return true;
}

bool HoboHttpGatewayModule::isDuplicate(const meshtastic_MeshPacket &mp)
{
    if (mp.id == 0)
        return false;
    const uint32_t from = getFrom(&mp);
    for (const auto &seen : seenPackets) {
        if (seen.from == from && seen.id == mp.id)
            return true;
    }
    seenPackets[seenPacketIndex].from = from;
    seenPackets[seenPacketIndex].id = mp.id;
    seenPacketIndex = (seenPacketIndex + 1) % SEEN_PACKET_SLOTS;
    return false;
}

void HoboHttpGatewayModule::fillStationName(char *dest, size_t destSize, uint32_t from)
{
    if (dest == nullptr || destSize == 0)
        return;
    dest[0] = '\0';
    const meshtastic_NodeInfoLite *node = nodeDB ? nodeDB->getMeshNode(from) : nullptr;
    if (node != nullptr && node->has_user) {
        const size_t sourceLength = strnlen(node->user.long_name, sizeof(node->user.long_name));
        if (sourceLength > 0) {
            size_t copyLength = sourceLength >= destSize ? destSize - 1 : sourceLength;
            memcpy(dest, node->user.long_name, copyLength);
            dest[copyLength] = '\0';
            return;
        }
    }
    snprintf(dest, destSize, "Node %08lx", static_cast<unsigned long>(from));
}

void HoboHttpGatewayModule::fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp)
{
    job.retries = 0;
    job.packetId = mp.id;
    job.from = getFrom(&mp);
    job.timestamp = getTime();
    job.channel = mp.channel;
    job.hopStart = mp.hop_start;
    job.hopLimit = mp.hop_limit;
    job.relayNode = mp.relay_node;
    int rssi = mp.rx_rssi;
    if (rssi > 0)
        rssi -= 200;
    job.rssi = static_cast<int16_t>(rssi);
    job.snr = mp.rx_snr;
    fillStationName(job.stationName, sizeof(job.stationName), job.from);
}

void HoboHttpGatewayModule::fillLocalCommon(UploadJob &job, uint16_t sequence)
{
    job.retries = 0;
    job.packetId = 0xCC000000UL | (++localPacketCounter & 0x00FFFFFFUL);
    job.from = nodeDB ? nodeDB->getNodeNum() : 0;
    job.timestamp = getTime();
    job.channel = 0;
    job.hopStart = 0;
    job.hopLimit = 0;
    job.relayNode = 0;
    job.rssi = 0;
    job.snr = 0;
    job.sequence = sequence;
    job.localBleSensor = true;
    fillStationName(job.stationName, sizeof(job.stationName), job.from);
}

bool HoboHttpGatewayModule::queueLocalEnvironment(float temperatureC, const char *loggerModel, const char *loggerMac,
                                                   int8_t bleRssi, uint16_t sequence)
{
    UploadJob job = {};
    job.type = JobType::ENVIRONMENT;
    fillLocalCommon(job, sequence);
    job.temperatureC = temperatureC;
    job.bleRssi = bleRssi;
    snprintf(job.loggerMac, sizeof(job.loggerMac), "%s", loggerMac ? loggerMac : "");
    snprintf(job.loggerModel, sizeof(job.loggerModel), "%s", loggerModel ? loggerModel : "HOBO");
    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: local environment queue full");
        return false;
    }
    setIntervalFromNow(0);
    return true;
}

bool HoboHttpGatewayModule::queueLocalMX2001(float waterLevelFt, float temperatureF, float temperatureC,
                                              uint16_t temperatureRaw, const char *loggerMac, int8_t bleRssi,
                                              uint16_t sequence)
{
    UploadJob job = {};
    job.type = JobType::MX2001;
    fillLocalCommon(job, sequence);
    job.waterLevelFt = waterLevelFt;
    job.temperatureF = temperatureF;
    job.temperatureC = temperatureC;
    job.temperatureRaw = temperatureRaw;
    job.bleRssi = bleRssi;
    snprintf(job.loggerMac, sizeof(job.loggerMac), "%s", loggerMac ? loggerMac : "");
    snprintf(job.loggerModel, sizeof(job.loggerModel), "MX2001");
    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: local MX2001 queue full");
        return false;
    }
    setIntervalFromNow(0);
    return true;
}

bool HoboHttpGatewayModule::enqueueMX2001(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size != 19)
        return false;
    const uint8_t *payload = mp.decoded.payload.bytes;
    if (payload[0] != 'M' || payload[1] != 'X')
        return false;

    UploadJob job = {};
    job.type = JobType::MX2001;
    fillCommon(job, mp);
    job.sequence = readLE16(&payload[4]);
    const int16_t stageTenths = readLE16Signed(&payload[6]);
    const int16_t temperatureTenthsF = readLE16Signed(&payload[8]);
    job.temperatureRaw = readLE16(&payload[10]);
    job.waterLevelFt = stageTenths / 10.0f;
    job.temperatureF = temperatureTenthsF / 10.0f;
    job.temperatureC = (job.temperatureF - 32.0f) * (5.0f / 9.0f);
    job.bleRssi = static_cast<int8_t>(payload[18]);
    snprintf(job.loggerMac, sizeof(job.loggerMac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[12], payload[13], payload[14], payload[15], payload[16], payload[17]);
    snprintf(job.loggerModel, sizeof(job.loggerModel), "MX2001");

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: upload queue full, dropped MX2001 packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }
    LOG_INFO("CCA sensor gateway: queued MX2001 from 0x%08lx", static_cast<unsigned long>(job.from));
    return true;
}

bool HoboHttpGatewayModule::enqueueMoisturePir(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size != 16)
        return false;
    const uint8_t *payload = mp.decoded.payload.bytes;
    if (payload[0] != 'R' || payload[1] != 'K' || payload[2] != 1)
        return false;

    UploadJob job = {};
    job.type = JobType::MOISTURE_PIR;
    fillCommon(job, mp);
    job.motionDetected = (payload[3] & 0x01) != 0;
    job.moistureAdc = readLE16(&payload[4]);
    job.moistureSensorMv = readLE16(&payload[6]);
    job.motionCount = readLE32(&payload[8]);
    job.batteryMv = readLE16(&payload[12]);
    job.batteryPercent = payload[14];

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: upload queue full, dropped moisture/PIR packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }
    LOG_INFO("CCA sensor gateway: queued moisture ADC=%u + PIR from 0x%08lx",
             job.moistureAdc, static_cast<unsigned long>(job.from));
    return true;
}

bool HoboHttpGatewayModule::enqueueEnvironment(const meshtastic_MeshPacket &mp)
{
    meshtastic_Telemetry decoded = meshtastic_Telemetry_init_zero;
    if (!pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size,
                              &meshtastic_Telemetry_msg, &decoded)) {
        LOG_WARN("CCA sensor gateway: could not decode TELEMETRY_APP packet");
        return false;
    }
    if (decoded.which_variant != meshtastic_Telemetry_environment_metrics_tag ||
        !decoded.variant.environment_metrics.has_temperature)
        return false;

    UploadJob job = {};
    job.type = JobType::ENVIRONMENT;
    fillCommon(job, mp);
    job.temperatureC = decoded.variant.environment_metrics.temperature;
    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: upload queue full, dropped environment packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }
    LOG_INFO("CCA sensor gateway: queued environment temp=%.2f C from 0x%08lx",
             job.temperatureC, static_cast<unsigned long>(job.from));
    return true;
}

bool HoboHttpGatewayModule::enqueueDevice(const meshtastic_MeshPacket &mp)
{
    meshtastic_Telemetry decoded = meshtastic_Telemetry_init_zero;
    if (!pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size,
                              &meshtastic_Telemetry_msg, &decoded))
        return false;
    if (decoded.which_variant != meshtastic_Telemetry_device_metrics_tag)
        return false;

    const auto &device = decoded.variant.device_metrics;
    if (!device.has_battery_level && !device.has_voltage)
        return false;

    UploadJob job = {};
    job.type = JobType::DEVICE;
    fillCommon(job, mp);
    job.hasDeviceBatteryLevel = device.has_battery_level;
    job.hasDeviceVoltage = device.has_voltage;
    job.hasChannelUtilization = device.has_channel_utilization;
    job.hasAirUtilTx = device.has_air_util_tx;
    job.hasUptimeSeconds = device.has_uptime_seconds;
    job.deviceBatteryLevel = device.battery_level;
    job.deviceVoltage = device.voltage;
    job.channelUtilization = device.channel_utilization;
    job.airUtilTx = device.air_util_tx;
    job.uptimeSeconds = device.uptime_seconds;

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("CCA sensor gateway: upload queue full, dropped device packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }
    LOG_INFO("CCA sensor gateway: queued device battery=%lu%% voltage=%.3f V from 0x%08lx",
             static_cast<unsigned long>(job.deviceBatteryLevel), job.deviceVoltage,
             static_cast<unsigned long>(job.from));
    return true;
}

ProcessMessage HoboHttpGatewayModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (isDuplicate(mp))
        return ProcessMessage::CONTINUE;
    if (mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP) {
        if (!enqueueMoisturePir(mp))
            enqueueMX2001(mp);
    } else if (mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP) {
        if (!enqueueEnvironment(mp))
            enqueueDevice(mp);
    }
    return ProcessMessage::CONTINUE;
}

bool HoboHttpGatewayModule::upload(const UploadJob &job)
{
    if (!WiFi.isConnected())
        return false;
    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) {
        LOG_ERROR("CCA sensor gateway: INGEST_KEY is empty");
        return false;
    }

    String body;
    body.reserve(1100);
    body += "{";
    if (job.type == JobType::MX2001)
        body += "\"type\":\"mx2001\"";
    else if (job.type == JobType::MOISTURE_PIR)
        body += "\"type\":\"rock_test\""; // backend compatibility; RK is only a legacy wire/database schema name
    else if (job.type == JobType::DEVICE)
        body += "\"type\":\"device\"";
    else
        body += "\"type\":\"telemetry\"";

    body += ",\"timestamp\":" + String(job.timestamp);
    body += ",\"from\":" + String(job.from);
    body += ",\"packet_id\":" + String(job.packetId);
    body += ",\"station_name\":" + jsonQuoted(job.stationName);
    body += ",\"payload\":{";

    if (job.type == JobType::MX2001) {
        body += "\"water_level_ft\":" + String(job.waterLevelFt, 3);
        body += ",\"temperature_f\":" + String(job.temperatureF, 3);
        body += ",\"temperature_c\":" + String(job.temperatureC, 3);
        body += ",\"temperature_raw\":" + String(job.temperatureRaw);
        body += ",\"logger_mac\":" + jsonQuoted(job.loggerMac);
        body += ",\"sequence\":" + String(job.sequence);
        body += ",\"ble_rssi_dbm\":" + String(job.bleRssi);
        if (job.localBleSensor)
            body += ",\"source\":\"heltec_ble\"";
    } else if (job.type == JobType::MOISTURE_PIR) {
        body += "\"rock_adc\":" + String(job.moistureAdc);
        body += ",\"rock_voltage_v\":" + String(job.moistureSensorMv / 1000.0f, 3);
        body += ",\"motion_detected\":";
        body += job.motionDetected ? "true" : "false";
        body += ",\"motion_count\":" + String(job.motionCount);
        body += ",\"battery_voltage_v\":" + String(job.batteryMv / 1000.0f, 3);
        body += ",\"battery_percent\":" + String(job.batteryPercent);
    } else if (job.type == JobType::DEVICE) {
        bool first = true;
        if (job.hasDeviceBatteryLevel) {
            body += "\"battery_level\":" + String(job.deviceBatteryLevel);
            first = false;
        }
        if (job.hasDeviceVoltage) {
            if (!first) body += ',';
            body += "\"voltage\":" + String(job.deviceVoltage, 3);
            first = false;
        }
        if (job.hasChannelUtilization) {
            if (!first) body += ',';
            body += "\"channel_utilization\":" + String(job.channelUtilization, 3);
            first = false;
        }
        if (job.hasAirUtilTx) {
            if (!first) body += ',';
            body += "\"air_util_tx\":" + String(job.airUtilTx, 3);
            first = false;
        }
        if (job.hasUptimeSeconds) {
            if (!first) body += ',';
            body += "\"uptime_seconds\":" + String(job.uptimeSeconds);
        }
    } else {
        body += "\"temperature\":" + String(job.temperatureC, 3);
        if (job.localBleSensor) {
            body += ",\"temperature_c\":" + String(job.temperatureC, 3);
            body += ",\"logger_model\":" + jsonQuoted(job.loggerModel);
            body += ",\"logger_mac\":" + jsonQuoted(job.loggerMac);
            body += ",\"ble_rssi_dbm\":" + String(job.bleRssi);
            body += ",\"sequence\":" + String(job.sequence);
            body += ",\"source\":\"heltec_ble\"";
        }
    }

    body += "},\"radio\":{";
    body += "\"rssi\":" + String(job.rssi);
    body += ",\"snr\":" + String(job.snr, 2);
    body += ",\"hop_start\":" + String(job.hopStart);
    body += ",\"hop_limit\":" + String(job.hopLimit);
    body += ",\"hops_away\":" + String(hopsAway(job.hopStart, job.hopLimit));
    body += ",\"relay_node\":" + String(job.relayNode);
    body += ",\"channel\":" + String(job.channel);
    body += ",\"gateway\":" + jsonQuoted(HOBO_HTTP_GATEWAY_NAME);
    body += "}}";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, HOBO_HTTP_GATEWAY_URL)) {
        LOG_WARN("CCA sensor gateway: could not initialize HTTPS request");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
    http.addHeader("User-Agent", "cca-heltec-sensor-gateway/2.0");

    const int status = http.POST(body);
    if (status >= 200 && status < 300) {
        LOG_INFO("CCA sensor gateway: cloud accepted packet 0x%08lx (HTTP %d)",
                 static_cast<unsigned long>(job.packetId), status);
        http.end();
        return true;
    }
    if (status > 0) {
        const String response = http.getString();
        LOG_WARN("CCA sensor gateway: HTTP %d: %.120s", status, response.c_str());
    } else {
        LOG_WARN("CCA sensor gateway: POST failed: %s", http.errorToString(status).c_str());
    }
    http.end();
    return false;
}

int32_t HoboHttpGatewayModule::runOnce()
{
    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0)
        return 60000;
    if (!WiFi.isConnected())
        return 5000;

    UploadJob job = {};
    if (!uploadQueue.dequeue(&job, 0))
        return 1000;
    if (upload(job))
        return 25;

    if (job.retries < MAX_RETRIES) {
        ++job.retries;
        if (uploadQueue.enqueue(job, 0)) {
            const uint32_t delayMs = 1000UL << job.retries;
            LOG_WARN("CCA sensor gateway: retry %u/%u in %lu ms",
                     job.retries, MAX_RETRIES, static_cast<unsigned long>(delayMs));
            return delayMs;
        }
    }
    LOG_ERROR("CCA sensor gateway: dropping packet 0x%08lx after upload failure",
              static_cast<unsigned long>(job.packetId));
    return 1000;
}

#endif // ARCH_ESP32 && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
