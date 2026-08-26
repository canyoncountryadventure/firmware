#pragma once

#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include <cstdint>

class CCARockTelemetryModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    CCARockTelemetryModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendRockPacket();
    bool sendText(uint32_t destination, uint8_t channel, const char *text, bool wantAck);
};

extern CCARockTelemetryModule *ccaRockTelemetryModule;

#endif
