#pragma once

#include "configuration.h"

#if defined(ARCH_ESP32) && HAS_WIFI

#if __has_include("hobo_gateway_secrets.h")
#include "hobo_gateway_secrets.h"
#endif

#ifndef HOBO_HTTP_GATEWAY_ENABLED
#define HOBO_HTTP_GATEWAY_ENABLED 0
#endif

#if HOBO_HTTP_GATEWAY_ENABLED

#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "mesh/TypedQueue.h"

#include <cstdint>

class HoboHttpGatewayModule : public MeshModule, private concurrency::OSThread
{
  public:
    HoboHttpGatewayModule();

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    enum class JobType : uint8_t {
        MX2001 = 1,
        ENVIRONMENT = 2,
    };

    struct UploadJob {
        JobType type;
        uint8_t retries;
        uint8_t channel;
        uint8_t hopStart;
        uint8_t hopLimit;
        uint8_t relayNode;
        int8_t bleRssi;
        int16_t rssi;
        float snr;
        uint32_t packetId;
        uint32_t from;
        uint32_t timestamp;
        uint16_t sequence;
        uint16_t temperatureRaw;
        float waterLevelFt;
        float temperatureF;
        float temperatureC;
        char loggerMac[18];
        char stationName[40];
    };

    struct SeenPacket {
        uint32_t from;
        uint32_t id;
    };

    static constexpr uint8_t UPLOAD_QUEUE_SIZE = 8;
    static constexpr uint8_t SEEN_PACKET_SLOTS = 32;
    static constexpr uint8_t MAX_RETRIES = 4;

    TypedQueue<UploadJob> uploadQueue;
    SeenPacket seenPackets[SEEN_PACKET_SLOTS] = {};
    uint8_t seenPacketIndex = 0;

    bool isDuplicate(const meshtastic_MeshPacket &mp);
    bool enqueueMX2001(const meshtastic_MeshPacket &mp);
    bool enqueueEnvironment(const meshtastic_MeshPacket &mp);
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp);
    void fillStationName(char *dest, size_t destSize, uint32_t from);
    bool upload(const UploadJob &job);
};

#endif // HOBO_HTTP_GATEWAY_ENABLED
#endif // ARCH_ESP32 && HAS_WIFI
