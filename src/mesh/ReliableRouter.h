#pragma once

#include "NextHopRouter.h"

/**
 * This is a mixin that extends Router with the ability to do (one hop only) reliable message sends.
 */
class ReliableRouter : public NextHopRouter
{
  public:
    /**
     * Constructor
     *
     */
    // ReliableRouter();

    /**
     * Send a packet on a suitable interface.  This routine will
     * later free() the packet to pool.  This routine is not allowed to stall.
     * If the txmit queue is full it might return an error
     */
    virtual ErrorCode send(meshtastic_MeshPacket *p) override;
#if defined(ARCH_NRF52) && defined(RAK_4631)
    // Bounded pending sensor packets; all other Meshtastic traffic is unchanged.
    // Each packet is replayed ONCE as a NEW unrestricted broadcast ID if the
    // preferred first-hop rebroadcast is not overheard within 25 seconds.
    virtual int32_t runOnce() override;
#endif

  protected:
    /**
     * Look for acks/naks or someone retransmitting us
     */
    virtual void sniffReceived(const meshtastic_MeshPacket *p, const meshtastic_Routing *c) override;

    /**
     * We hook this method so we can see packets before FloodingRouter says they should be discarded
     */
    virtual bool shouldFilterReceived(const meshtastic_MeshPacket *p) override;

  private:
#if defined(ARCH_NRF52) && defined(RAK_4631)
    struct RelayFallback {
        meshtastic_MeshPacket *original = nullptr;
        uint8_t expectedRelay = 0;
        uint32_t sentAtMs = 0;
    };
    static constexpr uint32_t RELAY_FALLBACK_MS = 25000UL;
    RelayFallback relayFallbacks[4] = {};
    int32_t processRelayFallbacks();
#endif

    /**
     * Should this packet be ACKed with a want_ack for reliable delivery?
     */
    bool shouldSuccessAckWithWantAck(const meshtastic_MeshPacket *p);
};