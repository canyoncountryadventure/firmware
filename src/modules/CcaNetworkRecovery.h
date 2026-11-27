#pragma once
#include <cstdint>

// Monotonic network liveness policy; independent of sensor traffic.
// Called from loopTask, covered by the 90-second task watchdog.
class CcaNetworkRecovery {
public:
    enum class Action { None, Reconnect, Reboot };
    static constexpr uint32_t minute = 60000UL;
    Action update(uint32_t now, bool wifi, uint32_t cloudSuccess, bool uploadStalled, uint32_t outageBoots) {
        if (!initialized) {
            initialized = true;
            lastCloud = lastRepair = now;
            priorCloud = cloudSuccess;
        }
        if (cloudSuccess != priorCloud) {
            priorCloud = cloudSuccess;
            lastCloud = now;
        }
        if (!wifi && !wifiDown) { wifiDown = true; wifiSince = now; }
        if (wifi) wifiDown = false;
        if (uploadStalled && !uploadDown) { uploadDown = true; uploadSince = now; }
        if (!uploadStalled) uploadDown = false;
        const uint32_t cloudAge = now - lastCloud;
        const uint32_t wifiAge = wifiDown ? now - wifiSince : 0;
        const uint32_t uploadAge = uploadDown ? now - uploadSince : 0;
        const bool healthy = wifi && cloudSuccess != 0 && cloudAge < 8 * minute && !uploadDown;
        if (healthy && !healthyTracking) { healthyTracking = true; healthySince = now; }
        if (!healthy) healthyTracking = false;
        // No permanent lockout: prolonged infrastructure outages back off to hourly restarts.
        const uint32_t rebootAfter = (outageBoots == 0 ? 20 : outageBoots == 1 ? 40 : 60) * minute;
        if (cloudAge >= rebootAfter || wifiAge >= rebootAfter || uploadAge >= rebootAfter)
            return Action::Reboot;
        const bool repairDue = wifiAge >= 2 * minute || cloudAge >= 10 * minute || uploadDown;
        if (repairDue && (!repaired || now - lastRepair >= 5 * minute)) {
            repaired = true;
            lastRepair = now;
            return Action::Reconnect;
        }
        return Action::None;
    }
    bool stable(uint32_t now) const { return healthyTracking && now - healthySince >= 15 * minute; }
    uint32_t cloudAge(uint32_t now) const { return initialized ? now - lastCloud : 0; }
private:
    bool initialized=false, wifiDown=false, uploadDown=false, repaired=false, healthyTracking=false;
    uint32_t lastCloud=0, priorCloud=0, wifiSince=0, uploadSince=0, lastRepair=0, healthySince=0;
};
