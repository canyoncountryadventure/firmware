#include "CcaRemoteOtaStage.h"
#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <cstring>
#include <cstdio>

namespace {
// The Heltec V4's Meshtastic 16 MiB partition map uses OTA_0 at 0x10000
// and OTA_1 (independent recovery loader) at 0x650000, each 0x640000 bytes.
// NEVER write below 0xA0000 within OTA_1, where the loader lives.
constexpr size_t LOADER_RESERVED_BYTES = 0xA0000;
constexpr size_t ERASE_BYTES = 4096;
constexpr size_t CHUNK_BYTES = 1024;
constexpr uint8_t MAX_STAGING_ATTEMPTS = 10;
constexpr const char *NAMESPACE = "cca_remote_ota";
constexpr const char *LOADER_VERSION_PREFIX = "CCA_PULL_1";
uint32_t lastAttemptMs = 0;

bool unpackSha(const String &hex, uint8_t out[32])
{
    if (hex.length() != 64) return false;
    for (size_t i = 0; i < 32; ++i) {
        const char h = hex[2 * i], l = hex[2 * i + 1];
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int hi = digit(h), lo = digit(l);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

String safe(const String &v)
{
    String out = "\"";
    for (size_t i = 0; i < v.length(); ++i) {
        const char c = v[i];
        if (c == '\"' || c == '\\') { out += '\\'; out += c; }
        else if (static_cast<unsigned char>(c) >= 32) out += c;
    }
    return out + "\"";
}

bool shaFlash(const esp_partition_t *part, uint32_t offset, size_t length, const uint8_t expected[32])
{
    uint8_t buffer[CHUNK_BYTES] = {};
    uint8_t actual[32] = {};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    for (size_t pos = 0; pos < length; pos += CHUNK_BYTES) {
        const size_t n = (length - pos < CHUNK_BYTES) ? length - pos : CHUNK_BYTES;
        if (esp_partition_read(part, offset + pos, buffer, n) != ESP_OK) {
            mbedtls_sha256_free(&ctx);
            return false;
        }
        mbedtls_sha256_update(&ctx, buffer, n);
        delay(0);
    }
    mbedtls_sha256_finish(&ctx, actual);
    const bool ok = memcmp(actual, expected, sizeof(actual)) == 0;
    mbedtls_sha256_free(&ctx);
    return ok;
}

void recordFailure(const String &version, const char *reason)
{
    Preferences p;
    if (!p.begin(NAMESPACE, false)) return;
    const String prior = p.getString("target", "");
    uint8_t tries = prior == version ? p.getUChar("tries", 0) : 0;
    if (tries < MAX_STAGING_ATTEMPTS) ++tries;
    p.putString("target", version);
    p.putUChar("tries", tries);
    p.putUChar("phase", 0);
    p.putString("error", reason);
    p.end();
}

bool prepareMeshtasticLoader(const uint8_t expected[32])
{
    const String ssid = WiFi.SSID(), pass = WiFi.psk();
    if (!ssid.length()) return false;
    Preferences p;
    if (!p.begin("MeshtasticOTA", false)) return false;
    bool ok = p.putBytes("ota_hash", expected, 32) == 32;
    ok = p.putUChar("method", 2) == 1 && ok; // WiFi OTA
    ok = p.putString("ssid", ssid) == ssid.length() && ok;
    ok = p.putString("psk", pass) == pass.length() && ok;
    p.end();
    return ok;
}
}

namespace CcaRemoteOtaStage {
bool loaderReady()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (!running || running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        !loader || loader->size <= LOADER_RESERVED_BYTES + ERASE_BYTES) return false;
    esp_app_desc_t desc = {};
    return esp_ota_get_partition_description(loader, &desc) == ESP_OK &&
           strncmp(desc.version, LOADER_VERSION_PREFIX, strlen(LOADER_VERSION_PREFIX)) == 0;
}
String statusJson()
{
    Preferences p;
    if (!p.begin(NAMESPACE, true)) return "{\"state\":\"unavailable\"}";
    const uint8_t phase = p.getUChar("phase", 0);
    const uint8_t attempts = p.getUChar("tries", 0);
    const String target = p.getString("target", "");
    const String applied = p.getString("applied", "");
    const String error = p.getString("error", "");
    p.end();
    String state = "idle";
    if (phase == 1) state = "staged";
    else if (phase == 2) state = "installing";
    else if (phase == 3) state = "installed";
    else if (phase == 4) state = "verified-boot";
    else if (attempts >= MAX_STAGING_ATTEMPTS) state = "retry-limit";
    else if (attempts) state = "retry-pending";
    return "{\"loader_ready\":" + String(loaderReady() ? "true" : "false") + ",\"state\":" + safe(state) + ",\"attempts\":" + String(attempts) +
           ",\"target\":" + safe(target) + ",\"applied\":" + safe(applied) +
           ",\"error\":" + safe(error) + "}";
}

void markCloudConfirmed()
{
    Preferences p;
    if (!p.begin(NAMESPACE, false)) return;
    if (p.getUChar("phase", 0) == 3) p.putUChar("phase", 4);
    p.end();
}

String healthNote()
{
    Preferences p;
    if (!p.begin(NAMESPACE, false)) return "remote-ota: nvs unavailable";
    const uint8_t phase = p.getUChar("phase", 0);
    const uint8_t tries = p.getUChar("tries", 0);
    const String applied = p.getString("applied", "");
    p.end();
    return "remote-ota: loader_ready=" + String(loaderReady() ? "1" : "0") + ",phase=" + String(phase) +
           ",attempts=" + String(tries) + ",applied=" + applied;
}

bool stageAndReboot(const String &version, const String &url, const String &sha256, String &reason)
{
    reason = "checking prerequisites";
    if (!WiFi.isConnected()) { reason = "WiFi offline"; return false; }
    if ((millis() - lastAttemptMs) < 300000UL && lastAttemptMs) {
        reason = "waiting for next retry window";
        return false;
    }
    lastAttemptMs = millis();
    if (!url.startsWith("https://raw.githubusercontent.com/canyoncountryadventure/firmware/") &&
        !url.startsWith("https://github.com/canyoncountryadventure/firmware/")) {
        reason = "unapproved download host"; return false;
    }
    uint8_t expected[32] = {};
    if (!unpackSha(sha256, expected)) { reason = "invalid hash"; return false; }
    Preferences p;
    if (!p.begin(NAMESPACE, true)) { reason = "NVS unavailable"; return false; }
    const String applied = p.getString("applied", "");
    const String target = p.getString("target", "");
    const uint8_t priorTries = p.getUChar("tries", 0);
    const uint8_t phase = p.getUChar("phase", 0);
    p.end();
    if (version == applied) { reason = "already installed"; return false; }
    if (version == target && phase == 1) { reason = "already staged"; return false; }
    if (version == target && priorTries >= MAX_STAGING_ATTEMPTS) {
        reason = "retry limit reached"; return false;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (!running || running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        !loader || loader->size <= LOADER_RESERVED_BYTES + ERASE_BYTES) {
        reason = "unsupported partition layout"; return false;
    }
    esp_app_desc_t desc = {};
    if (esp_ota_get_partition_description(loader, &desc) != ESP_OK ||
        strncmp(desc.version, LOADER_VERSION_PREFIX, strlen(LOADER_VERSION_PREFIX)) != 0) {
        reason = "CCA recovery loader not installed"; return false;
    }
    // A running image in OTA_0 and the verified CCA loader in OTA_1 are
    // mandatory. Generic Arduino Update.begin() would destroy our loader.
    WiFiClientSecure client;
    client.setInsecure(); // Transport hash is cryptographically pinned by the signed manifest.
    HTTPClient http;
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) { reason = "download open failed"; recordFailure(version, reason.c_str()); return false; }
    const int rc = http.GET();
    if (rc != 200) {
        reason = "download HTTP " + String(rc);
        http.end(); recordFailure(version, reason.c_str()); return false;
    }
    const int length = http.getSize();
    if (length <= 0 || static_cast<size_t>(length) > loader->size - LOADER_RESERVED_BYTES) {
        reason = "firmware size outside safe staging area";
        http.end(); recordFailure(version, reason.c_str()); return false;
    }
    const uint32_t rounded = (static_cast<uint32_t>(length) + ERASE_BYTES - 1) & ~(ERASE_BYTES - 1);
    if (esp_partition_erase_range(loader, LOADER_RESERVED_BYTES, rounded) != ESP_OK) {
        reason = "staging erase failed";
        http.end(); recordFailure(version, reason.c_str()); return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[CHUNK_BYTES] = {};
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    bool ok = true;
    size_t copied = 0;
    while (ok && copied < static_cast<size_t>(length)) {
        const size_t wanted = (static_cast<size_t>(length) - copied < CHUNK_BYTES)
                            ? static_cast<size_t>(length) - copied : CHUNK_BYTES;
        const size_t got = stream->readBytes(buffer, wanted);
        // Do not round/interleave short network reads: every flash offset
        // must remain 4-byte aligned until the very last chunk.
        if (got != wanted) { ok = false; reason = "download short read"; break; }
        mbedtls_sha256_update(&sha, buffer, got);
        // Flash writes are 4-byte aligned. Pad only the final chunk, while the
        // integrity hash covers precisely the Content-Length image bytes.
        const size_t aligned = (got + 3) & ~size_t(3);
        if (aligned > got) memset(buffer + got, 0xff, aligned - got);
        if (esp_partition_write(loader, LOADER_RESERVED_BYTES + copied, buffer, aligned) != ESP_OK) {
            ok = false; reason = "staging flash write failed"; break;
        }
        copied += got;
        delay(0);
    }
    uint8_t actual[32] = {};
    if (ok) {
        mbedtls_sha256_finish(&sha, actual);
        ok = memcmp(actual, expected, sizeof(actual)) == 0;
    }
    mbedtls_sha256_free(&sha);
    http.end();
    if (!ok || copied != static_cast<size_t>(length) ||
        !shaFlash(loader, LOADER_RESERVED_BYTES, copied, expected)) {
        if (reason == "checking prerequisites") reason = "download/hash verification failed";
        recordFailure(version, reason.c_str());
        return false;
    }
    if (!prepareMeshtasticLoader(expected)) {
        reason = "loader WiFi credentials not saved";
        recordFailure(version, reason.c_str()); return false;
    }
    if (!p.begin(NAMESPACE, false)) {
        reason = "NVS commit failed"; recordFailure(version, reason.c_str()); return false;
    }
    bool stored = p.putString("target", version) == version.length();
    stored = (p.putBytes("sha", expected, sizeof(expected)) == sizeof(expected)) && stored;
    stored = (p.putUInt("size", length) == sizeof(uint32_t)) && stored;
    stored = (p.putUInt("offset", LOADER_RESERVED_BYTES) == sizeof(uint32_t)) && stored;
    stored = (p.putUChar("tries", 0) == 1) && stored;
    stored = (p.putString("error", "") == 0) && stored;
    stored = (p.putUChar("phase", 1) == 1) && stored;
    p.end();
    if (!stored) { reason = "NVS stage commit failed"; return false; }
    if (esp_ota_set_boot_partition(loader) != ESP_OK) {
        reason = "unable to boot recovery loader";
        recordFailure(version, reason.c_str()); return false;
    }
    reason = "verified image staged; rebooting into CCA loader";
    ESP.restart();
    return true;
}
} // namespace CcaRemoteOtaStage
#endif
