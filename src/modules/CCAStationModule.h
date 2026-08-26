#pragma once

#include "configuration.h"

#if defined(CCA_MX_PIR) && defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/RadioLibInterface.h"
#include <Arduino.h>
#include <cstdint>

// CCA PIR RF hardening for the XIAO + Wio-SX1262 station.
//
// The SEN0171/D6 input can be driven HIGH by RF from this node's own LoRa TX.
// RadioLib exposes whether a packet is actively transmitting. Because the CCA
// PIR thread samples D6 every 100 ms and our LongFast TX airtime is hundreds of
// milliseconds, we can reliably observe each local TX without changing the
// Meshtastic radio core.
//
// If a NEW raw D6 rising edge begins while TX is active, or during the short
// post-TX settling window, suppress that entire HIGH pulse until D6 physically
// returns LOW. This is important: the SEN0171 can hold its output HIGH for many
// seconds, so a simple debounce would merely count the same RF-induced pulse
// after the debounce expired.
//
// A real PIR HIGH that began before TX remains valid; transmitting while a
// legitimate motion pulse is already active does not erase that detection.
constexpr uint32_t CCA_PIR_POST_TX_GUARD_MS = 1500;

inline void ccaPirPinMode(uint32_t pin, uint32_t mode)
{
    // Give D6 a stronger defined LOW than the previous plain INPUT. The PIR's
    // active output can still drive the nRF52840 input HIGH normally.
    if (pin == D6 && mode == INPUT)
        ::pinMode(pin, INPUT_PULLDOWN);
    else
        ::pinMode(pin, mode);
}

inline int ccaPirDigitalRead(uint32_t pin)
{
    const int raw = ::digitalRead(pin);
    if (pin != D6)
        return raw;

    static uint32_t lastTxObservedMs = 0;
    static bool rawWasHigh = false;
    static bool suppressThisHighPulse = false;

    const uint32_t now = millis();
    const bool radioSending = RadioLibInterface::instance != nullptr && RadioLibInterface::instance->isSending();

    if (radioSending)
        lastTxObservedMs = now;

    if (!raw) {
        rawWasHigh = false;
        suppressThisHighPulse = false;
        return 0;
    }

    // Only classify the start of a HIGH pulse. Once an RF-correlated pulse is
    // rejected, keep rejecting it until the sensor returns physically LOW.
    if (!rawWasHigh) {
        const bool withinPostTxGuard =
            lastTxObservedMs != 0 && static_cast<uint32_t>(now - lastTxObservedMs) <= CCA_PIR_POST_TX_GUARD_MS;
        if (radioSending || withinPostTxGuard)
            suppressThisHighPulse = true;
    }

    rawWasHigh = true;
    return suppressThisHighPulse ? 0 : 1;
}

class CCAStationModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    CCAStationModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    bool sendText(uint32_t destination, uint8_t channel, const char *text, bool wantAck);
    bool sendAutomaticAlert(const char *text);
};

extern CCAStationModule *ccaStationModule;

// Arduino defines LOW as a preprocessor macro. The CCA implementation uses
// LOW as the scoped name of a power-alert state, so remove the macro for this
// translation unit after all framework types needed by this header are parsed.
#ifdef LOW
#undef LOW
#endif

// Keep the existing CCAStationModule.cpp call sites intact while routing D6
// through the RF-aware filter. These macros are intentionally local to CCA
// translation units that include this header.
#define pinMode(pin, mode) ccaPirPinMode((pin), (mode))
#define digitalRead(pin) ccaPirDigitalRead((pin))

#endif
