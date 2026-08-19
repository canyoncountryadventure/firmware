#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class HOBOMX2001MX2201MX2203TelemetryModule :
    public SinglePortModule,
    private concurrency::OSThread
{
  public:
    HOBOMX2001MX2201MX2203TelemetryModule();

  protected:
    int32_t runOnce() override;

    bool wantPacket(
        const meshtastic_MeshPacket *p) override;

    ProcessMessage handleReceived(
        const meshtastic_MeshPacket &mp) override;

  private:
    bool sendTextReply(
        uint32_t destination,
        uint8_t channel,
        const char *text);
};

#endif
