#pragma once

#include <cstdint>

// Updated by the gateway's cooperative HTTP workers; any actual HTTP response proves transport.
struct CcaNetworkEvidence {
    uint32_t replies = 0;
    uint8_t configFailures = 0;
    uint8_t legacyFailures = 0;
    int configStatus = 0;
    int healthStatus = 0;
    int legacyStatus = 0;
    int probeStatus = 0;
    int configTls = 0;
    int probeTls = 0;

    void recordConfig(int status)
    {
        configStatus = status;
        countResult(status, configFailures);
    }

    void recordHealth(int status)
    {
        healthStatus = status;
        if (status > 0)
            ++replies;
    }

    void recordUpload(bool cloudOnly, int status)
    {
        if (cloudOnly) {
            if (status > 0)
                ++replies;
        } else {
            legacyStatus = status;
            countResult(status, legacyFailures);
        }
    }

    void recordProbe(int status)
    {
        probeStatus = status;
        countResult(status, legacyFailures);
    }

  private:
    void countResult(int status, uint8_t &failures)
    {
        if (status > 0) {
            ++replies;
            failures = 0;
        } else if (failures < UINT8_MAX) {
            ++failures;
        }
    }
};

class CcaNetworkRecovery
{
  public:
    enum class Action { None, Reconnect, RestartWifi, Reboot };
    static constexpr uint32_t MINUTE = 60000UL;

    Action update(uint32_t now, bool hasIp, const CcaNetworkEvidence &http, uint32_t validConfigs, bool rebootUsed)
    {
        if (!initialized) {
            initialized = true;
            lastReply = lastConfig = now;
        }
        if (http.replies != priorReplies) {
            priorReplies = http.replies;
            lastReply = now;
            repair = Action::None;
        }
        if (validConfigs != priorConfigs) {
            priorConfigs = validConfigs;
            lastConfig = now;
        }
        if (!hasIp && !ipDown)
            ipDownSince = now;
        ipDown = !hasIp;

        const uint32_t silentMs = now - lastReply;
        const uint32_t offlineMs = ipDown ? now - ipDownSince : 0;
        const bool healthy = hasIp && validConfigs != 0 && cloudAge(now) < 8 * MINUTE;
        if (healthy && !trackingHealth)
            healthySince = now;
        trackingHealth = healthy;
        const bool bothFailed = http.configFailures >= 2 && http.legacyFailures >= 1;
        if (offlineMs < 2 * MINUTE && !(bothFailed && silentMs >= 20 * MINUTE))
            return Action::None;

        if (repair == Action::None) {
            repair = Action::Reconnect;
        } else {
            if (now - lastRepair < 5 * MINUTE)
                return Action::None;
            if (repair == Action::RestartWifi && !rebootUsed &&
                (offlineMs >= 60 * MINUTE || (bothFailed && silentMs >= 60 * MINUTE)))
                return Action::Reboot;
            if (offlineMs < 10 * MINUTE && !(bothFailed && silentMs >= 30 * MINUTE))
                return Action::None;
            if (repair == Action::RestartWifi && now - lastRepair < 15 * MINUTE)
                return Action::None;
            repair = Action::RestartWifi;
        }
        lastRepair = now;
        return repair;
    }

    bool stable(uint32_t now) const { return trackingHealth && now - healthySince >= 90 * MINUTE; }
    uint32_t cloudAge(uint32_t now) const { return initialized ? now - lastConfig : 0; }

  private:
    bool initialized = false;
    bool ipDown = false;
    bool trackingHealth = false;
    Action repair = Action::None;
    uint32_t lastReply = 0, priorReplies = 0;
    uint32_t lastConfig = 0, priorConfigs = 0;
    uint32_t ipDownSince = 0, lastRepair = 0, healthySince = 0;
};
