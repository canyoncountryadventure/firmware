#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

// Final production-facing name for the hardware-proven combined MX2201/MX2001
// reader. The implementation remains in the bench-proven HOBOUniversalTest
// source so this final branch does not alter the tested BLE/protocol behavior.
#include "modules/Telemetry/HOBOUniversalTest/HOBOUniversalTest.h"

using HOBOMX2201MX2001TelemetryModule = HOBOUniversalTestModule;

#endif
