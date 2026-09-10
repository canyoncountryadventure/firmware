#include "WaterAlertGatewayModule.h"

#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include "NodeDB.h"
#include "main.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>

namespace
{
bool startsWith(const meshtastic_MeshPacket &mp, const char *prefix)
{
    const size_t n = strlen(prefix);
    return mp.decoded.payload.size >= n && memcmp(mp.decoded.payload.bytes, prefix, n) == 0;
}

String quoteJson(const char *text)
{
    String out = "\"";
    if (text) {
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            if (*p == '\"') out += "\\\"";
            else if (*p == '\\') out += "\\\\";
            else if (*p == '\n') out += "\\n";
            else if (*p == '\r') out += "\\r";
            else if (*p == '\t') out += "\\t";
            else if (*p >= 0x20) out += static_cast<char>(*p);
        }
    }
    out += "\"";
    return out;
}
}

WaterAlertGatewayModule::WaterAlertGatewayModule()
    : MeshModule("cca_water_alert_gateway"), concurrency::OSThread("cca_water_alert_gateway"), queue(QUEUE_SIZE)
{
    isPromiscuous = true;
    queue.setReader(this);
    setInterval(5000);
    LOG_INFO("CCA water alert gateway enabled: recovered A02YYUW WATER_ALERT receiver");
}

bool WaterAlertGatewayModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p && p->which_payload_variant == meshtastic_MeshPacket_decoded_tag &&
           p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP &&
           getFrom(p) != 0 && nodeDB && getFrom(p) != nodeDB->getNodeNum();
}

void WaterAlertGatewayModule::fillStationName(char *dest, size_t size, uint32_t from)
{
    if (!dest || size == 0) return;
    dest[0] = '\0';
    const meshtastic_NodeInfoLite *node = nodeDB ? nodeDB->getMeshNode(from) : nullptr;
    if (node && node->has_user && node->user.long_name[0]) {
        snprintf(dest, size, "%s", node->user.long_name);
        return;
    }
    snprintf(dest, size, "Node %08lx", static_cast<unsigned long>(from));
}

ProcessMessage WaterAlertGatewayModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!startsWith(mp, "WATER_ALERT|"))
        return ProcessMessage::CONTINUE;

    AlertJob job = {};
    job.packetId = mp.id;
    job.from = getFrom(&mp);
    job.channel = mp.channel;
    job.hopStart = mp.hop_start;
    job.hopLimit = mp.hop_limit;
    int rssi = mp.rx_rssi;
    if (rssi > 0) rssi -= 200;
    job.rssi = static_cast<int16_t>(rssi);
    job.snr = mp.rx_snr;
    fillStationName(job.stationName, sizeof(job.stationName), job.from);
    size_t n = mp.decoded.payload.size;
    if (n >= sizeof(job.text)) n = sizeof(job.text) - 1;
    memcpy(job.text, mp.decoded.payload.bytes, n);
    job.text[n] = '\0';

    if (!queue.enqueue(job, 0))
        LOG_WARN("CCA water alert gateway: queue full, dropped packet 0x%08lx", static_cast<unsigned long>(job.packetId));
    else
        LOG_INFO("CCA water alert gateway: queued %s from %s", job.text, job.stationName);
    return ProcessMessage::CONTINUE;
}

bool WaterAlertGatewayModule::uploadAlert(const AlertJob &job)
{
    if (!WiFi.isConnected()) return false;
    if (strlen(WATER_GITHUB_TOKEN) == 0) {
        LOG_WARN("CCA water alert gateway: WATER_GITHUB_TOKEN empty; retaining alert for retry");
        return false;
    }

    String title = "Water alert: ";
    title += job.stationName;
    String issue;
    issue.reserve(600);
    issue += "Automated A02YYUW/Meshtastic water-level alert.\n\n**Station:** ";
    issue += job.stationName;
    issue += "\n\n**Event:** `";
    issue += job.text;
    issue += "`\n\n**LoRa RSSI:** ";
    issue += String(job.rssi);
    issue += " dBm\n\n**LoRa SNR:** ";
    issue += String(job.snr, 2);
    issue += " dB\n\n**Packet:** `";
    char hex[12] = {};
    snprintf(hex, sizeof(hex), "%08lx", static_cast<unsigned long>(job.packetId));
    issue += hex;
    issue += "`";

    String body = "{\"title\":" + quoteJson(title.c_str()) + ",\"body\":" + quoteJson(issue.c_str()) + "}";
    String url = "https://api.github.com/repos/";
    url += WATER_GITHUB_REPOSITORY;
    url += "/issues";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) return false;
    String auth = "Bearer ";
    auth += WATER_GITHUB_TOKEN;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Authorization", auth);
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    http.addHeader("User-Agent", "cca-heltec-water-alert-gateway/2026-09-10");
    const int status = http.POST(body);
    if (status == 201) {
        LOG_INFO("CCA water alert gateway: GitHub issue created for %s", job.stationName);
        http.end();
        return true;
    }
    LOG_WARN("CCA water alert gateway: GitHub POST status=%d", status);
    http.end();
    return false;
}

int32_t WaterAlertGatewayModule::runOnce()
{
    AlertJob job = {};
    if (!queue.dequeue(&job, 0)) return 1000;
    if (uploadAlert(job)) return 25;
    if (job.retries < MAX_RETRIES) {
        ++job.retries;
        if (queue.enqueue(job, 0)) return 1000UL << job.retries;
    }
    LOG_ERROR("CCA water alert gateway: alert dropped after retries: %s", job.text);
    return 1000;
}

#endif
