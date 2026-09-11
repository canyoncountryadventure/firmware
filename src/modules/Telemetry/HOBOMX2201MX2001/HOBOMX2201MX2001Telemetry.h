#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"
#include "modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.h"

// Keep the proven universal HOBO reader as the primary module and attach a
// separate non-destructive supervisor for watchdog, BLE power tuning, health
// diagnostics and remote recovery commands.
class HOBOMX2201MX2001TelemetryModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    HOBOMX2201MX2001TelemetryModule() : HOBOMX2001MX2201MX2203TelemetryModule()
    {
        new HOBOSelfRecoveryModule();
    }
};

#endif
