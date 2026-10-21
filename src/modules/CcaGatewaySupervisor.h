#pragma once
// Heltec V7 progress supervisor. Deliberately independent of Arduino/FreeRTOS,
// so its decisions can be tested on a host before flashing field hardware.
#include <cstdint>

struct CcaGatewaySnapshot {
    uint32_t rxGood = 0;              // Actual radio RX decodes
    uint32_t txDoneIrq = 0;           // Real TX_DONE IRQ, not txGood
    uint32_t gatewayCycles = 0;       // RunOnce() entered
    uint32_t httpAttempts = 0;        // POST was attempted
    uint32_t httpAccepted = 0;        // HTTP 2xx only
    uint32_t dmRequests = 0;          // Sent to Meshtastic queue, not RF success
    uint32_t queueUsed = 0;
    int lastHttpStatus = 0;
    bool wifiConnected = false;
};

enum class CcaGatewayRecovery : uint8_t {
    None = 0,
    ModuleStalled = 1,
    NoRxOrTx = 2,
    SoftRecoverRx = 3,
    RxSilentThreeHours = 4,
    UploadStalled = 5,
    WifiStalled = 6,
};

class CcaGatewaySupervisor {
  public:
    static constexpr uint32_t BOOT_GRACE_MS = 5UL * 60UL * 1000UL;
    static constexpr uint32_t MODULE_STALL_MS = 2UL * 60UL * 1000UL;
    static constexpr uint32_t QUEUE_STALL_MS = 10UL * 60UL * 1000UL;
    static constexpr uint32_t NO_RF_MS = 60UL * 60UL * 1000UL;
    static constexpr uint32_t RX_SOFT_MS = 90UL * 60UL * 1000UL;
    static constexpr uint32_t RX_HARD_MS = 3UL * 60UL * 60UL * 1000UL;
    static constexpr uint32_t HTTP_FAILURE_MS = 60UL * 60UL * 1000UL;
    static constexpr uint32_t WIFI_FAILURE_MS = 60UL * 60UL * 1000UL;

    CcaGatewayRecovery update(uint32_t now, const CcaGatewaySnapshot &s)
    {
        if (!initialized) {
            initialized = true;
            firstMs = lastRxMs = lastRfMs = lastModuleMs = lastHttpAttemptMs = now;
            wifiDownSinceMs = now;
            previous = s;
            return CcaGatewayRecovery::None;
        }
        if (s.rxGood != previous.rxGood) {
            lastRxMs = lastRfMs = now;
            rxObserved = true;
            softRecovered = false;
        }
        if (s.txDoneIrq != previous.txDoneIrq)
            lastRfMs = now;
        if (s.gatewayCycles != previous.gatewayCycles)
            lastModuleMs = now;
        if (s.httpAccepted != previous.httpAccepted) {
            hadAcceptedUpload = true;
            failedHttpActive = false;
            lastAcceptedMs = now;
        }
        if (s.httpAttempts != previous.httpAttempts) {
            lastHttpAttemptMs = now;
            if (s.httpAccepted == previous.httpAccepted && s.lastHttpStatus < 0) {
                if (!failedHttpActive) {
                    failedHttpActive = true;
                    failedHttpSinceMs = now;
                }
            } else if (s.lastHttpStatus >= 0) {
                // 401/400/500 or an accepted 2xx is not an ESP32 transport freeze.
                failedHttpActive = false;
            }
        }
        if (s.queueUsed && !previous.queueUsed)
            queueOccupiedSinceMs = now;
        if (!s.queueUsed)
            queueOccupiedSinceMs = 0;
        if (s.wifiConnected) {
            wifiDown = false;
        } else if (!wifiDown) {
            wifiDown = true;
            wifiDownSinceMs = now;
        }

        previous = s;
        if (static_cast<uint32_t>(now - firstMs) < BOOT_GRACE_MS)
            return CcaGatewayRecovery::None;

        if (static_cast<uint32_t>(now - lastModuleMs) >= MODULE_STALL_MS)
            return CcaGatewayRecovery::ModuleStalled;

        // The DM request counter is only a sign that an RF transmission was
        // expected. The actual TX evidence is the TX_DONE interrupt counter.
        if (s.dmRequests >= 2 && static_cast<uint32_t>(now - lastRfMs) >= NO_RF_MS)
            return CcaGatewayRecovery::NoRxOrTx;

        if (s.dmRequests >= 2 && static_cast<uint32_t>(now - lastRxMs) >= RX_HARD_MS)
            return CcaGatewayRecovery::RxSilentThreeHours;

        if (s.dmRequests >= 2 && !softRecovered &&
            static_cast<uint32_t>(now - lastRxMs) >= RX_SOFT_MS) {
            softRecovered = true;
            return CcaGatewayRecovery::SoftRecoverRx;
        }

        if (s.wifiConnected && s.queueUsed && queueOccupiedSinceMs &&
            static_cast<uint32_t>(now - queueOccupiedSinceMs) >= QUEUE_STALL_MS &&
            static_cast<uint32_t>(now - lastHttpAttemptMs) >= QUEUE_STALL_MS)
            return CcaGatewayRecovery::UploadStalled;

        if (s.wifiConnected && failedHttpActive && s.lastHttpStatus < 0 &&
            static_cast<uint32_t>(now - failedHttpSinceMs) >= HTTP_FAILURE_MS)
            return CcaGatewayRecovery::UploadStalled;

        if (!s.wifiConnected && wifiDown &&
            static_cast<uint32_t>(now - wifiDownSinceMs) >= WIFI_FAILURE_MS)
            return CcaGatewayRecovery::WifiStalled;

        return CcaGatewayRecovery::None;
    }

    uint32_t rxAgeMs(uint32_t now) const { return initialized ? static_cast<uint32_t>(now - lastRxMs) : 0; }
    uint32_t rfAgeMs(uint32_t now) const { return initialized ? static_cast<uint32_t>(now - lastRfMs) : 0; }
    uint32_t moduleAgeMs(uint32_t now) const { return initialized ? static_cast<uint32_t>(now - lastModuleMs) : 0; }
    bool provenHealthy() const { return rxObserved && hadAcceptedUpload; }
    bool softRecoveryAttempted() const { return softRecovered; }

  private:
    CcaGatewaySnapshot previous{};
    bool initialized = false;
    bool softRecovered = false;
    bool wifiDown = false;
    bool failedHttpActive = false;
    bool rxObserved = false;
    bool hadAcceptedUpload = false;
    uint32_t firstMs = 0;
    uint32_t lastRxMs = 0;
    uint32_t lastRfMs = 0;
    uint32_t lastModuleMs = 0;
    uint32_t lastHttpAttemptMs = 0;
    uint32_t lastAcceptedMs = 0;
    uint32_t queueOccupiedSinceMs = 0;
    uint32_t failedHttpSinceMs = 0;
    uint32_t wifiDownSinceMs = 0;
};
