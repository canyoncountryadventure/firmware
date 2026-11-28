#include "configuration.h"
// This header defines HOBO_HTTP_GATEWAY_ENABLED, including the Heltec default.
// Include it before evaluating the gateway-specific compilation guard.
#include "HoboHttpGatewayModule.h"
#if defined(ARCH_ESP32) && defined(HELTEC_V4) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
#include "CcaTlsAllocator.h"
#include <Arduino.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#if !defined(MBEDTLS_PLATFORM_MEMORY) || defined(MBEDTLS_PLATFORM_CALLOC_MACRO) || defined(MBEDTLS_PLATFORM_FREE_MACRO)
#error "Heltec TLS PSRAM requires the actual SDK Mbed TLS runtime allocator hook"
#endif

namespace CcaTlsAllocator {
namespace {
std::atomic<uint32_t> extCount{0}, fallbackCount{0}, failCount{0};
bool installed = false;
bool attempted = false;
constexpr uint32_t TLS_PSRAM_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr uint32_t TLS_INTERNAL_CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
void *allocate(size_t n, size_t size) {
    if (size && n > SIZE_MAX / size) {
        ++failCount;
        return nullptr;
    }
    if (void *p = heap_caps_calloc(n, size, TLS_PSRAM_CAPS)) {
        ++extCount;
        return p;
    }
    // Preserve at least 24 KiB of internal headroom on fallback; report
    // failure rather than exhausting the Wi-Fi/LoRa task memory reserve.
    const size_t bytes = n * size;
    const size_t free = heap_caps_get_free_size(TLS_INTERNAL_CAPS);
    if (free >= 24576 && bytes <= free - 24576) {
        if (void *p = heap_caps_calloc(n, size, TLS_INTERNAL_CAPS)) {
            ++fallbackCount;
            return p;
        }
    }
    ++failCount;
    return nullptr;
}
void release(void *p) { heap_caps_free(p); }
}
void install() {
    if (attempted) return;
    attempted = true;
    // This is the SDK's real runtime hook, not a macro pretending to
    // reconfigure the precompiled Arduino ESP-IDF libraries.
    if (heap_caps_get_largest_free_block(TLS_PSRAM_CAPS) < 65536) return;
    if (mbedtls_platform_set_calloc_free(allocate, release) != 0) return;
    // Verify the actual linked library calls the hook at runtime.
    const uint32_t before = extCount.load();
    void *probe = mbedtls_calloc(1, 64);
    installed = probe && extCount.load() > before;
    mbedtls_free(probe);
}
bool active() { return installed; }
uint32_t externalAllocations() { return extCount.load(); }
uint32_t internalFallbacks() { return fallbackCount.load(); }
uint32_t allocationFailures() { return failCount.load(); }
}
#endif
