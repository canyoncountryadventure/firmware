#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class MX2001DiagnosticModule :
    public SinglePortModule,
    private concurrency::OSThread
{
  public:
    MX2001DiagnosticModule();

  protected:
    int32_t runOnce() override;

    bool wantPacket(
        const meshtastic_MeshPacket *p) override;

    ProcessMessage handleReceived(
        const meshtastic_MeshPacket &mp) override;

  private:
    bool sendMeasurementPacket(
        float stageFeet,
        float temperatureF,
        uint16_t temperatureRaw);

    bool sendTextReply(
        uint32_t destination,
        uint8_t channel,
        const char *text);
};

#endif
