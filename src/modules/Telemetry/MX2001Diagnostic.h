#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.h"

// Keep the existing Modules.cpp hook stable while selecting the RAK-specific
// automatic HOBO-to-mesh behavior on this branch.
using MX2001DiagnosticModule = RAKHoboAutoTelemetryModule;

#endif
