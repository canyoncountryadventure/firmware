#include "FishlakePollerModule.h"

#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include "HoboHttpGatewayModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "gps/RTC.h"
#include "main.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

bool timeReached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

uint8_t hopsAway(uint8_t hopStart, uint8_t hopLimit)
{
    return hopStart >= hopLimit ? hopStart - hopLimit : 0;
}

String jsonQuoted(const char *text)
{
    String out;
    out += '"';
    if (text != nullptr) {
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p >= 0x20)
                    out += static_cast<char>(*p);
                break;
            }
        }
    }
    out += '"';
    return out;
}

void copyField(char *dest, size_t destSize, const char *start)
{
    if (dest == nullptr || destSize == 0 || start == nullptr)
        return;
    while (*start == ' ' || *start == '\t')
        ++start;
    size_t n = 0;
    while (start[n] != '\0' && start[n] != '\r' && start[n] != '\n' && n + 1 < destSize) {
        dest[n] = start[n];
        ++n;
    }
    dest[n] = '\0';
}

} // namespace

FishlakePollerModule *fishlakePollerModule = nullptr;

FishlakePollerModule::FishlakePollerModule()
    : MeshModule("fishlake_hobo_poller"),
      concurrency::OSThread("fishlake_hobo_poller"),
      uploadQueue(QUEUE_SIZE)
{
    isPromiscuous = true;
    uploadQueue.setReader(this);
    nextPollMs = millis() + FIRST_POLL_DELAY_MS;
    setInterval(1000);
    LOG_INFO("Fishlake poller: remote READ enabled for !5e021e35 every 60 minutes");
}

bool FishlakePollerModule::wantPacket(const meshtastic_MeshPacket *p)
{
    if (p == nullptr || p->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return false;
    if (p->decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;
    return getFrom(p) == FISHLAKE_NODE_NUM;
}

ProcessMessage FishlakePollerModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    Reading reading = {};
    if (!parseReply(mp, reading))
        return ProcessMessage::CONTINUE;

    if (!uploadQueue.enqueue(reading, 0)) {
        LOG_WARN("Fishlake poller: upload queue full; dropped READ reply packet 0x%08lx",
                 static_cast<unsigned long>(reading.packetId));
        return ProcessMessage::CONTINUE;
    }

    LOG_INFO("Fishlake poller: captured HOBO reply %.2f C from !5e021e35; queued for cloud",
             reading.temperatureC);
    setIntervalFromNow(0);
    return ProcessMessage::CONTINUE;
}

bool FishlakePollerModule::sendReadCommand()
{
    if (service == nullptr || nodeDB == nullptr)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("Fishlake poller: could not allocate READ packet");
        return false;
    }

    static const char command[] = "READ";
    memcpy(packet->decoded.payload.bytes, command, sizeof(command) - 1);
    packet->decoded.payload.size = sizeof(command) - 1;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = FISHLAKE_NODE_NUM;
    packet->channel = 0;
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    LOG_INFO("Fishlake poller: sent READ to !5e021e35");
    return true;
}

