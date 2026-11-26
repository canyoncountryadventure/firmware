#include "CcaCloudRemote.h"
#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include "HoboHttpGatewayModule.h"
#include "CcaRemoteOtaStage.h"
#include "serialization/JSON.h"
#include "serialization/JSONValue.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <memory>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>
#include <esp_heap_caps.h>
#include <esp_system.h>
extern void esp32FieldDiagFormat(char *out, size_t outSize);

namespace {
constexpr size_t MAX_CLOUD_NODES = 24;
constexpr size_t MAX_DM_OVERRIDES = 16;
constexpr uint32_t CONFIG_INTERVAL_MS = 300000UL; // five minutes
constexpr uint32_t HEALTH_INTERVAL_MS = 900000UL; // 15 minutes
constexpr uint32_t BOOT_DELAY_MS = 20000UL;
const uint32_t INITIAL_CLOUD_NODES[] = {1439303287UL, 1668198402UL}; // DYPA08 1 and 2

portMUX_TYPE cloudLock = portMUX_INITIALIZER_UNLOCKED;
uint32_t cloudNodes[MAX_CLOUD_NODES] = {1439303287UL, 1668198402UL};
size_t cloudCount = 2;
uint32_t cloudVersion = 0;
CcaRemoteDmSetting dmOverrides[MAX_DM_OVERRIDES] = {};
size_t dmCount = 0;
uint32_t dmRevision = 0;
bool otaAvailable = false;
char otaVersion[64] = {};
char otaReason[48] = "not authorized";
int lastCloudCode = 0;
int lastHealthCode = 0;
char lastConfigError[128] = "not attempted";
uint8_t configFailureStreak = 0;
uint32_t tlsFreeHeap = 0;
uint32_t tlsLargestHeapBlock = 0;
int tlsWiFiRssi = 0;
uint32_t successCount = 0;
uint32_t failCount = 0;
uint32_t loopStackMinFreeBytes = 0; // ESP-IDF stack high-water mark, bytes

bool numericValue(JSONValue *v, uint32_t &out)
{
    if (!v || !v->IsNumber()) return false;
    const double n = v->AsNumber();
    if (n < 1.0 || n > 4294967295.0 || n != static_cast<uint32_t>(n)) return false;
    out = static_cast<uint32_t>(n);
    return true;
}

String safeJsonString(const String &s)
{
    String result = "\"";
    for (size_t i = 0; i < s.length(); ++i) {
        const char c = s[i];
        if (c == '\"' || c == '\\') { result += '\\'; result += c; }
        else if (c >= ' ') result += c;
    }
    result += "\"";
    return result;
}

bool hexDigit(char c, uint8_t &n)
{
    if (c >= '0' && c <= '9') { n = c - '0'; return true; }
    if (c >= 'a' && c <= 'f') { n = c - 'a' + 10; return true; }
    if (c >= 'A' && c <= 'F') { n = c - 'A' + 10; return true; }
    return false;
}

bool verifyHmac(const String &message, const String &signature)
{
    const char *secret = HOBO_HTTP_GATEWAY_INGEST_KEY;
    if (!secret || !secret[0] || signature.length() != 64) return false;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return false;
    uint8_t mac[32] = {};
    if (mbedtls_md_hmac(info, reinterpret_cast<const uint8_t *>(secret), strlen(secret),
                        reinterpret_cast<const uint8_t *>(message.c_str()), message.length(), mac) != 0)
        return false;
    unsigned int diff = 0;
    for (unsigned int i = 0; i < 32; ++i) {
        uint8_t hi = 0, lo = 0;
        if (!hexDigit(signature[i * 2], hi) || !hexDigit(signature[i * 2 + 1], lo)) return false;
        diff |= mac[i] ^ static_cast<uint8_t>((hi << 4) | lo);
    }
    return diff == 0;
}
}

CcaCloudRemote *ccaCloudRemote = nullptr;

