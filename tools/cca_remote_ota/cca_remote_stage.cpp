#include "cca_remote_stage.h"
#include <nvs.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

namespace {
constexpr const char *NAMESPACE = "cca_remote_ota";
constexpr uint32_t STAGE_OFFSET = 0xA0000; // exactly matches Heltec V10 staging code
constexpr uint8_t MAX_INSTALL_ATTEMPTS = 10;
constexpr size_t CHUNK = 1024;
static uint8_t buf[CHUNK];

void setError(nvs_handle_t h, const char *message, uint8_t phase, uint8_t attempt)
{
    nvs_set_str(h, "error", message);
    nvs_set_u8(h, "phase", phase);
    nvs_set_u8(h, "tries", attempt);
    nvs_commit(h);
    printf("CCA remote OTA: %s\n", message);
}

bool verifyStaged(const esp_partition_t *source, uint32_t offset,
                  uint32_t length, const uint8_t expected[32])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx, 0) != 0) {
        mbedtls_sha256_free(&ctx); return false;
    }
    for (uint32_t pos = 0; pos < length; pos += CHUNK) {
        const size_t count = (length - pos < CHUNK) ? length - pos : CHUNK;
        if (esp_partition_read(source, offset + pos, buf, count) != ESP_OK ||
            mbedtls_sha256_update(&ctx, buf, count) != 0) {
            mbedtls_sha256_free(&ctx); return false;
        }
        vTaskDelay(1);
    }
    uint8_t digest[32] = {};
    const bool matches = mbedtls_sha256_finish(&ctx, digest) == 0 &&
                         memcmp(expected, digest, sizeof(digest)) == 0;
    mbedtls_sha256_free(&ctx);
    return matches;
}

bool applyImage(const esp_partition_t *source, const esp_partition_t *target,
                uint32_t offset, uint32_t length, const uint8_t expected[32])
{
    esp_ota_handle_t handle = 0;
    // Keep the independent loader as boot target until the new app passes
    // esp_ota_end() validation. If power is lost mid-write, loader resumes.
    if (esp_ota_set_boot_partition(source) != ESP_OK) return false;
    if (esp_ota_begin(target, length, &handle) != ESP_OK) return false;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    bool ok = mbedtls_sha256_starts(&sha, 0) == 0;
    for (uint32_t pos = 0; ok && pos < length; pos += CHUNK) {
        const size_t count = length - pos < CHUNK ? length - pos : CHUNK;
        if (esp_partition_read(source, offset + pos, buf, count) != ESP_OK ||
            mbedtls_sha256_update(&sha, buf, count) != 0 ||
            esp_ota_write(handle, buf, count) != ESP_OK) {
            ok = false; break;
        }
        if ((pos % 65536) == 0) vTaskDelay(1);
    }
    uint8_t digest[32] = {};
    if (ok) ok = mbedtls_sha256_finish(&sha, digest) == 0 &&
                 memcmp(digest, expected, sizeof(digest)) == 0;
    mbedtls_sha256_free(&sha);
    if (!ok) {
        esp_ota_abort(handle);
        return false;
    }
    if (esp_ota_end(handle) != ESP_OK) return false;
    return esp_ota_set_boot_partition(target) == ESP_OK;
}
}

void cca_remote_stage_install()
{
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    uint8_t phase = 0, tries = 0;
    nvs_get_u8(h, "phase", &phase);
    nvs_get_u8(h, "tries", &tries);
    if (phase != 1 && phase != 2) { nvs_close(h); return; }

    uint32_t offset = 0, size = 0;
    uint8_t expected[32] = {};
    size_t hashBytes = sizeof(expected);
    char version[65] = {};
    size_t versionLen = sizeof(version);
    const bool recordOk =
        nvs_get_u32(h, "offset", &offset) == ESP_OK &&
        nvs_get_u32(h, "size", &size) == ESP_OK &&
        nvs_get_blob(h, "sha", expected, &hashBytes) == ESP_OK &&
        hashBytes == sizeof(expected) &&
        nvs_get_str(h, "target", version, &versionLen) == ESP_OK;
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *loader = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    const esp_partition_t *app = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);

    if (!recordOk || !running || !loader || !app || running != loader ||
        offset != STAGE_OFFSET || size < 4096 || size > loader->size - STAGE_OFFSET ||
        size > app->size) {
        setError(h, "staged metadata/layout invalid", 0, tries);
        nvs_close(h);
        if (app && esp_ota_set_boot_partition(app) == ESP_OK) esp_restart();
        return;
    }
    if (!verifyStaged(loader, offset, size, expected)) {
        setError(h, "staged image SHA-256 invalid", 0, tries);
        nvs_close(h);
        if (esp_ota_set_boot_partition(app) == ESP_OK) esp_restart();
        return;
    }

    // A crash/power loss leaves phase=2 with OTA_1 selected. The next boot
    // retries using the verified staged image, not an incomplete OTA_0.
    while (tries < MAX_INSTALL_ATTEMPTS) {
        ++tries;
        nvs_set_u8(h, "phase", 2);
        nvs_set_u8(h, "tries", tries);
        nvs_commit(h);
        printf("CCA remote OTA: applying %s, attempt %u/%u\n",
               version, (unsigned)tries, (unsigned)MAX_INSTALL_ATTEMPTS);
        if (applyImage(loader, app, offset, size, expected)) {
            nvs_set_str(h, "applied", version);
            nvs_set_str(h, "error", "");
            nvs_set_u8(h, "phase", 3);
            nvs_set_u8(h, "tries", 0);
            nvs_commit(h);
            nvs_close(h);
            printf("CCA remote OTA: flash verified; rebooting main firmware\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
        setError(h, "app image flash failed", 2, tries);
        vTaskDelay(pdMS_TO_TICKS(2000 + tries * 2000));
    }
    // OTA_0 may now be incomplete. DO NOT set the boot target to OTA_0.
    // Stay in the independent OTA_1 loader; original TCP/3232 and BLE
    // recovery paths remain available for a local recovery image.
    setError(h, "install retry limit reached; loader recovery only", 5, tries);
    nvs_close(h);
}
