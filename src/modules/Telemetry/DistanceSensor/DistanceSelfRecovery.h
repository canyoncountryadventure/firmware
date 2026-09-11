#pragma once

#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class DistanceSelfRecoveryModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    DistanceSelfRecoveryModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);
};

#endif
