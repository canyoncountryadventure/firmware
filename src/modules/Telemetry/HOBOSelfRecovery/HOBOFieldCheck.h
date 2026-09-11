#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && (defined(SEEED_XIAO_NRF52840_KIT) || defined(RAK_4631))

#include "mesh/SinglePortModule.h"

class HOBOFieldCheckModule : public SinglePortModule
{
  public:
    HOBOFieldCheckModule();

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);
};

#endif
