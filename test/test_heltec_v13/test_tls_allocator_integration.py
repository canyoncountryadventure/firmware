from pathlib import Path
root=Path(__file__).resolve().parents[2]
allocator=(root/"src/modules/CcaTlsAllocator.cpp").read_text()
main=(root/"src/platform/esp32/main-esp32.cpp").read_text()
gw=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
cloud=(root/"src/modules/CcaCloudRemote.cpp").read_text()
assert allocator.index('#include "HoboHttpGatewayModule.h"') < allocator.index("&& HOBO_HTTP_GATEWAY_ENABLED")
assert "constexpr uint32_t TLS_PSRAM_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;" in allocator
assert "constexpr uint32_t TLS_INTERNAL_CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;" in allocator
assert "constexpr uint32_t EXTERNAL" not in allocator and "constexpr uint32_t INTERNAL" not in allocator
assert "mbedtls_platform_set_calloc_free(allocate, release)" in allocator
assert "mbedtls_calloc(1, 64)" in allocator and "mbedtls_free(probe)" in allocator
assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in allocator and "heap_caps_free(p)" in allocator
assert main.index("CcaTlsAllocator::install()") < main.index("CcaTlsMemory::beginBoot(rebootCounter)")
for field in ("tls_psram=", "tls_ps_allocs=", "tls_int_fallback=", "tls_alloc_fail="):
    assert field in main
assert gw.index("pollMissingAutomatic();") < gw.index("CcaTlsMemory::canAttempt(3)")
assert gw.index("pollConfiguredStations();") < gw.index("CcaTlsMemory::canAttempt(3)")
assert "CcaTlsMemory::canAttempt(5)" in gw
for op in (1,2,4):
    assert f"CcaTlsMemory::canAttempt({op})" in cloud
print("PASS: linked allocator hook, PSRAM verification, independent queue pacing and DM preservation")
