#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Load all shared dependencies under the real RAK4631 board configuration
// before reusing the hardware-proven universal implementation below.
#include "HOBOMX2001MX2201MX2203Telemetry.h"
#include "../../../mesh/generated/meshtastic/telemetry.pb.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "SPILock.h"
#include "main.h"
#include "pb_encode.h"
#include <bluefruit.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// The universal implementation has only one Seeed-specific condition: its
// outer translation-unit guard. Dependencies above have already been parsed
// as RAK4631, so this local definition exposes the identical protocol/state
// machine without changing board configuration or maintaining a forked copy.
#define SEEED_XIAO_NRF52840_KIT 1
#include "HOBOMX2001MX2201MX2203Telemetry.cpp"
#undef SEEED_XIAO_NRF52840_KIT

#endif
