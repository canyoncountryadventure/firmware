#pragma once

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"

class MX2201TelemetryModule : private concurrency::OSThread
{
  public:
    MX2201TelemetryModule();

  protected:
    virtual int32_t runOnce() override;
};

#endif