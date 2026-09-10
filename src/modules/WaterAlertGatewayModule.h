#pragma once

#include "configuration.h"

#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#if __has_include("hobo_gateway_secrets.h")
#include "hobo_gateway_secrets.h"
#endif

#ifndef WATER_GITHUB_TOKEN
#define WATER_GITHUB_TOKEN ""
#endif

#ifndef WATER_GITHUB_REPOSITORY
#define WATER_GITHUB_REPOSITORY "canyoncountryadventure/firmware"
#endif

#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "mesh/TypedQueue.h"

class WaterAlertGatewayModule : public MeshModule, private concurrency::OSThread
{
  public:
    WaterAlertGatewayModule();

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    struct AlertJob {
        uint32_t packetId;
        uint32_t from;
        uint8_t channel;
        uint8_t hopStart;
        uint8_t hopLimit;
        int16_t rssi;
        float snr;
        char stationName[40];
        char text[190];
        uint8_t retries;
    };

    static constexpr uint8_t QUEUE_SIZE = 12;
    static constexpr uint8_t MAX_RETRIES = 4;
    TypedQueue<AlertJob> queue;

    void fillStationName(char *dest, size_t size, uint32_t from);
    bool uploadAlert(const AlertJob &job);
};

#endif
