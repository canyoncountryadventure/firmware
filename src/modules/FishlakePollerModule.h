#pragma once

#include "configuration.h"

#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "mesh/TypedQueue.h"

#include <cstdint>

class FishlakePollerModule : public MeshModule, private concurrency::OSThread
{
  public:
    FishlakePollerModule();

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    struct Reading {
        uint8_t retries;
        uint8_t channel;
        uint8_t hopStart;
        uint8_t hopLimit;
        uint8_t relayNode;
        int8_t bleRssi;
        int16_t rssi;
        float snr;
        uint32_t packetId;
        uint32_t timestamp;
        float temperatureC;
        char loggerModel[12];
        char loggerMac[18];
    };

    static constexpr uint32_t FISHLAKE_NODE_NUM = 1577197109UL; // !5e021e35
    static constexpr uint32_t POLL_INTERVAL_MS = 60UL * 60UL * 1000UL;
    static constexpr uint32_t FIRST_POLL_DELAY_MS = 60UL * 1000UL;
    static constexpr uint8_t MAX_RETRIES = 4;
    static constexpr uint8_t QUEUE_SIZE = 8;

    TypedQueue<Reading> uploadQueue;
    uint32_t nextPollMs = 0;

    bool sendReadCommand();
    bool parseReply(const meshtastic_MeshPacket &mp, Reading &reading);
    bool upload(const Reading &reading);
};

extern FishlakePollerModule *fishlakePollerModule;

#endif
