#pragma once

#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class CCAStationModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    CCAStationModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendText(uint32_t destination, uint8_t channel, const char *text, bool wantAck);
    bool sendBroadcast(const char *text);
};

extern CCAStationModule *ccaStationModule;

// Arduino defines LOW as a preprocessor macro. The CCA implementation uses
// LOW as the scoped name of a power-alert state, so remove the macro for this
// translation unit after all framework types needed by this header are parsed.
#ifdef LOW
#undef LOW
#endif

#endif
