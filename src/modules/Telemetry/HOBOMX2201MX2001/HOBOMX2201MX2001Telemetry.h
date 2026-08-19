#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// TEST-BRANCH OVERRIDE ONLY.
//
// The production hobo-mx2201-mx2001 branch keeps this entrypoint mapped to the
// proven combined MX2201/MX2001 reader. On mx2203-discovery-test we temporarily
// route the same Seeed module slot to the isolated MX2203 discovery scanner so
// no production code or behavior is changed.
#include "modules/Telemetry/MX2203Discovery/MX2203Discovery.h"

using HOBOMX2201MX2001TelemetryModule = MX2203DiscoveryModule;

#endif
