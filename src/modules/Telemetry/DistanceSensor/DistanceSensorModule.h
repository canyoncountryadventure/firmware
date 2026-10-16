#pragma once

#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "DistanceSensorDrivers.h"
#include "concurrency/OSThread.h"
#include "concurrency/Lock.h"
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
    struct PersistentConfig
    {
        uint32_t magic;
        uint8_t version;
        uint8_t sensorType;
        uint8_t flags;
        uint8_t reserved0;
        int32_t stageReferenceMm;
        uint32_t calibrationRawMm;
        int32_t calibrationStageMm;
        uint32_t calibrationUnix;
        uint32_t reportIntervalSec;
        uint32_t sequence;
        uint32_t crc32;
    } __attribute__((packed));

    static constexpr uint8_t FLAG_CALIBRATED = 0x01;
    static constexpr uint8_t FLAG_CAL_LOCKED = 0x02;

    SEN0590DistanceDriver sen0590;
    DFRobotUARTDistanceDriver uartDriver;
    MaxBotixMB7388DistanceDriver mb7388Driver;
    DistanceSensorDriver *activeDriver = nullptr;
    DistanceSensorType activeType = DistanceSensorType::NONE;
    PersistentConfig cfg = {};

    // Water sampling runs in this module's OSThread, never in the mesh command handler.
    // The Pololu's eight-second settling period must not block HOBO BLE work.
    enum class SampleJob : uint8_t { NONE, SCHEDULED, STATUS, READ, RAW, VERIFY, CALIBRATE, TELEMETRY_NOW, AUTO_PROBE };
    concurrency::Lock sampleLock;
    SampleJob sampleJob = SampleJob::NONE;
    uint32_t samplePowerOnMs = 0;
    uint32_t nextSampleMs = 0;
    uint32_t sampleRequester = 0;
    uint8_t sampleChannel = 0;
    int32_t sampleStageMm = 0;
    uint8_t sampleCount = 0;
    uint8_t sampleAttempts = 0;
    static constexpr uint8_t SAMPLE_COUNT = 10;
    static constexpr uint8_t MAX_SAMPLE_ATTEMPTS = 20;
    uint32_t sampleValues[SAMPLE_COUNT] = {};
    uint8_t latestSampleCount = 0;
    uint32_t latestSampleSpreadMm = 0;
    bool reinitializeSensorPending = false;
    bool moduleInitialized = false;
    int8_t activeConfigSlot = -1;
    DistanceReading latestReading = {};
    int32_t latestStageMm = 0;
    bool latestStageValid = false;
    uint32_t lastReportMs = 0;
    uint32_t lastSensorAttemptMs = 0;
    uint32_t sensorReadErrors = 0;
    uint32_t sensorReadSuccesses = 0;
    uint32_t consecutiveReadErrors = 0;
    uint16_t telemetrySequence = 0;

    void setDefaults();
    bool loadConfig();
    bool saveConfig();
    bool readConfigSlot(const char *path, PersistentConfig &out) const;
    bool migrateLegacyConfig();
    uint32_t configCrc32(const PersistentConfig &record) const;
    bool configValid(const PersistentConfig &record) const;

    bool configureSensor(DistanceSensorType requested, bool persistSelection);
    bool autoDetectSensor();
    DistanceReading readDistance();
    bool scheduleSample(SampleJob job, uint32_t requester = 0, uint8_t channel = 0, int32_t knownStageMm = 0);
    bool sampleBusy();
    void cancelSample();
    void serviceSample(uint32_t now);
    bool finishCalibration(int32_t knownStageMm, uint32_t requester, uint8_t channel,
                           uint32_t median, uint32_t minMm, uint32_t maxMm);
    bool sendDistanceTelemetry();
    bool sendTextReply(uint32_t destination, uint8_t channel, const char *text);

    bool calibrateStage(int32_t knownStageMm, uint32_t requester, uint8_t channel);
    void replyStatus(uint32_t requester, uint8_t channel, bool refreshReading);
    void replyRead(uint32_t requester, uint8_t channel, bool rawOnly);
    void replySensor(uint32_t requester, uint8_t channel);
    void replyCalibration(uint32_t requester, uint8_t channel);
    void replyVerify(uint32_t requester, uint8_t channel, uint32_t median, uint32_t minMm, uint32_t maxMm);

    static bool normalizeCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize);
    static bool parseDistanceMm(const char *text, int32_t &valueMm);
    static bool parseIntervalSeconds(const char *text, uint32_t &seconds);
};

#endif
