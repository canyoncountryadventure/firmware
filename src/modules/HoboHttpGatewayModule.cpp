#include "HoboHttpGatewayModule.h"

#if defined(ARCH_ESP32) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED

#include "NodeDB.h"
#include "gps/RTC.h"
#include "main.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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

bool payloadStartsWith(const meshtastic_MeshPacket &mp, const char *prefix)
{
    if (prefix == nullptr)
        return false;
    const size_t n = strlen(prefix);
    return mp.decoded.payload.size >= n &&
           memcmp(mp.decoded.payload.bytes, prefix, n) == 0;
}

} // namespace

HoboHttpGatewayModule::HoboHttpGatewayModule()
    : MeshModule("hobo_http_gateway"),
      concurrency::OSThread("hobo_http_gateway"),
      uploadQueue(UPLOAD_QUEUE_SIZE)
{
    isPromiscuous = true;
    uploadQueue.setReader(this);
    setInterval(5000);

    LOG_INFO("HOBO HTTP gateway enabled: MX2001 -> %s", HOBO_HTTP_GATEWAY_URL);
    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0)
        LOG_ERROR("HOBO HTTP gateway: INGEST_KEY is empty; MX2001 cloud uploads are disabled");

    if (strlen(WATER_GITHUB_TOKEN) == 0)
        LOG_WARN("Water alert gateway: WATER_GITHUB_TOKEN is empty; email/issue alerts are disabled");
    else
        LOG_INFO("Water alert gateway enabled: GitHub repo %s", WATER_GITHUB_REPOSITORY);

#if HOBO_HTTP_GATEWAY_FAVORITES_ONLY
    LOG_INFO("HOBO HTTP gateway: favorite-nodes-only filtering enabled");
#endif
}

bool HoboHttpGatewayModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p == nullptr || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return false;

    if (p->decoded.portnum != meshtastic_PortNum_PRIVATE_APP &&
        p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
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
            size_t copyLength = sourceLength;
            if (copyLength >= destSize)
                copyLength = destSize - 1;
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

bool HoboHttpGatewayModule::enqueueMX2001(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size != 19)
        return false;

    const uint8_t *payload = mp.decoded.payload.bytes;
    if (payload[0] != 'M' || payload[1] != 'X')
        return false;

    UploadJob job = {};
    job.kind = JobKind::MX2001;
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

bool HoboHttpGatewayModule::enqueueWaterAlert(const meshtastic_MeshPacket &mp)
{
    if (!payloadStartsWith(mp, "WATER_ALERT|"))
        return false;

    UploadJob job = {};
    job.kind = JobKind::WATER_ALERT;
    fillCommon(job, mp);

    size_t n = mp.decoded.payload.size;
    if (n >= sizeof(job.waterAlert))
        n = sizeof(job.waterAlert) - 1;
    memcpy(job.waterAlert, mp.decoded.payload.bytes, n);
    job.waterAlert[n] = '\0';

    if (!uploadQueue.enqueue(job, 0)) {
        LOG_WARN("Water alert gateway: queue full, dropped packet 0x%08lx",
                 static_cast<unsigned long>(mp.id));
        return false;
    }

    LOG_INFO("Water alert gateway: queued alert from %s", job.stationName);
    return true;
}

ProcessMessage HoboHttpGatewayModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (isDuplicate(mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP)
        enqueueMX2001(mp);
    else if (mp.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP)
        enqueueWaterAlert(mp);

    return ProcessMessage::CONTINUE;
}

bool HoboHttpGatewayModule::uploadMX2001(const UploadJob &job)
{
    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) {
        LOG_ERROR("HOBO HTTP gateway: INGEST_KEY is empty");
        return false;
    }

    String body;
    body.reserve(768);
    body += "{";
    body += "\"type\":\"mx2001\"";
    body += ",\"timestamp\":" + String(job.timestamp);
    body += ",\"from\":" + String(job.from);
    body += ",\"packet_id\":" + String(job.packetId);
    body += ",\"station_name\":" + jsonQuoted(job.stationName);
    body += ",\"payload\":{";
    body += "\"water_level_ft\":" + String(job.waterLevelFt, 3);
    body += ",\"temperature_f\":" + String(job.temperatureF, 3);
    body += ",\"temperature_c\":" + String(job.temperatureC, 3);
    body += ",\"temperature_raw\":" + String(job.temperatureRaw);
    body += ",\"logger_mac\":" + jsonQuoted(job.loggerMac);
    body += ",\"sequence\":" + String(job.sequence);
    body += ",\"ble_rssi_dbm\":" + String(job.bleRssi);
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
    if (!http.begin(client, HOBO_HTTP_GATEWAY_URL))
        return false;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
    http.addHeader("User-Agent", "heltec-hobo-http-gateway/1.1");

    const int status = http.POST(body);
    if (status >= 200 && status < 300) {
        LOG_INFO("HOBO HTTP gateway: cloud stored packet 0x%08lx (HTTP %d)",
                 static_cast<unsigned long>(job.packetId), status);
        http.end();
        return true;
    }

    if (status > 0)
        LOG_WARN("HOBO HTTP gateway: HTTP %d: %.120s", status, http.getString().c_str());
    else
        LOG_WARN("HOBO HTTP gateway: POST failed: %s", http.errorToString(status).c_str());

    http.end();
    return false;
}

