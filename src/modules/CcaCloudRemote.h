#pragma once
#include "configuration.h"
#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)

#include "concurrency/OSThread.h"
#include <Arduino.h>
#include <cstdint>

// V10: outbound-only Cloudflare management and per-node destination routing.
// Cached station IDs survive Wi-Fi outages and reboots. Existing V9 node list,
// AUTO DM clock policy, HOBO/mesh behavior and Vercel routing are unaffected.
class CcaCloudRemote : private concurrency::OSThread {
  public:
    CcaCloudRemote();
    static bool isCloudNode(uint32_t id);
    static uint32_t configVersion();
    static String statusJson();
  protected:
    int32_t runOnce() override;
  private:
    uint32_t lastConfigPoll = 0;
    uint32_t lastHealthPost = 0;
    uint32_t lastOtaCheck = 0;
    uint32_t bootAt = 0;
    uint32_t syncOk = 0;
    uint32_t syncFail = 0;
    uint32_t lastHttpStatus = 0;
    bool syncConfig();
    void postHealth();
    bool applyOta(const String &version, const String &url, const String &sha, const String &signature);
};

extern CcaCloudRemote *ccaCloudRemote;

#ifndef CCA_CLOUD_URL
#define CCA_CLOUD_URL "https://cces-mesh-remote.raspy-lake-a7f4.workers.dev"
#endif

#endif
