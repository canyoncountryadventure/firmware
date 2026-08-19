#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Compile the already hardware-proven MX2201 reader under the real RAK4631
// configuration. Dependencies are processed as RAK before the Seeed-only
// implementation gate is exposed for this isolated raw-capture test.
//
// This test intentionally uses the original reader's hard-coded MX2201 target:
// EB:9A:E4:52:6D:5F — the logger that produced the bad 108.4 F universal reading.
#include "MX2201Telemetry.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "main.h"
#include <bluefruit.h>
#include <cstdint>
#include <cstring>

#define SEEED_XIAO_NRF52840_KIT 1
#include "MX2201Telemetry.cpp"
#undef SEEED_XIAO_NRF52840_KIT

#endif
