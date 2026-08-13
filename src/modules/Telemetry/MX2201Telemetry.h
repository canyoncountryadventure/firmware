#pragma once

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "ProtobufModule.h"
#include "concurrency/OSThread.h"

class MX2201TelemetryModule : private concurrency::OSThread,
                              public ProtobufModule<meshtastic_Telemetry>
{
  public:
    MX2201TelemetryModule();

  protected:
    virtual int32_t runOnce() override;

    virtual bool handleReceivedProtobuf(
        const meshtastic_MeshPacket &mp,
        meshtastic_Telemetry *decoded) override;

  private:
    bool sendTemperatureTelemetry(float temperatureC);
};

#endif