#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Temporary RAK4631 MX2201 raw-capture validation route.
// Modules.cpp already instantiates MX2001DiagnosticModule on RAK4631; alias
// that hook to the older hardware-proven MX2201 reader for this debug branch.
#include "modules/Telemetry/MX2201Telemetry.h"
using MX2001DiagnosticModule = MX2201TelemetryModule;

#endif
