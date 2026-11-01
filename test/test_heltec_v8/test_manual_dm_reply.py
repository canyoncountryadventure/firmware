#!/usr/bin/env python3
"""Compile the actual V8 unsolicited manual DM handler against host fakes."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/modules/CcaCc01DmPoller.cpp").read_text()
pattern = r"void HoboHttpGatewayModule::handleManualSensorReply\(const meshtastic_MeshPacket &mp\)\s*\{"
match = re.search(pattern, source)
assert match, "manual DM handler missing"
depth = 0
method = None
for i in range(match.end() - 1, len(source)):
    if source[i] == "{":
        depth += 1
    elif source[i] == "}":
        depth -= 1
        if depth == 0:
            method = source[match.start():i + 1]
            break
assert method is not None

header = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "src/modules/AutoFallbackPolicy.h"
#include "src/modules/CcaCc01DmParser.h"

constexpr uint8_t CC01_LONGFAST_CHANNEL = 1;
constexpr uint32_t CC01_POLL_NODE = 1695962694UL;
constexpr uint32_t PACK_CREEK_POLL_NODE = 1949224949UL;
constexpr uint32_t MILL_CREEK_POLL_NODE = 1683604927UL;
constexpr uint32_t HIDDEN_VALLEY_POLL_NODE = 3044869407UL;
constexpr uint32_t GATEWAY_NODE = 2740603892UL;
struct FakeNodeDB {
    uint32_t getNodeNum() const { return GATEWAY_NODE; }
};
FakeNodeDB fakeDb;
FakeNodeDB *nodeDB = &fakeDb;
struct meshtastic_MeshPacket {
    uint32_t from = MILL_CREEK_POLL_NODE, to = GATEWAY_NODE, id = 12;
    bool pki_encrypted = true;
    uint8_t channel = 0;
    struct {
        struct {
            uint8_t bytes[241] = {};
            size_t size = 0;
        } payload;
    } decoded;
};
uint32_t getFrom(const meshtastic_MeshPacket *p) { return p->from; }

class HoboHttpGatewayModule {
public:
    enum class JobType : uint8_t { MX2001, WATER_DISTANCE, ENVIRONMENT };
    struct UploadJob {
        JobType type = JobType::ENVIRONMENT;
        uint32_t from = 0, packetId = 0;
        bool dmPollResult = false, distanceValid = false, stageCalibrated = false;
        float waterLevelFt = 0, temperatureF = 0, temperatureC = 0;
        int8_t bleRssi = 0;
        uint8_t distanceSensorType = 0;
        uint32_t distanceMm = 0;
        int32_t stageMm = 0;
        char loggerMac[18] = {}, loggerModel[12] = {};
    };
    struct Queue {
        std::vector<UploadJob> saved;
        bool full = false;
        bool enqueue(const UploadJob &job, int) {
            if (full) return false;
            saved.push_back(job); return true;
        }
    } uploadQueue;
    cca_auto_fallback::Policy autoFallback;
    uint32_t manualDmReceived = 0, manualDmQueued = 0, manualDmRejected = 0, cc01QueueFull = 0;
    void fillCommon(UploadJob &job, const meshtastic_MeshPacket &packet) {
        job.from = packet.from;
        job.packetId = packet.id;
    }
    void setIntervalFromNow(uint32_t) {}
    void handleManualSensorReply(const meshtastic_MeshPacket &mp);
};
meshtastic_MeshPacket packet(const char *s, uint32_t from = MILL_CREEK_POLL_NODE) {
    meshtastic_MeshPacket p;
    p.from = from;
    p.decoded.payload.size = std::strlen(s);
    assert(p.decoded.payload.size < sizeof(p.decoded.payload.bytes));
    std::memcpy(p.decoded.payload.bytes, s, p.decoded.payload.size);
    return p;
}
"""
tests = r"""
int main() {
    const char *logger = "MX2001\nLogger: E3:1A:98:DC:FB:C2\nBLE: -76 dBm\nLevel: 1.54 ft\nTemp: 55.7 F";
    HoboHttpGatewayModule g;
    auto mill = packet(logger);
    mill.id = 897;
    g.handleManualSensorReply(mill);
    assert(g.manualDmReceived == 1 && g.manualDmQueued == 1 && g.manualDmRejected == 0);
    assert(g.uploadQueue.saved.size() == 1);
    const auto &first = g.uploadQueue.saved[0];
    assert(first.type == HoboHttpGatewayModule::JobType::MX2001);
    assert(first.from == MILL_CREEK_POLL_NODE && first.packetId == 897 && first.dmPollResult);
    assert(std::fabs(first.waterLevelFt - 1.54f) < 0.001f);
    assert(std::fabs(first.temperatureF - 55.7f) < 0.001f);

    // Manually read even when AUTO rescue is disabled.
    g.autoFallback.pauseAll();
    g.handleManualSensorReply(mill);
    assert(g.manualDmQueued == 2);

    // Wrong destination, channel, unknown node are never accepted.
    auto wrong = mill; wrong.to = 0xDEADBEEF;
    g.handleManualSensorReply(wrong); assert(g.manualDmQueued == 2);
    wrong = mill; wrong.channel = 2;
    g.handleManualSensorReply(wrong); assert(g.manualDmQueued == 2);
    wrong = mill; wrong.from = 0xDEADBEEF;
    g.handleManualSensorReply(wrong); assert(g.manualDmQueued == 2);

    auto invalid = packet("HOBO unavailable");
    g.handleManualSensorReply(invalid);
    assert(g.manualDmQueued == 2 && g.manualDmRejected == 1);
    invalid = packet("READ failed: TIMEOUT");
    g.handleManualSensorReply(invalid);
    assert(g.manualDmQueued == 2 && g.manualDmRejected == 2);

    // Pack Creek's raw/stage reply is recorded with its own sensor identity.
    auto pack = packet("WATER READ\nRaw: 8.865 ft (2702 mm)\nStage: 0.577 ft\nSamples:10 Spread:1mm",
                       PACK_CREEK_POLL_NODE);
    g.handleManualSensorReply(pack);
    assert(g.manualDmQueued == 3);
    const auto &water = g.uploadQueue.saved.back();
    assert(water.type == HoboHttpGatewayModule::JobType::WATER_DISTANCE);
    assert(water.distanceSensorType == 3 && water.stageCalibrated && water.distanceValid);

    // A temperature-only station's MX2001 must not invent stage.
    auto temp = packet(logger, HIDDEN_VALLEY_POLL_NODE);
    g.handleManualSensorReply(temp);
    assert(g.manualDmQueued == 4);
    assert(g.uploadQueue.saved.back().type == HoboHttpGatewayModule::JobType::ENVIRONMENT);

    // The upload queue can fail without a false success count.
    g.uploadQueue.full = true;
    g.handleManualSensorReply(mill);
    assert(g.manualDmQueued == 4 && g.cc01QueueFull == 1);
    std::puts("PASS: manual Mill Creek MX2001 & Pack stage replies; no spoof/timeout/queue surprises");
}
"""
with tempfile.TemporaryDirectory() as folder:
    f = Path(folder) / "manual_dm.cpp"
    f.write_text(header + "\n" + method + "\n" + tests)
    output = Path(folder) / "manual_dm"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root), str(f), "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)
