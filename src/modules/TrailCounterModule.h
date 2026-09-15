#pragma once

#include "SinglePortModule.h"

class TrailCounterModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    TrailCounterModule()
        : SinglePortModule("trailcounter", meshtastic_PortNum_DETECTION_SENSOR_APP), OSThread("TrailCounter")
    {
    }

  protected:
    int32_t runOnce() override;

  private:
    bool initialized = false;
    bool armed = false;
    bool lastState = false;
    uint32_t personCount = 0;
    uint32_t highStarted = 0;
    uint32_t lowStarted = 0;

    void sendPersonMessage(uint32_t lowGapMs);
};

extern TrailCounterModule *trailCounterModule;