CcaCloudRemote::CcaCloudRemote() : concurrency::OSThread("cca_cloud_remote")
{
    bootAt = millis();
    Preferences p;
    if (p.begin("cca_cf_remote", true)) {
        const uint8_t count = p.getUChar("count", 0);
        if (count > 0 && count <= MAX_CLOUD_NODES &&
            p.getBytesLength("nodes") == count * sizeof(uint32_t)) {
            uint32_t stored[MAX_CLOUD_NODES] = {};
            if (p.getBytes("nodes", stored, count * sizeof(uint32_t)) == count * sizeof(uint32_t)) {
                portENTER_CRITICAL(&cloudLock);
                memcpy(cloudNodes, stored, count * sizeof(uint32_t));
                cloudCount = count;
                cloudVersion = p.getUInt("version", 0);
                portEXIT_CRITICAL(&cloudLock);
            }
        }
        p.end();
    }
    setIntervalFromNow(BOOT_DELAY_MS);
}

bool CcaCloudRemote::isCloudNode(uint32_t id)
{
    bool found = false;
    portENTER_CRITICAL(&cloudLock);
    for (size_t i = 0; i < cloudCount; ++i)
        if (cloudNodes[i] == id) { found = true; break; }
    portEXIT_CRITICAL(&cloudLock);
    return found;
}

uint32_t CcaCloudRemote::configVersion()
{
    portENTER_CRITICAL(&cloudLock);
    const uint32_t v = cloudVersion;
    portEXIT_CRITICAL(&cloudLock);
    return v;
}

uint32_t CcaCloudRemote::remoteDmRevision()
{
    portENTER_CRITICAL(&cloudLock);
    const uint32_t v = dmRevision;
    portEXIT_CRITICAL(&cloudLock);
    return v;
}

size_t CcaCloudRemote::readRemoteDmSettings(CcaRemoteDmSetting *out, size_t capacity)
{
    if (!out || !capacity) return 0;
    portENTER_CRITICAL(&cloudLock);
    const size_t count = dmCount < capacity ? dmCount : capacity;
    memcpy(out, dmOverrides, count * sizeof(CcaRemoteDmSetting));
    portEXIT_CRITICAL(&cloudLock);
    return count;
}

String CcaCloudRemote::statusJson()
{
    uint32_t v = 0, revision = 0;
    size_t stations = 0;
    bool canStage = false;
    portENTER_CRITICAL(&cloudLock);
    v = cloudVersion;
    stations = cloudCount;
    revision = dmRevision;
    canStage = otaAvailable;
    portEXIT_CRITICAL(&cloudLock);
    String s = "{\"build\":\"heltec-gateway-v12\",\"remote_config_version\":" + String(v) +
               ",\"cloud_nodes\":" + String(stations) +
               ",\"remote_dm_revision\":" + String(revision) +
               ",\"sync_ok\":" + String(successCount) +
               ",\"sync_failed\":" + String(failCount) +
               ",\"loop_stack_min_free_bytes\":" + String(loopStackMinFreeBytes) +
               ",\"last_http_status\":" + String(lastCloudCode) +
               ",\"last_health_http_status\":" + String(lastHealthCode) +
               ",\"last_config_error\":" + safeJsonString(String(lastConfigError)) +
               ",\"tls_free_heap\":" + String(tlsFreeHeap) +
               ",\"tls_largest_heap_block\":" + String(tlsLargestHeapBlock) +
               ",\"tls_wifi_rssi_dbm\":" + String(tlsWiFiRssi) +
               ",\"ota_manifest_verified\":" + String(canStage ? "true" : "false") +
               ",\"ota_install_enabled\":" + String((canStage && CcaRemoteOtaStage::loaderReady()) ? "true" : "false") +
               ",\"ota_status\":" +
               safeJsonString(otaReason) + ",\"ota_stage\":" + CcaRemoteOtaStage::statusJson() + "}";
    return s;
}

