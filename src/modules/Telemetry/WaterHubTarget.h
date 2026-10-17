#pragma once
// V6 preferred telemetry destination. Default remains ordinary mesh broadcast.
// A valid saved hub survives application-only UF2/DFU and normal reboots.
#include "configuration.h"
#if defined(ARCH_NRF52)
#include "FSCommon.h"
#include "SPILock.h"
#include "concurrency/LockGuard.h"
#include <cstdint>
#include <cstring>

struct WaterHubRecord {
    uint32_t magic;
    uint32_t destination;
    uint32_t checksum;
};

inline WaterHubRecord &waterHubRecord()
{
    static WaterHubRecord state = [] {
        WaterHubRecord rec{0x36554248UL, 0xFFFFFFFFUL, 0};
        {
            concurrency::LockGuard g(spiLock);
            File f = FSCom.open("/prefs/water_hub_v6.bin", FILE_O_READ);
            if (f) {
                WaterHubRecord stored = {};
                if (f.size() == sizeof(stored) && f.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored) &&
                    stored.magic == 0x36554248UL && stored.checksum == (stored.magic ^ stored.destination ^ 0x76543210UL) &&
                    stored.destination != 0) {
                    rec = stored;
                }
                f.close();
            }
        }
        return rec;
    }();
    return state;
}

inline uint32_t waterHubDestination()
{
    return waterHubRecord().destination;
}

inline bool setWaterHubDestination(uint32_t destination)
{
    if (destination == 0)
        return false;
    WaterHubRecord updated{0x36554248UL, destination, 0};
    updated.checksum = updated.magic ^ updated.destination ^ 0x76543210UL;
    bool stored = false;
    {
        concurrency::LockGuard g(spiLock);
        File f = FSCom.open("/prefs/water_hub_v6.bin", FILE_O_WRITE);
        if (!f)
            return false;
        if (f.seek(0) && f.truncate() &&
            f.write(reinterpret_cast<const uint8_t *>(&updated), sizeof(updated)) == sizeof(updated)) {
            f.flush();
            stored = true;
        }
        f.close();
    }
    if (!stored)
        return false;
    // Verify using a separate reader rather than trusting flash write return.
    {
        concurrency::LockGuard g(spiLock);
        File f = FSCom.open("/prefs/water_hub_v6.bin", FILE_O_READ);
        WaterHubRecord verify = {};
        stored = f && f.size() == sizeof(verify) &&
                 f.read(reinterpret_cast<uint8_t *>(&verify), sizeof(verify)) == sizeof(verify) &&
                 memcmp(&verify, &updated, sizeof(updated)) == 0;
        if (f)
            f.close();
    }
    if (stored)
        waterHubRecord() = updated;
    return stored;
}
#endif