bool FishlakePollerModule::parseReply(const meshtastic_MeshPacket &mp, Reading &reading)
{
    if (getFrom(&mp) != FISHLAKE_NODE_NUM || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return false;

    char text[221] = {};
    size_t length = mp.decoded.payload.size;
    if (length >= sizeof(text))
        length = sizeof(text) - 1;
    memcpy(text, mp.decoded.payload.bytes, length);
    text[length] = '\0';

    char *temp = strstr(text, "Temp:");
    if (temp == nullptr)
        return false;

    float temperatureC = NAN;
    char *slash = strchr(temp, '/');
    if (slash != nullptr) {
        float parsedC = NAN;
        if (sscanf(slash + 1, " %f C", &parsedC) == 1)
            temperatureC = parsedC;
    }

    if (!std::isfinite(temperatureC)) {
        float parsedF = NAN;
        if (sscanf(temp + 5, " %f F", &parsedF) == 1)
            temperatureC = (parsedF - 32.0f) * (5.0f / 9.0f);
    }

    if (!std::isfinite(temperatureC)) {
        float parsedC = NAN;
        if (sscanf(temp + 5, " %f C", &parsedC) == 1)
            temperatureC = parsedC;
    }

    if (!std::isfinite(temperatureC) || temperatureC < -80.0f || temperatureC > 80.0f)
        return false;

    reading.retries = 0;
    reading.packetId = mp.id;
    reading.timestamp = getTime();
    reading.channel = mp.channel;
    reading.hopStart = mp.hop_start;
    reading.hopLimit = mp.hop_limit;
    reading.relayNode = mp.relay_node;
    int radioRssi = mp.rx_rssi;
    if (radioRssi > 0)
        radioRssi -= 200;
    reading.rssi = static_cast<int16_t>(radioRssi);
    reading.snr = mp.rx_snr;
    reading.temperatureC = temperatureC;
    reading.bleRssi = 0;
    snprintf(reading.loggerModel, sizeof(reading.loggerModel), "HOBO");
    reading.loggerMac[0] = '\0';

    if (strncmp(text, "MX2203", 6) == 0)
        snprintf(reading.loggerModel, sizeof(reading.loggerModel), "MX2203");
    else if (strncmp(text, "MX2201", 6) == 0)
        snprintf(reading.loggerModel, sizeof(reading.loggerModel), "MX2201");
    else if (strncmp(text, "MX2001", 6) == 0)
        snprintf(reading.loggerModel, sizeof(reading.loggerModel), "MX2001");

    char *logger = strstr(text, "Logger:");
    if (logger != nullptr)
        copyField(reading.loggerMac, sizeof(reading.loggerMac), logger + 7);

    char *ble = strstr(text, "BLE:");
    if (ble != nullptr) {
        int parsedBle = 0;
        if (sscanf(ble + 4, " %d", &parsedBle) == 1 && parsedBle >= -127 && parsedBle <= 20)
            reading.bleRssi = static_cast<int8_t>(parsedBle);
    }

    return true;
}

bool FishlakePollerModule::upload(const Reading &reading)
{
    if (!WiFi.isConnected())
        return false;
    if (strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) {
        LOG_ERROR("Fishlake poller: INGEST_KEY is empty");
        return false;
    }

    String body;
    body.reserve(900);
    body += "{\"type\":\"telemetry\"";
    body += ",\"timestamp\":" + String(reading.timestamp);
    body += ",\"from\":" + String(FISHLAKE_NODE_NUM);
    body += ",\"packet_id\":" + String(reading.packetId);
    body += ",\"station_name\":\"Fishlake Hightop\"";
    body += ",\"payload\":{";
    body += "\"temperature\":" + String(reading.temperatureC, 3);
    body += ",\"temperature_c\":" + String(reading.temperatureC, 3);
    body += ",\"logger_model\":" + jsonQuoted(reading.loggerModel);
    if (reading.loggerMac[0] != '\0')
        body += ",\"logger_mac\":" + jsonQuoted(reading.loggerMac);
    if (reading.bleRssi != 0)
        body += ",\"ble_rssi_dbm\":" + String(reading.bleRssi);
    body += ",\"source\":\"fishlake_dm_read\"";
    body += "},\"radio\":{";
    body += "\"rssi\":" + String(reading.rssi);
    body += ",\"snr\":" + String(reading.snr, 2);
    body += ",\"hop_start\":" + String(reading.hopStart);
    body += ",\"hop_limit\":" + String(reading.hopLimit);
    body += ",\"hops_away\":" + String(hopsAway(reading.hopStart, reading.hopLimit));
    body += ",\"relay_node\":" + String(reading.relayNode);
    body += ",\"channel\":" + String(reading.channel);
    body += ",\"gateway\":" + jsonQuoted(HOBO_HTTP_GATEWAY_NAME);
    body += "}}";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, HOBO_HTTP_GATEWAY_URL)) {
        LOG_WARN("Fishlake poller: could not initialize HTTPS request");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
    http.addHeader("User-Agent", "cca-heltec-fishlake-poller/1.0");

    const int status = http.POST(body);
    if (status == 200 || status == 201) {
        LOG_INFO("Fishlake poller: cloud stored %.2f C (HTTP %d)", reading.temperatureC, status);
        http.end();
        return true;
    }

    if (status > 0) {
        const String response = http.getString();
        LOG_WARN("Fishlake poller: HTTP %d: %.160s", status, response.c_str());
    } else {
        LOG_WARN("Fishlake poller: POST failed: %s", http.errorToString(status).c_str());
    }
    http.end();
    return false;
}

int32_t FishlakePollerModule::runOnce()
{
    const uint32_t now = millis();

    if (nextPollMs == 0)
        nextPollMs = now + FIRST_POLL_DELAY_MS;

    if (timeReached(now, nextPollMs)) {
        if (sendReadCommand())
            nextPollMs = now + POLL_INTERVAL_MS;
        else
            nextPollMs = now + 60000UL;
    }

    if (!WiFi.isConnected())
        return 1000;

    Reading reading = {};
    if (!uploadQueue.dequeue(&reading, 0))
        return 1000;

    if (upload(reading))
        return 25;

    if (reading.retries < MAX_RETRIES) {
        ++reading.retries;
        if (uploadQueue.enqueue(reading, 0)) {
            const uint32_t delayMs = 1000UL << reading.retries;
            LOG_WARN("Fishlake poller: retry %u/%u in %lu ms",
                     reading.retries, MAX_RETRIES, static_cast<unsigned long>(delayMs));
            return delayMs;
        }
    }

    LOG_ERROR("Fishlake poller: dropping reply packet 0x%08lx after upload failure",
              static_cast<unsigned long>(reading.packetId));
    return 1000;
}

#endif
