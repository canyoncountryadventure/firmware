#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "HOBOMX2001MX2201MX2203Telemetry.h"

class RAKHoboAutoTelemetryModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    RAKHoboAutoTelemetryModule();

  protected:
    int32_t runOnce() override;

  private:
    bool sendEnvironmentTelemetry();

    uint32_t nextAutomaticReadMs = 0;
    bool automaticReadInProgress = false;
};

#endif
