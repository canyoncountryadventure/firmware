#pragma once

#include "configuration.h"

#if defined(ARCH_ESP32) && defined(HELTEC_V4) && !MESHTASTIC_EXCLUDE_BLUETOOTH

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class HoboBleSensorModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    HoboBleSensorModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);
};

#endif // ARCH_ESP32 && HELTEC_V4 && Bluetooth