bool CcaCloudRemote::verifyOtaManifest(const String &version, const String &url, const String &sha,
                                      const String &signature)
{
    // The Heltec's Meshtastic OTA loader is NOT a generic app-slot updater.
    // Check/approve manifests here, but never overwrite its loader partition.
    // An installer must be validated against the Heltec V4 OTA partition
    // scheme before OTA install is enabled in a later audited release.
    if (version.isEmpty() || version.length() > 63 || sha.length() != 64 ||
        signature.length() != 64) return false;
    const String cloudPrefix = "https://cces-mesh-remote.raspy-lake-a7f4.workers.dev/v10/firmware/";
    bool cloudAllowed = url.startsWith(cloudPrefix) && url.length() == cloudPrefix.length() + 64;
    if (cloudAllowed) {
        for (size_t i = cloudPrefix.length(); i < url.length(); ++i) {
            uint8_t unused = 0;
            if (!hexDigit(url[i], unused)) { cloudAllowed = false; break; }
        }
    }
    if (!cloudAllowed &&
        !url.startsWith("https://raw.githubusercontent.com/canyoncountryadventure/firmware/") &&
        !url.startsWith("https://github.com/canyoncountryadventure/firmware/")) return false;
    for (size_t i = 0; i < sha.length(); ++i) {
        uint8_t unused = 0;
        if (!hexDigit(sha[i], unused)) return false;
    }
    return verifyHmac(version + "\n" + url + "\n" + sha, signature);
}

