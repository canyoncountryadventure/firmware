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
    for (auto state : {G::Cc01PollState::IDLE, G::Cc01PollState::WAIT_WATER,
                       G::Cc01PollState::SEND_HOBO, G::Cc01PollState::WAIT_HOBO}) {
        G g; g.cc01PollState = state;
        g.handleCc01Reply(water);
        assert(g.uploadQueue.jobs.size() == 1 && g.cc01Parsed == 1 && g.cc01Queued == 1);
        const auto &w = g.uploadQueue.jobs[0];
        assert(w.type == G::JobType::WATER_DISTANCE && w.distanceMm == 2702 && w.dmPollResult);
        assert(w.from == 1695962694UL && w.packetId == water.id && w.stageMm == 176);
        assert(g.cc01PollState == (state == G::Cc01PollState::WAIT_WATER ? G::Cc01PollState::SEND_HOBO : state));
        G h; h.cc01PollState = state; h.handleCc01Reply(hobo);
        assert(h.uploadQueue.jobs.size() == 1 && h.cc01Queued == 1);
        const auto &r = h.uploadQueue.jobs[0];
        assert(r.type == G::JobType::MX2001 && r.dmPollResult && r.bleRssi == -76);
        assert(fabs(r.waterLevelFt - 0.54f) < 0.001f && fabs(r.temperatureF - 49.4f) < 0.001f);
        assert(!strcmp(r.loggerMac, "E3:1A:98:DC:FB:C2"));
        assert(h.cc01PollState == (state == G::Cc01PollState::WAIT_HOBO ? G::Cc01PollState::IDLE : state));
    }
    G invalid; invalid.cc01PollState = G::Cc01PollState::WAIT_WATER;
    invalid.handleCc01Reply(reply("HOBO unavailable"));
    invalid.handleCc01Reply(reply("WATER READ\nRaw: 8.865 ft (2702mm)\nStage: NOT CALIBRATED\nSamples:10"));
    assert(invalid.uploadQueue.jobs.empty() && invalid.cc01Rejected == 2);
    assert(invalid.cc01PollState == G::Cc01PollState::WAIT_WATER);
    G full; full.uploadQueue.full = true; full.handleCc01Reply(hobo);
    assert(full.cc01Parsed == 1 && full.cc01Queued == 0 && full.cc01QueueFull == 1);
    G poll; clockMs = 119999; poll.pollCc01(); assert(poll.cc01Requests == 0);
    clockMs = 120000; poll.pollCc01(); assert(poll.cc01Requests == 1);
    assert(poll.cc01PollState == G::Cc01PollState::WAIT_WATER);
    clockMs += 120000; poll.pollCc01(); assert(poll.cc01PollState == G::Cc01PollState::SEND_HOBO);
    clockMs += 3000; poll.pollCc01(); assert(poll.cc01Requests == 2);
    assert(poll.cc01PollState == G::Cc01PollState::WAIT_HOBO);
    clockMs += 120000; poll.pollCc01(); assert(poll.cc01PollState == G::Cc01PollState::IDLE);
    poll.handleCc01Reply(hobo); assert(poll.cc01Queued == 1); // late reply after timeout
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
