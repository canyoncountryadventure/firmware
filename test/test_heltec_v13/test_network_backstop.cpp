#include "src/modules/CcaNetworkRecovery.h"
#include <cassert>
#include <cstdio>

using Action = CcaNetworkRecovery::Action;
constexpr uint32_t minute = CcaNetworkRecovery::MINUTE;

static void backendIsolation()
{
    for (int scenario = 0; scenario < 6; ++scenario) {
        CcaNetworkRecovery policy;
        CcaNetworkEvidence http;
        uint32_t configs = 0;
        for (uint32_t m = 0; m <= 180; ++m) {
            if (m % 5 == 0) {
                const int configStatus = scenario == 0 ? 200 : scenario == 1 ? 401 : -1;
                http.recordConfig(configStatus);
                if (configStatus == 200)
                    ++configs;
                if (scenario == 2)
                    http.recordProbe(405); // A quiet mesh still gets an independent check.
                if (scenario == 3)
                    http.recordUpload(false, 201);
                if (scenario == 4 && m % 15 == 0)
                    http.recordHealth(503);
                if (scenario == 5)
                    http.recordUpload(true, 201);
            }
            http.recordUpload(false, -1);
            assert(policy.update(m * minute, true, http, configs, false) == Action::None);
        }
        assert(policy.stable(180 * minute) == (scenario == 0));
    }
}

static void totalOutage(bool hasIp, uint32_t start)
{
    CcaNetworkRecovery policy;
    CcaNetworkEvidence http;
    bool rebootUsed = false;
    unsigned int rejoins = 0, driverRestarts = 0, fullReboots = 0;
    for (uint32_t m = 0; m <= 300; ++m) {
        if (hasIp && m % 5 == 0) {
            http.recordConfig(-1);
            http.recordProbe(-1);
        }
        const auto action = policy.update(start + m * minute, hasIp, http, 0, rebootUsed);
        if (action == Action::Reconnect) {
            ++rejoins;
            if (!rebootUsed)
                assert(m == (hasIp ? 20U : 2U));
        } else if (action == Action::RestartWifi) {
            if (!driverRestarts)
                assert(m == (hasIp ? 30U : 10U));
            ++driverRestarts;
        } else if (action == Action::Reboot) {
            assert(driverRestarts > 0 && m == 60 && !rebootUsed);
            ++fullReboots;
            rebootUsed = true;
            policy = {};
            http = {};
        }
    }
    assert(rejoins == 2 && driverRestarts > 10 && fullReboots == 1);
}

static void transientAndRearm()
{
    CcaNetworkRecovery policy;
    CcaNetworkEvidence http;
    uint32_t configs = 0;
    for (uint32_t m = 0; m <= 195; ++m) {
        const bool hasIp = m != 96;
        if (m % 5 == 0 && hasIp) {
            http.recordConfig(200);
            ++configs;
        }
        if (m % 3 == 0) {
            http.recordUpload(false, -1);
            http.recordUpload(false, -1);
        }
        assert(policy.update(m * minute, hasIp, http, configs, true) == Action::None);
        const bool expected = (m >= 90 && m < 96) || m >= 187;
        assert(policy.stable(m * minute) == expected);
    }
}

static void intermittentAndRecovery()
{
    CcaNetworkRecovery policy;
    CcaNetworkEvidence http;
    for (uint32_t m = 0; m < 60; ++m) {
        if (m % 5 == 0) {
            http.recordConfig(-1);
            http.recordProbe(-1);
        }
        const bool hasIp = m % 3 != 1;
        assert(policy.update(m * minute, hasIp, http, 0, false) != Action::Reboot);
    }
    http.recordUpload(false, 201);
    assert(policy.update(60 * minute, true, http, 0, false) == Action::None);
    // Losing config validation or an idle queue alone cannot prove both paths are down.
    assert(policy.update(121 * minute, true, http, 0, false) == Action::None);
}

int main()
{
    backendIsolation();
    totalOutage(true, 0);
    totalOutage(false, 0);
    totalOutage(true, UINT32_MAX - 3 * minute);
    totalOutage(false, UINT32_MAX - 3 * minute);
    transientAndRearm();
    intermittentAndRecovery();
    std::puts("PASS: endpoint isolation, idle sensor network, confirmed outage escalation, retained reboot budget, health rearm and timer wrap");
}
