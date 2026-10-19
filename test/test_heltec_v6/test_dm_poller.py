"""Compile the real poller against host fakes and replay CC01 replies in every state."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = (root / 'src/modules/HoboHttpGatewayModule.h').read_text()
body = header[header.index('class HoboHttpGatewayModule'):header.index('extern HoboHttpGatewayModule')]
body = re.sub(r'class HoboHttpGatewayModule\s*:.*?\{', 'class HoboHttpGatewayModule {', body, count=1, flags=re.S)
body = body.replace('private:', 'public:').replace('protected:', 'public:')
body = body.replace('HoboHttpGatewayModule();', 'HoboHttpGatewayModule() : uploadQueue(32) {}\n    void setIntervalFromNow(unsigned long) {}')
body = body.replace(' override', '')
source = (root / 'src/modules/CcaCc01DmPoller.cpp').read_text()
source = re.sub(r'^#include "(?:HoboHttpGatewayModule.h|MeshService.h|Router.h|NodeDB.h|main.h)"\n', '', source, flags=re.M)
source = source.replace('#include "mesh/Throttle.h"', '#include "src/mesh/Throttle.h"')
source = source.replace('#include "CcaCc01DmParser.h"', '#include "src/modules/CcaCc01DmParser.h"')
fixture = r'''
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include <cmath>
using String = std::string;
enum class ProcessMessage { CONTINUE };
#define ARCH_ESP32 1
#define HAS_WIFI 1
#define HOBO_HTTP_GATEWAY_ENABLED 1
void logLine(const char *, ...) {}
#define LOG_INFO(...) logLine(__VA_ARGS__)
#define LOG_WARN(...) logLine(__VA_ARGS__)
uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
#include "src/mesh/Throttle.h"
bool Throttle::isWithinTimespanMs(uint32_t last, uint32_t interval) { return uint32_t(clockMs-last) < interval; }
template <class T> struct TypedQueue {
    std::vector<T> jobs;
    bool full = false;
    TypedQueue(int) {}
    bool enqueue(T job, int) { if (full) return false; jobs.push_back(job); return true; }
};
struct meshtastic_MeshPacket {
    uint32_t id = 1, to = 0, from = 1695962694UL;
    unsigned channel = 0, priority = 0;
    bool want_ack = false;
    struct { struct { uint8_t bytes[241] = {}; size_t size = 0; } payload;
        int portnum = 0; bool want_response = false; } decoded;
};
constexpr int meshtastic_PortNum_TEXT_MESSAGE_APP = 1;
constexpr int meshtastic_MeshPacket_Priority_RELIABLE = 1;
constexpr int RX_SRC_LOCAL = 1;
struct NodeDB { uint32_t getNodeNum() { return 2740603892UL; } } db;
NodeDB *nodeDB = &db;
struct Router { meshtastic_MeshPacket *allocForSending() { return new meshtastic_MeshPacket; } } routerInstance;
Router *router = &routerInstance;
struct MeshService { void sendToMesh(meshtastic_MeshPacket *p, int, bool) { delete p; } } serviceInstance;
MeshService *service = &serviceInstance;
'''
checks = r'''
void HoboHttpGatewayModule::fillCommon(UploadJob &job, const meshtastic_MeshPacket &mp) {
    job.packetId = mp.id; job.from = mp.from;
}
meshtastic_MeshPacket reply(const char *text) {
    meshtastic_MeshPacket p; p.to = 2740603892UL;
    p.decoded.payload.size = strlen(text);
    memcpy(p.decoded.payload.bytes, text, p.decoded.payload.size);
    return p;
}
int main() {
    using G = HoboHttpGatewayModule;
    const auto water = reply("WATER READ\nRaw: 8.865 ft (2702mm)\nStage: 0.577 ft (17.6 cm)\nSamples:10 Spread:1mm");
    const auto hobo = reply("MX2001\nLogger: E3:1A:98:DC:FB:C2\nBLE: -76 dBm\nLevel: 0.54 ft\nTemp: 49.4 F");

    // Parsed replies are useful at any time, even outside scheduled polling.
    G manual; manual.handleCc01Reply(water); manual.handleCc01Reply(hobo);
    assert(manual.uploadQueue.jobs.size() == 2 && manual.cc01Queued == 2);
    assert(manual.uploadQueue.jobs[0].type == G::JobType::WATER_DISTANCE);
    assert(manual.uploadQueue.jobs[0].distanceMm == 2702);
    assert(manual.uploadQueue.jobs[0].stageMm == 176);
    assert(manual.uploadQueue.jobs[1].type == G::JobType::MX2001);
    assert(fabs(manual.uploadQueue.jobs[1].waterLevelFt - .54f) < .001f);
    assert(fabs(manual.uploadQueue.jobs[1].temperatureF - 49.4f) < .001f);

    // No send before two minutes. Initial WATER times out and retries,
    // then the first READ runs after radio spacing, not after WATER success.
    G g; clockMs = 119999; g.pollCc01(); assert(g.cc01Requests == 0);
    clockMs = 120000; g.pollCc01();
    assert(g.cc01Requests == 1 && g.cc01WaterPending && g.cc01HoboPending);
    assert(g.cc01WaterSent && !g.cc01HoboSent);
    clockMs = 239999; g.pollCc01(); assert(g.cc01Requests == 1);
    clockMs = 240000; g.pollCc01();
    assert(g.cc01Requests == 2 && g.cc01WaterRetries == 1);
    clockMs = 242999; g.pollCc01(); assert(g.cc01Requests == 2);
    clockMs = 243000; g.pollCc01();
    assert(g.cc01Requests == 3 && g.cc01HoboSent);
    clockMs = 360000; g.pollCc01();
    assert(g.cc01Requests == 4 && g.cc01WaterRetries == 2);
    clockMs = 362999; g.pollCc01(); assert(g.cc01HoboRetries == 0);
    clockMs = 363000; g.pollCc01();
    assert(g.cc01Requests == 5 && g.cc01HoboRetries == 1);

    // Invalid format, including "unavailable", satisfies neither request.
    g.handleCc01Reply(reply("HOBO unavailable"));
    g.handleCc01Reply(reply("WATER READ\nRaw: 8.865 ft (2702mm)\nStage: NOT CALIBRATED\nSamples:10"));
    assert(g.cc01Rejected == 2 && g.cc01WaterPending && g.cc01HoboPending);

    // A valid HOBO stops only HOBO retries; WATER continues until valid.
    g.handleCc01Reply(hobo);
    assert(!g.cc01HoboPending && g.cc01WaterPending);
    clockMs = 480000; g.pollCc01();
    assert(g.cc01Requests == 6 && g.cc01WaterRetries == 3 && g.cc01HoboRetries == 1);
    g.handleCc01Reply(water);
    assert(!g.cc01WaterPending && !g.cc01HoboPending && g.cc01Queued == 2);
    const auto n = g.cc01Requests;
    clockMs = 600000; g.pollCc01(); assert(g.cc01Requests == n);

    // Opposite success order: WATER stops, READ continues independently.
    G opposite; clockMs = 120000; opposite.pollCc01();
    clockMs = 120100; opposite.handleCc01Reply(water);
    assert(!opposite.cc01WaterPending && opposite.cc01HoboPending);
    clockMs = 123099; opposite.pollCc01(); assert(opposite.cc01Requests == 1);
    clockMs = 123100; opposite.pollCc01();
    assert(opposite.cc01Requests == 2 && opposite.cc01HoboSent);
    clockMs = 243100; opposite.pollCc01();
    assert(opposite.cc01Requests == 3 && opposite.cc01HoboRetries == 1);
    assert(opposite.cc01WaterRetries == 0 && !opposite.cc01WaterPending);

    // Full upload queue never counts as success for either sensor.
    G full; clockMs = 120000; full.pollCc01();
    full.uploadQueue.full = true;
    full.handleCc01Reply(water); full.handleCc01Reply(hobo);
    assert(full.cc01QueueFull == 2 && full.cc01WaterPending && full.cc01HoboPending);
    full.uploadQueue.full = false;
    full.handleCc01Reply(hobo);
    assert(!full.cc01HoboPending && full.cc01WaterPending);
    full.handleCc01Reply(water);
    assert(!full.cc01WaterPending && full.cc01Queued == 2);

    // The 30-minute cycle re-arms successful types, but does not reset
    // outstanding retries or suppress the opposite type.
    G newCycle; clockMs = 120000; newCycle.pollCc01();
    clockMs = 120100; newCycle.handleCc01Reply(hobo);
    clockMs = 120200; newCycle.handleCc01Reply(water);
    assert(!newCycle.cc01HoboPending && !newCycle.cc01WaterPending);
    clockMs = 120000 + G::CC01_POLL_INTERVAL_MS;
    newCycle.pollCc01();
    assert(newCycle.cc01WaterPending && newCycle.cc01HoboPending &&
           newCycle.cc01Requests == 2 && !newCycle.cc01HoboSent);

    G pendingCycle; clockMs = 120000; pendingCycle.pollCc01();
    clockMs = 120100; pendingCycle.handleCc01Reply(hobo);
    clockMs = 120000 + G::CC01_POLL_INTERVAL_MS;
    pendingCycle.pollCc01();
    // WATER was still missing. Its request timer remains at the last retry;
    // the new HOBO cycle is re-armed rather than lost.
    assert(pendingCycle.cc01WaterPending && pendingCycle.cc01HoboPending &&
           pendingCycle.cc01WaterRetries == 1 && pendingCycle.cc01Requests == 2);
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    cpp = path / 'poller.cpp'
    cpp.write_text(fixture + body + source + checks)
    exe = path / 'poller'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(root), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: actual CC01 poller accepts valid manual/late/out-of-order replies, rejects invalid readings, preserves schedule and reports queue failure')
