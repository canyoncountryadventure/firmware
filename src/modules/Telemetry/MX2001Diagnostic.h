#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"
#include "modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.h"
#include "modules/Telemetry/HOBOSelfRecovery/HOBOFieldCheck.h"
#include "modules/Telemetry/SoilMoisture/SEN0308SoilMoisture.h"

// Preserve the existing Meshtastic RAK hook name while running the universal
// MX2001/MX2201/MX2203 reader, non-destructive self-recovery/watchdog,
// field diagnostics, and the SEN0308 analog soil-moisture station module.
class MX2001DiagnosticModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    MX2001DiagnosticModule() : HOBOMX2001MX2201MX2203TelemetryModule()
    {
        new HOBOSelfRecoveryModule();
        new HOBOFieldCheckModule();
        new SEN0308SoilMoistureModule();
    }
};

#endif
