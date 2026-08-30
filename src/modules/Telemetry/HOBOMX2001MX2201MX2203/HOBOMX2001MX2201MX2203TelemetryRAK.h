#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "HOBOMX2001MX2201MX2203Telemetry.h"

// RAK deployment policy layer: normal Meshtastic mesh + HOBO BLE automatic
// telemetry. No custom PIR/trail-counter behavior belongs in this module.
class RAKHoboAutoTelemetryModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    RAKHoboAutoTelemetryModule();

  protected:
    int32_t runOnce() override;

  private:
    bool sendEnvironmentTelemetry();
    bool requestLoggerInterval();
    uint32_t getAutomaticIntervalMs() const;

    uint32_t nextAutomaticReadMs = 0;
    uint32_t nextIntervalQueryMs = 0;
    uint32_t statusDeadlineMs = 0;
    bool automaticReadInProgress = false;
    bool intervalQueryInProgress = false;
    bool intervalQueryNeeded = true;
    bool statusCallbackInstalled = false;
};

#endif