bool CcaCloudRemote::syncConfig()
{
    if (!WiFi.isConnected() || strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) {
        snprintf(lastConfigError, sizeof(lastConfigError), "WiFi disconnected or gateway key missing");
        lastCloudCode = -100;
        return false;
    }
    tlsFreeHeap = ESP.getFreeHeap();
    tlsLargestHeapBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    tlsWiFiRssi = WiFi.RSSI();
    String response;
    // Complete and destroy the Cloudflare TLS client before processing the
    // signed manifest.  stageAndReboot() opens a second HTTPS connection
    // (GitHub) and must not overlap the Cloudflare TLS heap allocation.
    {
        WiFiClientSecure client;
        client.setInsecure(); // Mirrors V9 gateway transport.
        HTTPClient http;
        client.setTimeout(12000);
        http.setConnectTimeout(12000);
        http.setTimeout(12000);
        const String endpoint = String(CCA_CLOUD_URL) + "/v10/config";
        if (!http.begin(client, endpoint)) {
            lastCloudCode = -101;
            snprintf(lastConfigError, sizeof(lastConfigError), "HTTP setup failed heap=%lu maxblock=%lu rssi=%d",
                     static_cast<unsigned long>(tlsFreeHeap), static_cast<unsigned long>(tlsLargestHeapBlock), tlsWiFiRssi);
            return false;
        }
        http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
        const int status = http.GET();
        lastCloudCode = status;
        if (status != 200) {
            const String detail = http.errorToString(status);
            snprintf(lastConfigError, sizeof(lastConfigError),
                     "HTTP %d %s heap=%lu maxblock=%lu rssi=%d", status,
                     detail.c_str(), static_cast<unsigned long>(tlsFreeHeap),
                     static_cast<unsigned long>(tlsLargestHeapBlock), tlsWiFiRssi);
            LOG_WARN("CCA V11 Cloudflare config: %s", lastConfigError);
            http.end();
            return false;
        }
        response = http.getString();
        http.end();
    }
    if (response.length() < 30 || response.length() > 8192) {
        snprintf(lastConfigError, sizeof(lastConfigError), "Invalid config response length: %lu", static_cast<unsigned long>(response.length()));
        return false;
    }

    std::unique_ptr<JSONValue> root(JSON::Parse(response.c_str()));
    if (!root || !root->IsObject()) return false;
    JSONValue *gateway = root->Child("gateway");
    JSONValue *v = root->Child("version");
    JSONValue *nodes = root->Child("nodes");
    if (!gateway || !gateway->IsString() || gateway->AsString() != "heltec-moab" ||
        !nodes || !nodes->IsArray() || !v || !v->IsNumber()) return false;
    const uint32_t version = static_cast<uint32_t>(v->AsNumber());
    if (version == 0 || v->AsNumber() != version) return false;
    const JSONArray &items = nodes->AsArray();
    if (items.empty() || items.size() > MAX_CLOUD_NODES) return false;
    uint32_t proposed[MAX_CLOUD_NODES] = {};
    size_t count = 0;
    for (JSONValue *item : items) {
        if (!item || !item->IsObject()) return false;
        uint32_t id = 0;
        if (!numericValue(item->Child("id"), id)) return false;
        JSONValue *dest = item->Child("destination");
        if (!dest || !dest->IsString() || dest->AsString() != "cloudflare") return false;
        bool duplicate = false;
        for (size_t j = 0; j < count; ++j) if (proposed[j] == id) duplicate = true;
        if (!duplicate) proposed[count++] = id;
    }
    if (!count || version < configVersion()) return false;

    CcaRemoteDmSetting proposedDm[MAX_DM_OVERRIDES] = {};
    size_t newDmCount = 0;
    JSONValue *dm = root->Child("auto_dm");
    if (dm && dm->IsArray()) {
        const JSONArray &arr = dm->AsArray();
        if (arr.size() > MAX_DM_OVERRIDES) return false;
        for (JSONValue *item : arr) {
            if (!item || !item->IsObject()) return false;
            uint32_t id = 0;
            if (!numericValue(item->Child("id"), id)) return false;
            JSONValue *enabled = item->Child("enabled");
            JSONValue *interval = item->Child("interval_minutes");
            if (!enabled || !enabled->IsBool() || !interval || !interval->IsNumber()) return false;
            const uint32_t minutes = static_cast<uint32_t>(interval->AsNumber());
            if ((minutes != 15 && minutes != 30 && minutes != 45 &&
                 minutes != 60 && minutes != 360 && minutes != 720 && minutes != 1440) ||
                interval->AsNumber() != minutes) return false;
            proposedDm[newDmCount++] = {id, static_cast<uint16_t>(minutes), enabled->AsBool()};
        }
    }

    // OTA is a signed manifest CHECK only in V10. It cannot install unattended.
    JSONValue *ota = root->Child("ota");
    bool approved = false;
    String authorizedVersion, authorizedUrl, authorizedHash;
    if (ota && ota->IsObject()) {
        JSONValue *enabled = ota->Child("enabled"), *rv = ota->Child("version"),
                  *url = ota->Child("url"), *sha = ota->Child("sha256"),
                  *sig = ota->Child("signature");
        if (enabled && enabled->IsBool() && enabled->AsBool() &&
            rv && rv->IsString() && url && url->IsString() &&
            sha && sha->IsString() && sig && sig->IsString()) {
            const String vr = rv->AsString().c_str();
            approved = verifyOtaManifest(vr, String(url->AsString().c_str()),
                                         String(sha->AsString().c_str()), String(sig->AsString().c_str()));
            if (approved) {
                snprintf(otaVersion, sizeof(otaVersion), "%s", vr.c_str());
                authorizedVersion = vr;
                authorizedUrl = url->AsString().c_str();
                authorizedHash = sha->AsString().c_str();
            }
        }
    }

    // Update the in-memory allowlist first; existing hardcoded V9 stations are
    // not removed or redirected. Persist only a verified complete new snapshot.
    const bool configChanged = version != configVersion();
    portENTER_CRITICAL(&cloudLock);
    memcpy(cloudNodes, proposed, count * sizeof(uint32_t));
    cloudCount = count;
    cloudVersion = version;
    dmCount = newDmCount;
    if (newDmCount) memcpy(dmOverrides, proposedDm, newDmCount * sizeof(CcaRemoteDmSetting));
    if (configChanged) ++dmRevision;
    otaAvailable = approved;
    portEXIT_CRITICAL(&cloudLock);
    snprintf(otaReason, sizeof(otaReason), "%s", approved ? "signed manifest verified; install disabled" : "not authorized");

    if (configChanged) {
        Preferences p;
        if (p.begin("cca_cf_remote", false)) {
            p.putBytes("nodes", proposed, count * sizeof(uint32_t));
            p.putUChar("count", count);
            p.putUInt("version", version);
            p.end();
        }
    }
    // Only an explicitly authorized signed manifest may start staging. The
    // new loader is independently attested before any flash is written.
    // The default Cloudflare ota_enabled=0 prevents staging until approved.
    if (approved) {
        String stageReason;
        CcaRemoteOtaStage::stageAndReboot(authorizedVersion, authorizedUrl,
                                          authorizedHash, stageReason);
        snprintf(otaReason, sizeof(otaReason), "%s", stageReason.c_str());
    }
    snprintf(lastConfigError, sizeof(lastConfigError), "none");
    LOG_INFO("CCA V11 remote: config v%lu, %u cloud station(s), %u DM override(s), ota_manifest=%s",
             static_cast<unsigned long>(version), static_cast<unsigned>(count),
             static_cast<unsigned>(newDmCount), approved ? "VERIFIED" : "OFF");
    return true;
}

