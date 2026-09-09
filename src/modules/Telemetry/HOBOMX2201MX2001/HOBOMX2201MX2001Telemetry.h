#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// Experimental compatibility router for the fucking-around branch.
// It preserves the proven HOBO MX2001/MX2201/MX2203 implementation and
// layers the A02YYUW water monitor + Meshtastic read commands on top.
#include "modules/Telemetry/FuckingAroundWater/FuckingAroundWaterTelemetry.h"

using HOBOMX2201MX2001TelemetryModule = FuckingAroundWaterTelemetryModule;

#endif
