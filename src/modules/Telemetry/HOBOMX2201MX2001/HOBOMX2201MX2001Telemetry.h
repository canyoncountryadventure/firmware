#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// TEST-BRANCH OVERRIDE ONLY.
//
// The production hobo-mx2201-mx2001 branch remains mapped to the proven
// combined MX2201/MX2001 reader. On mx2203-discovery-test this Seeed module
// slot is temporarily routed to the isolated MX2203 calibration sampler.
#include "modules/Telemetry/MX2203Calibration/MX2203Calibration.h"

using HOBOMX2201MX2001TelemetryModule = MX2203CalibrationModule;

#endif
