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
    String statusJson();
    uint32_t healthCycles() const { return healthLoopCycles; }
    uint32_t healthHttpAttempts() const { return healthPostAttempts; }
    uint32_t healthHttpAccepted() const { return uploadedJobs; }
    uint32_t healthDmRequests() const { return cc01Requests + mathesonRequests; }
    uint32_t healthQueueUsed() { return uploadQueue.numUsed(); }
    int healthLastHttpStatus() const { return lastHttpStatus; }

    // Local BLE HOBO readers call these. Every reading is queued immediately.
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
        SOIL = 4,
        WATER_DISTANCE = 5,
        SENSOR_STATUS = 6,
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
        bool dmPollResult; // CC01 DM reply, not a PRIVATE_APP telemetry packet

        uint16_t moistureAdc;
        uint16_t moistureSensorMv;
        uint32_t motionCount;
        uint16_t batteryMv;
        uint8_t batteryPercent;
        bool motionDetected;
        uint8_t soilMoisturePercent;
        uint32_t distanceMm;
        int32_t stageMm;
        bool distanceValid;
        bool stageCalibrated;
        uint8_t distanceSensorType;
        uint32_t sensorTimestamp;

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
        char sensorStatusType[8];
        char sensorStatusReason[20];
    };

    struct SeenPacket {
        uint32_t from;
        uint32_t id;
    };

    static constexpr uint8_t UPLOAD_QUEUE_SIZE = 32;
    static constexpr uint8_t SEEN_PACKET_SLOTS = 64;
    static constexpr uint8_t MAX_RETRIES = 6;

    TypedQueue<UploadJob> uploadQueue;
    SeenPacket seenPackets[SEEN_PACKET_SLOTS] = {};
    uint8_t seenPacketIndex = 0;
    uint32_t localPacketCounter = 0;

    // Ordered CC01 sensor cycle: water first, HOBO second, each at most 3 attempts.
    // All times are monotonic millis; a sensor fault never schedules a reboot.
    enum class Cc01Phase : uint8_t { Idle, Water, Hobo, Done };
    enum class MathesonPhase : uint8_t { Idle, Hobo, Done };
    Cc01Phase cc01Phase = Cc01Phase::Idle;
    bool cc01PollStarted = false;
    bool cc01WaterPending = false;
    bool cc01HoboPending = false;
    bool cc01HasDmSent = false;
    bool cc01WaterEnabled = true;
    bool cc01HoboEnabled = true;
    bool cc01FastMode = false;
    bool cc01HaveStage = false;
    float cc01LastStageFt = 0.0f;
    uint32_t cc01LastStageMs = 0;
    uint32_t cc01AdminNode = 0;
    uint32_t cc01ConfigVersion = 0;
    uint32_t cc01LastPollMs = 0;
    uint32_t cc01WaterRequestMs = 0;
    uint32_t cc01HoboRequestMs = 0;
    uint32_t cc01LastDmSendMs = 0;
    uint32_t cc01NextConfigMs = 0;
    uint16_t cc01IntervalOverrideMinutes = 0;
    uint8_t cc01WaterAttempts = 0;
    uint8_t cc01HoboAttempts = 0;
    static constexpr uint32_t CC01_NORMAL_INTERVAL_MS = 60UL * 60UL * 1000UL;
    static constexpr uint32_t CC01_FAST_INTERVAL_MS = 15UL * 60UL * 1000UL;
    static constexpr uint32_t CC01_REPLY_TIMEOUT_MS = 30UL * 1000UL;
    static constexpr uint32_t CC01_DM_SPACING_MS = 3000UL;
    static constexpr uint8_t CC01_ATTEMPT_LIMIT = 3;
    static constexpr uint8_t CC01_LONGFAST_CHANNEL = 1;

    MathesonPhase mathesonPhase = MathesonPhase::Idle;
    bool mathesonPollStarted = false;
    bool mathesonHoboPending = false;
    uint16_t mathesonIntervalMinutes = 60;
    uint32_t mathesonLastPollMs = 0;
    uint32_t mathesonHoboRequestMs = 0;
    uint8_t mathesonHoboAttempts = 0;
    static constexpr uint32_t MATHESON_REPLY_TIMEOUT_MS = 30UL * 1000UL;
    static constexpr uint8_t MATHESON_ATTEMPT_LIMIT = 3;

    uint32_t cc01WaterRetries = 0;
    uint32_t cc01HoboRetries = 0;
    uint32_t cc01Requests = 0;
    uint32_t cc01Replies = 0;
    uint32_t cc01Parsed = 0;
    uint32_t cc01Queued = 0;
    uint32_t cc01Rejected = 0;
    uint32_t cc01Filtered = 0;
    uint32_t cc01Duplicates = 0;
    uint32_t cc01Dropped = 0;
    uint32_t cc01QueueFull = 0;
    uint32_t cc01LastReplyMs = 0;
    uint32_t cc01LastReplyPacket = 0;
    uint32_t cc01Uploaded = 0;
    uint32_t cc01UploadAttempts = 0;
    int cc01LastHttpStatus = 0;
    uint32_t cc01LastUploadPacket = 0;
    uint32_t cc01LastUploadMs = 0;
    uint32_t mathesonRequests = 0;
    uint32_t mathesonReplies = 0;
    uint32_t mathesonParsed = 0;
    uint32_t mathesonQueued = 0;
    uint32_t mathesonRejected = 0;
    uint32_t mathesonLastReplyMs = 0;
    int lastHttpStatus = 0;
    uint32_t uploadedJobs = 0;
    uint32_t healthLoopCycles = 0;
    uint32_t healthPostAttempts = 0;

    void pollCc01();
    void pollMatheson();
    bool sendCc01Dm(const char *command);
    void handleCc01Reply(const meshtastic_MeshPacket &mp);
    void handleMathesonReply(const meshtastic_MeshPacket &mp);
    void handleCc01ControlDm(const meshtastic_MeshPacket &mp);
    void refreshCc01CloudControl();
    void persistCc01Settings();
    void queueCc01SensorStatus(const meshtastic_MeshPacket &mp, const char *sensor, const char *reason);
    bool sendCc01Text(uint32_t to, const char *command);

    bool isDuplicate(const meshtastic_MeshPacket &mp);
    bool enqueueMX2001(const meshtastic_MeshPacket &mp);
    bool enqueueMoisturePir(const meshtastic_MeshPacket &mp);
    bool enqueueSoil(const meshtastic_MeshPacket &mp);
    bool enqueueWaterDistance(const meshtastic_MeshPacket &mp);
    bool enqueueEnvironment(const meshtastic_MeshPacket &mp);
    bool enqueueDevice(const meshtastic_MeshPacket &mp);
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp);
    void fillLocalCommon(UploadJob &job, uint16_t sequence);
    void fillStationName(char *dest, size_t destSize, uint32_t from);
    String serializeJob(const UploadJob &job) const;
    bool upload(const UploadJob &job);
};

extern HoboHttpGatewayModule *hoboHttpGatewayModule;

#endif // HOBO_HTTP_GATEWAY_ENABLED
#endif // ARCH_ESP32 && HAS_WIFI