void CcaCloudRemote::postHealth()
{
    if (!WiFi.isConnected() || strlen(HOBO_HTTP_GATEWAY_INGEST_KEY) == 0) return;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    const String endpoint = String(CCA_CLOUD_URL) + "/v10/health";
    if (!http.begin(client, endpoint)) return;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Ingest-Key", HOBO_HTTP_GATEWAY_INGEST_KEY);
    const uint32_t queue = hoboHttpGatewayModule ? hoboHttpGatewayModule->healthQueueUsed() : 0;
    const int httpStatus = hoboHttpGatewayModule ? hoboHttpGatewayModule->healthLastHttpStatus() : 0;
    String body = "{\"firmware\":\"HELTEC-GATEWAY-V12\",\"uptime_seconds\":" +
        String(millis() / 1000UL) + ",\"queue_depth\":" + String(queue) +
        ",\"last_http_status\":" + String(httpStatus) +
        ",\"config_version\":" + String(configVersion()) +
        ",\"note\":" + safeJsonString(CcaRemoteOtaStage::healthNote());
    char diag[530] = {};
    esp32FieldDiagFormat(diag, sizeof(diag));
    String diagnostic(diag);
    diagnostic.replace("\\n", " | ");
    body += ",\"reset_reason_code\":" + String(static_cast<int>(esp_reset_reason()));
    body += ",\"reset_diagnostic\":" + safeJsonString(diagnostic);
    body += ",\"auto_dm_state\":" + (hoboHttpGatewayModule ? hoboHttpGatewayModule->autoDmSnapshotJson() : String("null"));
    body += "}";
    const int rc = http.POST(body);
    lastHealthCode = rc;
    if (rc == 200) CcaRemoteOtaStage::markCloudConfirmed();
    else LOG_WARN("CCA V12 remote: health HTTP %d: %s", rc, http.errorToString(rc).c_str());
    http.end();
}

int32_t CcaCloudRemote::runOnce()
{
    const uint32_t now = millis();
    if ((now - bootAt) < BOOT_DELAY_MS) return 3000;
    if (!WiFi.isConnected()) return 10000; // preserve cached configuration offline
    if (!lastConfigPoll || (now - lastConfigPoll) >= CONFIG_INTERVAL_MS) {
        lastConfigPoll = now;
        if (syncConfig()) {
            ++successCount;
            configFailureStreak = 0;
        } else {
            ++failCount;
            if (configFailureStreak < 5) ++configFailureStreak;
            // Retry at 30, 60, 120, then 240 seconds; normal 5-minute
            // configuration cadence resumes on the next successful GET.
            const uint32_t retryMs = 30000UL << (configFailureStreak < 4 ? configFailureStreak - 1 : 3);
            lastConfigPoll = now + retryMs - CONFIG_INTERVAL_MS;
        }
    }
    if (!lastHealthPost || (now - lastHealthPost) >= HEALTH_INTERVAL_MS) {
        lastHealthPost = now;
        postHealth();
    }
    // This is the actual Arduino loopTask, so the high-water mark captures
    // the minimum stack remaining after the HTTPS handshake and JSON parse.
    loopStackMinFreeBytes = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    return 2000;
}
#endif
