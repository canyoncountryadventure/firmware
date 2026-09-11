#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"
#include "modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.h"
#include "modules/Telemetry/HOBOSelfRecovery/HOBOFieldCheck.h"

// Preserve the existing Meshtastic RAK hook name while running the same
// universal MX2001/MX2201/MX2203 reader plus non-destructive self-recovery,
// diagnostics, remote recovery commands, and a simple field packet check.
class MX2001DiagnosticModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    MX2001DiagnosticModule() : HOBOMX2001MX2201MX2203TelemetryModule()
    {
        new HOBOSelfRecoveryModule();
        new HOBOFieldCheckModule();
    }
};

#endif
