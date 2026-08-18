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

    ProcessMessage handleReceived(
        const meshtastic_MeshPacket &mp) override;

  private:
    bool sendMeasurementPacket(
        float stageFeet,
        float temperatureF,
        uint16_t temperatureRaw);
};

#endif
