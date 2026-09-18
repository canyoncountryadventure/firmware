#pragma once
#include "configuration.h"
#if defined(ARCH_NRF52) && defined(RAK_4631)
#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
class RAKRemoteDfuModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    RAKRemoteDfuModule();
  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
  private:
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);
};
#endif
