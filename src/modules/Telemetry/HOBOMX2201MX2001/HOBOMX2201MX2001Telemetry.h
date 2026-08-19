#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// Branch-specific compatibility router.
//
// Meshtastic's existing Seeed HOBO construction hook includes this header.
// On the dedicated hobo-mx2203 branch, route that hook to the clean production
// implementation in its own HOBOMX2203 folder. The hobo-mx2201-mx2001 branch
// remains unchanged and continues to route to its proven combined reader.
#include "modules/Telemetry/HOBOMX2203/HOBOMX2203Telemetry.h"

using HOBOMX2201MX2001TelemetryModule = HOBOMX2203TelemetryModule;

#endif