bool HoboHttpGatewayModule::uploadWaterAlert(const UploadJob &job)
{
    if (strlen(WATER_GITHUB_TOKEN) == 0) {
        LOG_ERROR("Water alert gateway: WATER_GITHUB_TOKEN is empty");
        return false;
    }

    String url = "https://api.github.com/repos/";
    url += WATER_GITHUB_REPOSITORY;
    url += "/issues";

    String title = "Water alert: ";
    title += job.stationName;

    String issueBody;
    issueBody.reserve(700);
    issueBody += "Automated Meshtastic water-level alert.\n\n";
    issueBody += "**Source:** ";
    issueBody += job.stationName;
    issueBody += "\n\n**Alert:** `";
    issueBody += job.waterAlert;
    issueBody += "`\n\n";
    issueBody += "**LoRa RSSI:** ";
    issueBody += String(job.rssi);
    issueBody += " dBm\n\n**LoRa SNR:** ";
    issueBody += String(job.snr, 2);
    issueBody += " dB\n\n**Hops:** ";
    issueBody += String(hopsAway(job.hopStart, job.hopLimit));
    issueBody += "\n\n**Packet:** `";
    char packetHex[12] = {};
    snprintf(packetHex, sizeof(packetHex), "%08lx", static_cast<unsigned long>(job.packetId));
    issueBody += packetHex;
    issueBody += "`";

    String body;
    body.reserve(1000);
    body += "{\"title\":";
    body += jsonQuoted(title.c_str());
    body += ",\"body\":";
    body += jsonQuoted(issueBody.c_str());
    body += "}";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url))
        return false;

    String authorization = "Bearer ";
    authorization += WATER_GITHUB_TOKEN;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Authorization", authorization);
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    http.addHeader("User-Agent", "heltec-water-alert-gateway/1.0");

    const int status = http.POST(body);
    if (status == 201) {
        LOG_INFO("Water alert gateway: GitHub issue created for packet 0x%08lx",
                 static_cast<unsigned long>(job.packetId));
        http.end();
        return true;
    }

    if (status > 0)
        LOG_WARN("Water alert gateway: GitHub HTTP %d: %.160s", status, http.getString().c_str());
    else
        LOG_WARN("Water alert gateway: GitHub POST failed: %s", http.errorToString(status).c_str());

    http.end();
    return false;
}

bool HoboHttpGatewayModule::upload(const UploadJob &job)
{
    if (!WiFi.isConnected())
        return false;

    if (job.kind == JobKind::WATER_ALERT)
        return uploadWaterAlert(job);
    return uploadMX2001(job);
}

int32_t HoboHttpGatewayModule::runOnce()
{
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
            LOG_WARN("Gateway: retry %u/%u in %lu ms",
                     job.retries, MAX_RETRIES, static_cast<unsigned long>(delayMs));
            return delayMs;
        }
    }

    LOG_ERROR("Gateway: dropping packet 0x%08lx after upload failure",
              static_cast<unsigned long>(job.packetId));
    return 1000;
}

#endif // ARCH_ESP32 && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
