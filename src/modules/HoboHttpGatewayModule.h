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

    // Local sensor modules use the same proven HTTP gateway as mesh-received data.
    // Local environmental temperature is deliberately held until the next Hidden Valley
    // environmental packet so both stations can be posted in one HTTPS batch / Neon wake window.
    bool queueLocalEnvironment(float temperatureC, const char *loggerModel, const char *loggerMac,
                               int8_t bleRssi, uint16_t sequence);
    bool queueLocalMX2001(float waterLevelFt, float temperatureF, float temperatureC,
                          uint16_t temperatureRaw, const char *loggerMac, int8_t bleRssi,
                          uint16_t sequence);

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    enum class JobType : uint8_t {
        MX2001 = 0,
        MOISTURE_PIR = 1,
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

        // HOBO / environmental temperature
        uint16_t sequence;
        uint16_t temperatureRaw;
        float waterLevelFt;
        float temperatureF;
        float temperatureC;
        char loggerMac[18];
        char loggerModel[12];
        bool localBleSensor;

        // Legacy RK wire packet: sandstone moisture + PIR
        uint16_t moistureAdc;
        uint16_t moistureSensorMv;
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
    static constexpr uint8_t LOCAL_HOLD_QUEUE_SIZE = 20;
    static constexpr uint8_t SEEN_PACKET_SLOTS = 48;
    static constexpr uint8_t MAX_RETRIES = 4;
    static constexpr uint32_t HIDDEN_VALLEY_NODE_NUM = 1436900584UL; // !55a55ce8

    TypedQueue<UploadJob> uploadQueue;
    TypedQueue<UploadJob> pendingLocalEnvironmentQueue;
    SeenPacket seenPackets[SEEN_PACKET_SLOTS] = {};
    uint8_t seenPacketIndex = 0;
    uint32_t localPacketCounter = 0;

    bool isDuplicate(const meshtastic_MeshPacket &mp);
    bool enqueueMX2001(const meshtastic_MeshPacket &mp);
    bool enqueueMoisturePir(const meshtastic_MeshPacket &mp);
    bool enqueueEnvironment(const meshtastic_MeshPacket &mp);
    bool enqueueDevice(const meshtastic_MeshPacket &mp);
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp);
    void fillLocalCommon(UploadJob &job, uint16_t sequence);
    void fillStationName(char *dest, size_t destSize, uint32_t from);
    bool isHiddenValleyEnvironment(const UploadJob &job) const;
    String serializeJob(const UploadJob &job) const;
    bool postBody(const String &body, uint32_t packetId, uint8_t readingCount);
    bool upload(const UploadJob &job);
    bool uploadHiddenValleyBatch(const UploadJob &hiddenValleyJob);
};

extern HoboHttpGatewayModule *hoboHttpGatewayModule;

#endif // HOBO_HTTP_GATEWAY_ENABLED
#endif // ARCH_ESP32 && HAS_WIFI
