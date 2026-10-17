#pragma once
// RAK V6 preferred FIRST relay for outbound broadcast sensor packets.
// This is NOT a destination override (HUB SET remains separate).
// Default is unrestricted Meshtastic broadcast. No storage writes per packet.
#include "configuration.h"
#if defined(ARCH_NRF52) && defined(RAK_4631)
#include "FSCommon.h"
#include "SPILock.h"
#include "NodeDB.h"
#include "MeshTypes.h"
#include "concurrency/LockGuard.h"
#include <cstdint>
#include <cstring>

struct WaterRelayRecord {
    uint32_t magic;
    uint32_t preferredNode;
    uint32_t checksum;
};

inline WaterRelayRecord &waterRelayRecord()
{
    static WaterRelayRecord saved = [] {
        WaterRelayRecord rec{0x36594C52UL, 0, 0};
        concurrency::LockGuard g(spiLock);
        File f = FSCom.open("/prefs/water_relay_v6.bin", FILE_O_READ);
        if (f) {
            WaterRelayRecord stored = {};
            if (f.size() == sizeof(stored) &&
                f.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored) &&
                stored.magic == 0x36594C52UL &&
                stored.checksum == (stored.magic ^ stored.preferredNode ^ 0x7B29D051UL)) {
                rec = stored;
            }
            f.close();
        }
        return rec;
    }();
    return saved;
}

inline uint32_t &waterRelayOnceNode()
{
    static uint32_t node = 0; // RAM only, expires when the next broadcast sensor packet is queued
    return node;
}

inline uint32_t waterRelayPreferredNode()
{
    return waterRelayRecord().preferredNode;
}

inline uint32_t waterRelayPendingOnce()
{
    return waterRelayOnceNode();
}

inline bool waterRelayValidNode(uint32_t node)
{
    // A first-hop identifier is one byte on the wire. Reject reserved,
    // self-targeted and broadcast IDs. Avoid truncation of unvalidated IDs.
    if (node == 0 || node == 0xFFFFFFFFUL || !nodeDB)
        return false;
    const uint8_t last = nodeDB->getLastByteOfNodeNum(node);
    return last != 0 && last != 0xFF &&
           last != nodeDB->getLastByteOfNodeNum(nodeDB->getNodeNum()) &&
           node != nodeDB->getNodeNum();
}

inline bool setWaterRelayPreferredNode(uint32_t node)
{
    if (node != 0 && !waterRelayValidNode(node))
        return false;
    WaterRelayRecord updated{0x36594C52UL, node, 0};
    updated.checksum = updated.magic ^ updated.preferredNode ^ 0x7B29D051UL;
    bool stored = false;
    {
        concurrency::LockGuard g(spiLock);
        File f = FSCom.open("/prefs/water_relay_v6.bin", FILE_O_WRITE);
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
    {
        concurrency::LockGuard g(spiLock);
        File f = FSCom.open("/prefs/water_relay_v6.bin", FILE_O_READ);
        WaterRelayRecord verify = {};
        stored = f && f.size() == sizeof(verify) &&
                 f.read(reinterpret_cast<uint8_t *>(&verify), sizeof(verify)) == sizeof(verify) &&
                 memcmp(&verify, &updated, sizeof(updated)) == 0;
        if (f)
            f.close();
    }
    if (stored) {
        waterRelayRecord() = updated;
        waterRelayOnceNode() = 0;
    }
    return stored;
}

inline bool setWaterRelayOnceNode(uint32_t node)
{
    if (!waterRelayValidNode(node))
        return false;
    waterRelayOnceNode() = node;
    return true;
}

// Only called by the two real sensor data emitters. Direct replies,
// unrelated mesh traffic, and unicast HUB packets retain stock routing.
inline void waterRelayPrepareTelemetry(meshtastic_MeshPacket *packet)
{
    if (!packet || !nodeDB || packet->to != NODENUM_BROADCAST)
        return;
    const uint32_t pending = waterRelayOnceNode();
    const uint32_t target = pending ? pending : waterRelayPreferredNode();
    if (!waterRelayValidNode(target))
        return;
    packet->next_hop = nodeDB->getLastByteOfNodeNum(target);
    if (pending)
        waterRelayOnceNode() = 0;
}
#endif
