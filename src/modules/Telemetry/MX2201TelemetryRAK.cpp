#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Compile the already hardware-proven MX2201 reader under the real RAK4631
// configuration. Dependencies are processed as RAK before the Seeed-only
// implementation gate is exposed for this isolated raw-capture test.
#include "MX2201Telemetry.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "main.h"
#include <bluefruit.h>
#include <cstdint>
#include <cstring>

// The older proven MX2201 reader is hard-coded to the earlier logger MAC.
// For this isolated RAK raw-capture test, redirect its only memcmp() call to
// the current MX2201 that produced the bad 108.4 F reading:
// E4:27:8C:B9:F4:B8
static int rakCurrentMX2201Compare(const void *lhs, const void *rhs, size_t count)
{
    if (count == 6) {
        static const uint8_t currentMX2201RawAddr[6] = {
            0xB8, 0xF4, 0xB9, 0x8C, 0x27, 0xE4
        };
        return std::memcmp(lhs, currentMX2201RawAddr, 6);
    }

    return std::memcmp(lhs, rhs, count);
}

#define memcmp rakCurrentMX2201Compare
#define SEEED_XIAO_NRF52840_KIT 1
#include "MX2201Telemetry.cpp"
#undef SEEED_XIAO_NRF52840_KIT
#undef memcmp

#endif
