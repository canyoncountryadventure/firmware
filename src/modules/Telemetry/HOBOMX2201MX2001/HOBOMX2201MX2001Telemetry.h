#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// Compatibility router for the existing Meshtastic module hook.
// The production implementation for this branch lives under the correctly
// named three-model folder below.
#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"

using HOBOMX2201MX2001TelemetryModule = HOBOMX2001MX2201MX2203TelemetryModule;

#endif
