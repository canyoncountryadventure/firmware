#pragma once

#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "DistanceSensorDrivers.h"
#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"

class DistanceSensorModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    DistanceSensorModule();

  protected:
    int32_t runOnce() override;
    bool wantPacket(const meshtastic_MeshPacket *p) override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    enum class Mode : uint8_t
    {
        IDLE = 0,
        WATER = 1,
        TRAIL = 2
    };

    struct PersistentConfig
    {
        uint32_t magic;
        uint8_t version;
        uint8_t sensorType;
        uint8_t mode;
        uint8_t flags;
        int32_t stageReferenceMm;
        uint32_t trailBaselineMm;
        uint16_t triggerDeltaMm;
        uint16_t clearDeltaMm;
        uint32_t reportIntervalSec;
        uint32_t dailyCount;
        uint32_t lifetimeCount;
        uint32_t dayKey;
        uint8_t checksum;
    } __attribute__((packed));

    static constexpr uint8_t FLAG_STAGE_CALIBRATED = 0x01;
    static constexpr uint8_t FLAG_TRAIL_CALIBRATED = 0x02;

    SEN0590DistanceDriver sen0590;
    DFRobotUARTDistanceDriver uartDriver;
    DistanceSensorDriver *activeDriver = nullptr;
    DistanceSensorType activeType = DistanceSensorType::NONE;
    PersistentConfig cfg = {};

    DistanceReading latestReading = {};
    int32_t latestStageMm = 0;
    bool latestStageValid = false;
    bool obstruction = false;
    bool trailBlocked = false;
    uint8_t blockedSamples = 0;
    uint8_t clearSamples = 0;
    uint32_t blockedSinceMs = 0;
    uint32_t lastEventMinDistanceMm = 0;
    uint32_t lastReportMs = 0;
    uint32_t lastWaterReadMs = 0;
    uint32_t sensorReadErrors = 0;
    uint32_t sensorReadSuccesses = 0;
    uint16_t telemetrySequence = 0;
    uint8_t unsavedCountEvents = 0;

    void setDefaults();
    bool loadConfig();
    bool saveConfig();
    uint8_t configChecksum(const PersistentConfig &record) const;
    bool configValid(const PersistentConfig &record) const;

    bool configureSensor(DistanceSensorType requested, bool persistSelection);
    bool autoDetectSensor();
    DistanceReading readDistance();
    void updateDailyRollover();
    void updateTrailState(const DistanceReading &reading);
    bool sendDistanceTelemetry(bool eventFlag = false);
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);

    bool calibrateTrail(uint32_t requester, uint8_t channel);
    bool calibrateStage(int32_t knownStageMm, uint32_t requester, uint8_t channel);
    void replyStatus(uint32_t requester, uint8_t channel);
    void replyRead(uint32_t requester, uint8_t channel, bool rawOnly);
    void replySensor(uint32_t requester, uint8_t channel);
    void replyCount(uint32_t requester, uint8_t channel);

    static const char *modeName(Mode mode);
    static bool normalizeCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize);
    static bool parseDistanceMm(const char *text, int32_t &valueMm);
    static bool parseIntervalSeconds(const char *text, uint32_t &seconds);
};

#endif
