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
#include <cstdio>

class HoboHttpGatewayModule : public MeshModule, private concurrency::OSThread
{
  public:
    HoboHttpGatewayModule();

    // The Home HOBO remains the preferred synchronized cloud batch clock.
    // Permanent remote readings also safety-flush after five minutes so a
    // Home logger/BLE problem can never strand live radio telemetry for an hour.
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

        uint16_t sequence;
        uint16_t temperatureRaw;
        float waterLevelFt;
        float temperatureF;
        float temperatureC;
        char loggerMac[18];
        char loggerModel[12];
        bool localBleSensor;

        uint16_t moistureAdc;
        uint16_t moistureSensorMv;
        uint32_t motionCount;
        uint16_t batteryMv;
        uint8_t batteryPercent;
        bool motionDetected;

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
    static constexpr uint8_t LOCAL_HOLD_QUEUE_SIZE = 48;
    static constexpr uint8_t SEEN_PACKET_SLOTS = 48;
    // Eight application-level HTTP retries gives transient Vercel/network
    // failures substantially more recovery time without changing mesh behavior.
    static constexpr uint8_t MAX_RETRIES = 8;
    // Home is still the preferred synchronized batch trigger, but permanent
    // remote telemetry must reach cloud even if Home BLE/pointer tracking fails.
    static constexpr uint32_t FALLBACK_FLUSH_MS = 5UL * 60UL * 1000UL;

    // Production permanent-node identities. !55a55ce8 is NOT Hidden Valley.
    static constexpr uint32_t HIDDEN_VALLEY_NODE_NUM = 3044869407UL; // !b57d051f
    static constexpr uint32_t FISHLAKE_NODE_NUM = 1577197109UL;      // !5e021e35
    static constexpr uint32_t SWELL_NODE_NUM = 1949224949UL;         // !742ecff5

    bool enqueueHeld(UploadJob job, TickType_t maxWait);

    // Remote environmental/device telemetry is held briefly for the Home HOBO
    // clock, with the five-minute independent fallback above. Unrelated public
    // Meshtastic telemetry is discarded before HTTP work.
    class SynchronizedUploadQueue : public TypedQueue<UploadJob>
    {
      public:
        explicit SynchronizedUploadQueue(int maxElements) : TypedQueue<UploadJob>(maxElements) {}

        void setReader(concurrency::OSThread *reader)
        {
            owner = static_cast<HoboHttpGatewayModule *>(reader);
            TypedQueue<UploadJob>::setReader(reader);
        }

        bool enqueue(UploadJob job, TickType_t maxWait)
        {
            const bool environmentOrDevice =
                job.type == JobType::ENVIRONMENT || job.type == JobType::DEVICE;
            const bool permanentRemote =
                job.from == HIDDEN_VALLEY_NODE_NUM ||
                job.from == FISHLAKE_NODE_NUM ||
                job.from == SWELL_NODE_NUM;

            // Local Home BLE environmental readings are allowed through this queue
            // and become the normal synchronized flush trigger.
            if (job.localBleSensor && job.type == JobType::ENVIRONMENT)
                return TypedQueue<UploadJob>::enqueue(job, maxWait);

            // Public/unconfigured environmental and device telemetry never wakes cloud.
            if (environmentOrDevice && !permanentRemote)
                return true;

            // Every configured remote environment/device reading waits briefly for
            // the Home trigger, then the independent fallback flushes it to cloud.
            if (owner != nullptr && environmentOrDevice && permanentRemote)
                return owner->enqueueHeld(job, maxWait);

            return TypedQueue<UploadJob>::enqueue(job, maxWait);
        }

      private:
        HoboHttpGatewayModule *owner = nullptr;
    };

    SynchronizedUploadQueue uploadQueue;
    TypedQueue<UploadJob> pendingLocalEnvironmentQueue;
    SeenPacket seenPackets[SEEN_PACKET_SLOTS] = {};
    uint8_t seenPacketIndex = 0;
    uint32_t localPacketCounter = 0;
    uint32_t heldQueueStartedMs = 0;
    uint32_t heldRetryDueMs = 0;
    uint8_t heldBatchRetries = 0;

  public:
    // Fishlake's timed DM READ result joins the same remote hold queue.
    bool queueTimedRemoteEnvironment(uint32_t from, uint32_t timestamp, uint32_t packetId,
                                     float temperatureC, int16_t rssi, float snr,
                                     uint8_t hopStart, uint8_t hopLimit, uint8_t relayNode, uint8_t channel,
                                     const char *stationName)
    {
        UploadJob job = {};
        job.type = JobType::ENVIRONMENT;
        job.retries = 0;
        job.from = from;
        job.timestamp = timestamp;
        job.packetId = packetId;
        job.temperatureC = temperatureC;
        job.rssi = rssi;
        job.snr = snr;
        job.hopStart = hopStart;
        job.hopLimit = hopLimit;
        job.relayNode = relayNode;
        job.channel = channel;
        job.localBleSensor = false;
        snprintf(job.stationName, sizeof(job.stationName), "%s", stationName ? stationName : "Remote station");
        return enqueueHeld(job, 0);
    }

  private:
    bool isDuplicate(const meshtastic_MeshPacket &mp);
    bool enqueueMX2001(const meshtastic_MeshPacket &mp);
    bool enqueueMoisturePir(const meshtastic_MeshPacket &mp);
    bool enqueueEnvironment(const meshtastic_MeshPacket &mp);
    bool enqueueDevice(const meshtastic_MeshPacket &mp);
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp);
    void fillLocalCommon(UploadJob &job, uint16_t sequence);
    void fillStationName(char *dest, size_t destSize, uint32_t from);
    bool isHomeEnvironmentTrigger(const UploadJob &job) const;
    String serializeJob(const UploadJob &job) const;
    bool postBody(const String &body, uint32_t packetId, uint8_t readingCount);
    bool upload(const UploadJob &job);
    bool uploadHomeBatch(const UploadJob &homeJob);
    bool uploadHeldBatch();
};

extern HoboHttpGatewayModule *hoboHttpGatewayModule;

#endif // HOBO_HTTP_GATEWAY_ENABLED
#endif // ARCH_ESP32 && HAS_WIFI
