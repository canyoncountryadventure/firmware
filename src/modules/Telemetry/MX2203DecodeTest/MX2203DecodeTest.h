#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"

class MX2203DecodeTestModule : private concurrency::OSThread
{
  public:
    MX2203DecodeTestModule();

  protected:
    int32_t runOnce() override;
};

#endif
