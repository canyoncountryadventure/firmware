#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class SEN0308SoilMoistureModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    SEN0308SoilMoistureModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    struct Reading {
        uint16_t adc10;
        uint8_t moisturePercent;
    };

    Reading sample();
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);
    bool sendTelemetry(const Reading &reading);
    bool sendRawPacket(const Reading &reading);
};

#endif
