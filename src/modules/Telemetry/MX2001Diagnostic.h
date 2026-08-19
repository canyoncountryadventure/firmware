#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"

using MX2001DiagnosticModule = HOBOMX2001MX2201MX2203TelemetryModule;

#endif
