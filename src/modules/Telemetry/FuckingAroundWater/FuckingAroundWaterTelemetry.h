#pragma once

#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.h"

class FuckingAroundWaterTelemetryModule : public HOBOMX2001MX2201MX2203TelemetryModule
{
  public:
    FuckingAroundWaterTelemetryModule();

  protected:
    int32_t runOnce() override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    static constexpr uint8_t WATER_ALERT_HISTORY_SIZE = 5;

    bool waterInitialized = false;
    bool haveWaterReading = false;
    bool haveConfirmedWaterReading = false;
    bool waterFaultActive = false;
    uint32_t nextWaterSampleMs = 0;
    uint32_t lastWaterSampleMs = 0;
    float currentWaterPercent = 0.0f;
    float confirmedWaterPercent = 0.0f;
    float previousWaterPercent = -1.0f;
    uint16_t currentWaterDistanceMm = 0;
    uint16_t confirmedWaterDistanceMm = 0;
    uint8_t currentWaterValidCount = 0;
    uint8_t consecutiveWaterFailures = 0;
    int nextWaterAlertThreshold = 90;
    uint16_t waterAlertDistanceHistory[WATER_ALERT_HISTORY_SIZE] = {};
    uint8_t waterAlertHistoryCount = 0;
    uint8_t waterAlertHistoryIndex = 0;

    void initializeWaterSensor();
    bool sampleWater(bool processAlerts);
    bool collectMedianDistance(uint16_t &medianMm, uint8_t &validCount);
    float distanceToPercent(uint16_t distanceMm) const;
    bool updateWaterAlertFilter(uint16_t distanceMm, uint16_t &filteredDistanceMm, float &filteredPercent);
    void resetWaterAlertLadder(float percent);
    void processWaterAlerts(float percent, uint16_t distanceMm);
    bool sendWaterText(uint32_t destination, uint8_t channel, const char *text, bool ack);
    void sendWaterEvent(const char *type, int threshold = -1, float eventPercent = -1.0f, uint16_t eventDistanceMm = 0);
    void sendWaterStatus(uint32_t destination, uint8_t channel, bool raw);
};

#endif
