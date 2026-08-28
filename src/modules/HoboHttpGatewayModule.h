#pragma once

#include "configuration.h"

#if defined(ARCH_ESP32) && HAS_WIFI

#if __has_include("hobo_gateway_secrets.h")
#include "hobo_gateway_secrets.h"
#endif

#ifndef HOBO_HTTP_GATEWAY_ENABLED
#if defined(HELTEC_V4)
#define HOBO_HTTP_GATEWAY_ENABLED 1
#else
#define HOBO_HTTP_GATEWAY_ENABLED 0
#endif
#endif

#ifndef HOBO_HTTP_GATEWAY_URL
#define HOBO_HTTP_GATEWAY_URL "https://meshtastic-ecru.vercel.app/api/ingest"
#endif

#ifndef HOBO_HTTP_GATEWAY_INGEST_KEY
#define HOBO_HTTP_GATEWAY_INGEST_KEY ""
#endif

#ifndef HOBO_HTTP_GATEWAY_NAME
#define HOBO_HTTP_GATEWAY_NAME "Heltec Hub"
#endif

#ifndef HOBO_HTTP_GATEWAY_FAVORITES_ONLY
#define HOBO_HTTP_GATEWAY_FAVORITES_ONLY 0
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
        MX2001 = 0,
        ROCK_TEST = 1,
        ENVIRONMENT = 2,
        DEVICE = 3,
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

        // MX2001 / environmental temperature
        uint16_t sequence;
        uint16_t temperatureRaw;
        float waterLevelFt;
        float temperatureF;
        float temperatureC;
        char loggerMac[18];

        // CCA sandstone test
        uint16_t rockAdc;
        uint16_t rockSensorMv;
        uint32_t motionCount;
        uint16_t batteryMv;
        uint8_t batteryPercent;
        bool motionDetected;

        // Native Meshtastic device telemetry
        bool hasDeviceBatteryLevel;
        bool hasDeviceVoltage;
        bool hasChannelUtilization;
        bool hasAirUtilTx;
        bool hasUptimeSeconds;
        uint32_t deviceBatteryLevel;
        float deviceVoltage;
        float channelUtilization;
        float airUtilTx;
        uint32_t uptimeSeconds;

        char stationName[40];
    };

    struct SeenPacket {
        uint32_t from;
        uint32_t id;
    };

    static constexpr uint8_t UPLOAD_QUEUE_SIZE = 24;
    static constexpr uint8_t SEEN_PACKET_SLOTS = 48;
    static constexpr uint8_t MAX_RETRIES = 4;

    TypedQueue<UploadJob> uploadQueue;
    SeenPacket seenPackets[SEEN_PACKET_SLOTS] = {};
    uint8_t seenPacketIndex = 0;

    bool isDuplicate(const meshtastic_MeshPacket &mp);
    bool enqueueMX2001(const meshtastic_MeshPacket &mp);
    bool enqueueRockTest(const meshtastic_MeshPacket &mp);
    bool enqueueEnvironment(const meshtastic_MeshPacket &mp);
    bool enqueueDevice(const meshtastic_MeshPacket &mp);
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp);
    void fillStationName(char *dest, size_t destSize, uint32_t from);
    bool upload(const UploadJob &job);
};

#endif // HOBO_HTTP_GATEWAY_ENABLED
#endif // ARCH_ESP32 && HAS_WIFI
