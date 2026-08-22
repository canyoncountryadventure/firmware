#include "HoboHttpGatewayModule.h"

#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED

#include "NodeDB.h"
#include "gps/RTC.h"
#include "main.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <pb_decode.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

uint16_t readLE16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
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
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
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
    if (hopStart >= hopLimit)
        return hopStart - hopLimit;
    return 0;
}

} // namespace

HoboHttpGatewayModule::HoboHttpGatewayModule()
    : MeshModule("hobo_http_gateway", meshtastic_PortNum_UNKNOWN_APP),
      concurrency::OSThread("hobo_http_gateway"),
      uploadQueue(UPLOAD_QUEUE_SIZE)
{
    // The home gateway must see packets even when they are not addressed to it.
    isPromiscuous = true;
    uploadQueue.setReader(this);
    setInterval(5000);

    LOG_INFO("HOBO HTTP gateway enabled: %s", HOBO_HTTP_GATEWAY_URL);
#if HOBO_HTTP_GATEWAY_FAVORITES_ONLY
    LOG_INFO("HOBO HTTP gateway: favorite-nodes-only filtering enabled");
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
    if (node != nullptr) {
        const size_t sourceLength = strnlen(node->long_name, sizeof(node->long_name));
        if (sourceLength > 0) {
            size_t copyLength = sourceLength;
            if (copyLength >= destSize)
                copyLength = destSize - 1;
            memcpy(dest, node->long_name, copyLength);
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

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("HOBO HTTP gateway: upload queue full, dropped MX2001 packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }

    LOG_INFO("HOBO HTTP gateway: queued MX2001 packet from 0x%08lx",
             static_cast<unsigned long>(job.from));
    return true;
}

bool HoboHttpGatewayModule::enqueueEnvironment(const meshtastic_MeshPacket &mp)
{
    meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(mp.decoded.payload.bytes, mp.decoded.payload.size);
    if (!pb_decode(&stream, &meshtastic_Telemetry_msg, &telemetry)) {
        LOG_DEBUG("HOBO HTTP gateway: telemetry protobuf decode failed");
        return false;
    }

    if (telemetry.which_variant != meshtastic_Telemetry_environment_metrics_tag ||
        !telemetry.variant.environment_metrics.has_temperature)
        return false;

    const float temperatureC = telemetry.variant.environment_metrics.temperature;
    if (!std::isfinite(temperatureC))
        return false;

    UploadJob job = {};
    job.type = JobType::ENVIRONMENT;
    fillCommon(job, mp);
    if (telemetry.time > 0)
        job.timestamp = telemetry.time;
    job.temperatureC = temperatureC;
    job.temperatureF = temperatureC * 9.0f / 5.0f + 32.0f;

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("HOBO HTTP gateway: upload queue full, dropped telemetry packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }

    LOG_INFO("HOBO HTTP gateway: queued environment telemetry from 0x%08lx",
             static_cast<unsigned long>(job.from));
    return true;
}

ProcessMessage HoboHttpGatewayModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (isDuplicate(mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP)
        enqueueMX2001(mp);
    else if (mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP)
        enqueueEnvironment(mp);

    // Never consume the packet; normal Meshtastic processing continues.
    return ProcessMessage::CONTINUE;
}

bool HoboHttpGatewayModule::upload(const UploadJob &job)
{
    if (!WiFi.isConnected())
        return false;

    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) {
        LOG_ERROR("HOBO HTTP gateway: INGEST_KEY is empty");
        return false;
    }

    String body;
    body.reserve(768);
    body += "{";
    body += "\"type\":";
    body += jsonQuoted(job.type == JobType::MX2001 ? "mx2001" : "telemetry");
    body += ",\"timestamp\":";
    body += String(job.timestamp);
    body += ",\"from\":";
    body += String(job.from);
    body += ",\"packet_id\":";
    body += String(job.packetId);
    body += ",\"station_name\":";
    body += jsonQuoted(job.stationName);
    body += ",\"payload\":{";

    if (job.type == JobType::MX2001) {
        body += "\"water_level_ft\":";
        body += String(job.waterLevelFt, 3);
        body += ",\"temperature_f\":";
        body += String(job.temperatureF, 3);
        body += ",\"temperature_c\":";
        body += String(job.temperatureC, 3);
        body += ",\"temperature_raw\":";
        body += String(job.temperatureRaw);
        body += ",\"logger_mac\":";
        body += jsonQuoted(job.loggerMac);
        body += ",\"sequence\":";
        body += String(job.sequence);
        body += ",\"ble_rssi_dbm\":";
        body += String(job.bleRssi);
    } else {
        body += "\"temperature\":";
        body += String(job.temperatureC, 3);
    }

    body += "},\"radio\":{";
    body += "\"rssi\":";
    body += String(job.rssi);
    body += ",\"snr\":";
    body += String(job.snr, 2);
    body += ",\"hop_start\":";
    body += String(job.hopStart);
    body += ",\"hop_limit\":";
    body += String(job.hopLimit);
    body += ",\"hops_away\":";
    body += String(hopsAway(job.hopStart, job.hopLimit));
    body += ",\"relay_node\":";
    body += String(job.relayNode);
    body += ",\"channel\":";
    body += String(job.channel);
    body += ",\"gateway\":";
    body += jsonQuoted(HOBO_HTTP_GATEWAY_NAME);
    body += "}}";

    WiFiClientSecure client;
    // Initial field build: HTTPS is encrypted, but certificate-chain validation is
    // intentionally disabled to avoid bundling a changing Vercel CA in firmware.
    // The application-layer INGEST_KEY still authenticates writes at the API.
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, HOBO_HTTP_GATEWAY_URL)) {
        LOG_WARN("HOBO HTTP gateway: could not initialize HTTPS request");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
    http.addHeader("User-Agent", "heltec-hobo-http-gateway/1.0");

    const int status = http.POST(body);
    if (status >= 200 && status < 300) {
        LOG_INFO("HOBO HTTP gateway: cloud stored packet 0x%08lx (HTTP %d)",
                 static_cast<unsigned long>(job.packetId), status);
        http.end();
        return true;
    }

    if (status > 0) {
        const String response = http.getString();
        LOG_WARN("HOBO HTTP gateway: HTTP %d: %.120s", status, response.c_str());
    } else {
        LOG_WARN("HOBO HTTP gateway: POST failed: %s", http.errorToString(status).c_str());
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
            LOG_WARN("HOBO HTTP gateway: retry %u/%u in %lu ms",
                     job.retries, MAX_RETRIES, static_cast<unsigned long>(delayMs));
            return delayMs;
        }
    }

    LOG_ERROR("HOBO HTTP gateway: dropping packet 0x%08lx after upload failure",
              static_cast<unsigned long>(job.packetId));
    return 1000;
}

#endif // ARCH_ESP32 && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
