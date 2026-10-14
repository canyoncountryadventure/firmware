"""Compile actual lifecycle functions against fault-injected Bluefruit boundaries.

These host tests do not simulate SoftDevice timing or establish RF reliability.
"""

from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (
    root
    / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp"
).read_text()
platform = (root / "src/platform/nrf52/NRF52Bluetooth.cpp").read_text()


def block(signature, text=source):
    match = re.search(re.escape(signature).replace(r"\ ", r"\s*") + r"\s*\{", text)
    assert match, signature
    opening = text.index("{", match.start())
    depth, pos = 1, opening + 1
    while depth:
        depth += (text[pos] == "{") - (text[pos] == "}")
        pos += 1
    return text[match.start() : pos]


prelude = r"""
#include <atomic>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#define LOG_ERROR(...) do {if(false) printf(__VA_ARGS__);}while(0)
#define LOG_INFO(...) do {if(false) printf(__VA_ARGS__);}while(0)
#define LOG_WARN(...) do {if(false) printf(__VA_ARGS__);}while(0)
constexpr uint16_t BLE_CONN_HANDLE_INVALID=65535;
constexpr unsigned BLE_MAX_CONNECTION=8;
constexpr uint32_t NRF_SUCCESS=0,NRF_ERROR_INVALID_STATE=8;
constexpr unsigned BLE_GAP_EVT_TIMEOUT=1,BLE_GAP_TIMEOUT_SRC_CONN=2,BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP=3, BLE_GAP_EVT_CONNECTED=4, BLE_GAP_EVT_DISCONNECTED=5;
uint32_t clockMs=100; uint32_t millis(){return clockMs;}
void delay(uint32_t t){clockMs+=t;} void yield(){clockMs++;}
namespace concurrency {struct Lock {bool held=false;};struct LockGuard {
 Lock& lock;LockGuard(Lock* l):lock(*l){assert(!lock.held);lock.held=true;}
 ~LockGuard(){lock.held=false;}};}
namespace Throttle {bool isWithinTimespanMs(uint32_t last,uint32_t duration){return uint32_t(clockMs-last)<duration;}}
struct ble_gap_addr_t {uint8_t addr[6]={};uint8_t addr_type=1;};
struct ble_gap_evt_adv_report_t {struct {bool connectable=true;} type;
 ble_gap_addr_t peer_addr;int8_t rssi=-72;struct {uint8_t* p_data=nullptr;uint16_t len=0;}data;};
struct ble_evt_t {struct {unsigned evt_id=0;}header;struct {
 struct {uint16_t conn_handle=BLE_CONN_HANDLE_INVALID;}common_evt;
 struct {struct {struct {unsigned src=0;}timeout;struct {uint8_t reason=0;}disconnected;}params;}gap_evt;
 struct {uint16_t gatt_status=0;struct {struct {uint16_t count=1;}prim_srvc_disc_rsp;}params;}gattc_evt;}evt;};
struct Scan {bool running=false,startOk=true,resumeOk=true,stopOk=true;unsigned starts=0,stops=0,resumes=0;
 bool start(int){starts++;if(startOk)running=true;return startOk;}
 bool stop(){stops++;if(stopOk)running=false;return stopOk;}
 bool resume(){resumes++;return resumeOk;}bool isRunning(){return running;}};
struct BF {Scan Scanner;bool links[BLE_MAX_CONNECTION]={};bool phones[BLE_MAX_CONNECTION]={};
 std::vector<uint16_t> disconnects;bool connectOk=true;uint8_t addressType=0;
 struct CentralType {BF* bf;unsigned connected(){unsigned n=0;for(bool v:bf->links)n+=v;return n;}
 bool connected(uint16_t h){return h<BLE_MAX_CONNECTION && bf->links[h];}
 bool connect(ble_gap_evt_adv_report_t* r){bf->addressType=r->peer_addr.addr_type;return bf->connectOk;}}Central{this};
 struct PeriphType {BF* bf;unsigned connected(){unsigned n=0;for(bool v:bf->phones)n+=v;return n;}
 bool connected(uint16_t h){return h<BLE_MAX_CONNECTION&&bf->phones[h];}}Periph{this};
 bool disconnect(uint16_t h){disconnects.push_back(h);if(h<BLE_MAX_CONNECTION){phones[h]=false;links[h]=false;}return true;}
} Bluefruit;
uint32_t cancelResult=NRF_SUCCESS;unsigned cancels=0;
uint32_t sd_ble_gap_connect_cancel(){cancels++;return cancelResult;}
void nrf52FieldDiagEvent(int,int,int){}
struct Svc {unsigned calls=0,failures=0;bool drop=false;bool discover(uint16_t); }hoboService;
struct Chr {unsigned discoverCalls=0,notifyCalls=0,discoverFailures=0,notifyFailures=0;
 bool discover();bool enableNotify();}hoboCharacteristic;
void triggerBleRecovery(const char*);
"""
globals_text = source[
    source.index("enum class LoggerType") : source.index("bool reached(")
]
uuid_text = source[
    source.index("static const uint8_t HOBO_SERVICE_UUID") : source.index(
        "BLEClientService hoboService"
    )
]
candidate_struct = source[
    source.index("struct CandidateInfo") : source.index(
        "CandidateInfo inspectAdvertisement"
    )
]
methods = "\n".join(
    block(signature)
    for signature in [
        "bool reached(uint32_t now, uint32_t target)",
        "void makeHumanMac(const uint8_t in[6], uint8_t out[6])",
        "bool containsAsciiIgnoreCase(const uint8_t *data, uint16_t length, const char *needle)",
        "CandidateInfo inspectAdvertisement(const ble_gap_evt_adv_report_t *report)",
        "bool isRejectedCandidate(const uint8_t rawAddr[6])",
        "void rejectCurrentCandidate(uint32_t retryMs = REJECT_RETRY_MS)",
        "void resetMeasurementCapture()",
        "void resumeScanner()",
        "bool restartScanner(uint32_t now)",
        "void bleEventCallback(ble_evt_t *event)",
        "void scanCallback(ble_gap_evt_adv_report_t *report)",
        "void triggerBleRecovery(const char *reason)",
        "void connectCallback(uint16_t connHandle)",
        "void disconnectCallback(uint16_t connHandle, uint8_t reason)",
        "const char *loggerTypeName(LoggerType type)",
    ]
)
logger_start = source.index("char reply[220]", source.index('"LOGGER"'))
logger_body = source[logger_start : source.index("sendTextReply(mp.from", logger_start)]
lifecycle_start = source.index(
    "    if (setupInProgress || recoveryPending)", source.index("int32_t HOBOMX")
)
lifecycle_end = source.index("    if (readRequestPending &&", lifecycle_start)
lifecycle = (
    "int lifecycleTick(){const uint32_t now=millis();\n"
    + source[lifecycle_start:lifecycle_end]
    + "return 100;}\n"
)
helpers = r"""
bool Svc::discover(uint16_t h){assert(setupInProgress);assert(lifecycleTick()==500);calls++;
 if(drop){Bluefruit.links[h]=false;return false;}
 return calls>failures;}
bool Chr::discover(){assert(setupInProgress);return ++discoverCalls>discoverFailures;}
bool Chr::enableNotify(){assert(setupInProgress);return ++notifyCalls>notifyFailures;}
struct NRF52Bluetooth {void disconnect();};
"""
helpers += block("void NRF52Bluetooth::disconnect()", platform)
helpers += (
    "\nconst char* loggerSummary(){static char output[220];\n"
    + logger_body
    + 'snprintf(output,sizeof(output),"%s",reply);return output;}\n'
)
tests = r"""
void reset(){connecting=false;connected=false;setupInProgress=false;recoveryPending=false;
 connectionHandle=BLE_CONN_HANDLE_INVALID;connectAttemptStartedMs=100;clockMs=100;
 bleRecoveryCycles=0;scanRestartRequested=false;connectTimedOut=false;scanRetryDueMs=0;
 universalState=UniversalState::IDLE;loggerLockEnabled=false;haveRejectedAddr=false;
 Bluefruit.Scanner=Scan{};Bluefruit.disconnects.clear();Bluefruit.connectOk=true;
 memset(Bluefruit.links,0,sizeof(Bluefruit.links));memset(Bluefruit.phones,0,sizeof(Bluefruit.phones));
 hoboService=Svc{};hoboCharacteristic=Chr{};cancels=0;cancelResult=NRF_SUCCESS;
 memset(candidateAddrRaw,0,6);memset(lockedAddrRaw,0,6);}
void beginConnect(){connecting=true;Bluefruit.links[1]=true;connectCallback(1);}
int main(){
 reset();beginConnect();assert(!setupInProgress);assert(connected);assert(connectionHandle==1);
 assert(universalState==UniversalState::SEND_INIT);assert(hoboService.calls==1);
 assert(hoboCharacteristic.discoverCalls==1&&hoboCharacteristic.notifyCalls==1);
 assert(stateDueMs==bootstrapStartedMs+500);
 reset();hoboService.failures=2;beginConnect();assert(hoboService.calls==3);assert(universalState==UniversalState::SEND_INIT);
 for(unsigned stage=0;stage<3;stage++){
  reset();if(stage==0)hoboService.failures=3;
  if(stage==1)hoboCharacteristic.discoverFailures=3;
  if(stage==2)hoboCharacteristic.notifyFailures=3;
  beginConnect();assert(!setupInProgress);assert(recoveryPending);assert(universalState==UniversalState::IDLE);
  assert(Bluefruit.disconnects.size()==1);assert(haveRejectedAddr);
  if(stage==0)assert(strstr(lastBleFailure,"rsp=0 gatt=FFFF n=0"));
  disconnectCallback(1,0x13);assert(!connected&&!recoveryPending);
  clockMs=scanRetryDueMs;assert(restartScanner(clockMs));assert(Bluefruit.Scanner.starts==1);
 }
 reset();hoboService.drop=true;beginConnect();assert(hoboService.calls==1);assert(!setupInProgress);
 disconnectCallback(1,0x08);assert(!connected);assert(!recoveryPending);
 reset();beginConnect();disconnectCallback(2,0x13);assert(connected&&connectionHandle==1);
 disconnectCallback(1,0x13);assert(!connected);
 reset();connecting=false;Bluefruit.links[1]=true;connectCallback(1);assert(!connected);assert(Bluefruit.disconnects.size()==1);
 reset();connecting=true;triggerBleRecovery("connect timeout");assert(cancels==1&&!connecting);
 assert(scanRetryDueMs==5100);clockMs=scanRetryDueMs;assert(restartScanner(clockMs));
 reset();connecting=true;cancelResult=NRF_ERROR_INVALID_STATE;Bluefruit.links[1]=true;
 triggerBleRecovery("connect timeout");assert(!restartScanner(clockMs));assert(Bluefruit.Scanner.starts==0);
 connectCallback(1);assert(Bluefruit.disconnects.size()==1);
 reset();connecting=true;cancelResult=NRF_ERROR_INVALID_STATE;Bluefruit.Scanner.running=true;
 Bluefruit.Scanner.stopOk=false;Bluefruit.Scanner.resumeOk=false;
 triggerBleRecovery("connect timeout");clockMs=scanRetryDueMs;assert(lifecycleTick()==500);
 assert(Bluefruit.Scanner.starts==1);
 reset();connecting=true;cancelResult=123;triggerBleRecovery("cancel failure");assert(connecting);assert(!restartScanner(clockMs));
 reset();for(int i=0;i<300;i++){clockMs+=5000;triggerBleRecovery("RF unavailable");}
 assert(bleRecoveryCycles==255);assert(restartScanner(clockMs));
 reset();Bluefruit.Scanner.running=true;Bluefruit.Scanner.resumeOk=false;Bluefruit.Scanner.stopOk=false;
 resumeScanner();assert(scanRestartRequested);assert(restartScanner(clockMs));assert(!scanRestartRequested);
 reset();Bluefruit.Scanner.startOk=false;assert(!restartScanner(clockMs));assert(scanRetryDueMs==5100);
 reset();ble_evt_t event;event.header.evt_id=BLE_GAP_EVT_TIMEOUT;
 event.evt.gap_evt.params.timeout.src=BLE_GAP_TIMEOUT_SRC_CONN;bleEventCallback(&event);assert(connectTimedOut);
 reset();connectionHandle=1;event.header.evt_id=BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP;
 event.evt.common_evt.conn_handle=2;discoveryResponseSeen=false;bleEventCallback(&event);assert(!discoveryResponseSeen);
 event.evt.common_evt.conn_handle=1;bleEventCallback(&event);assert(discoveryResponseSeen&&discoveryGattStatus==0&&discoveryServiceCount==1);
 reset();uint8_t ad[]={3,0xFF,0xC5,0};ble_gap_evt_adv_report_t report;
 report.data.p_data=ad;report.data.len=sizeof(ad);report.peer_addr.addr_type=3;
 scanCallback(&report);assert(connecting);assert(Bluefruit.addressType==3);assert(Bluefruit.Scanner.stops==1);
 reset();Bluefruit.Scanner.running=true;Bluefruit.Scanner.stopOk=false;Bluefruit.Scanner.resumeOk=false;
 Bluefruit.connectOk=false;scanCallback(&report);assert(!connecting);assert(Bluefruit.Scanner.running);
 assert(lifecycleTick()==500);assert(Bluefruit.Scanner.starts==0);
 clockMs=scanRetryDueMs;assert(lifecycleTick()==500);assert(Bluefruit.Scanner.starts==1);
 reset();connecting=true;Bluefruit.Scanner.running=true;Bluefruit.Scanner.stopOk=false;
 Bluefruit.Scanner.resumeOk=false;triggerBleRecovery("connect timeout");assert(!connecting);
 clockMs=scanRetryDueMs;assert(lifecycleTick()==500);assert(Bluefruit.Scanner.starts==1);
 reset();report.type.connectable=false;scanCallback(&report);assert(!connecting);assert(Bluefruit.Scanner.resumes==1);
 reset();report.type.connectable=true;loggerLockEnabled=true;lockedAddrRaw[0]=1;scanCallback(&report);
 assert(!connecting&&Bluefruit.Scanner.resumes==1);
 uint8_t malformed[]={31,0xFF,0xC5,0};report.data.p_data=malformed;assert(!inspectAdvertisement(&report).candidate);
 reset();Bluefruit.links[1]=true;Bluefruit.phones[5]=true;NRF52Bluetooth phone;phone.disconnect();
 assert(Bluefruit.links[1]&&!Bluefruit.phones[5]);assert(Bluefruit.disconnects.size()==1&&Bluefruit.disconnects[0]==5);
 reset();Bluefruit.links[1]=true;phone.disconnect();assert(Bluefruit.links[1]&&Bluefruit.disconnects.empty());
 reset();for(auto type:{LoggerType::MX2001,LoggerType::MX2201,LoggerType::MX2203}){
 connected=true;loggerType=type;assert(strstr(loggerSummary(),loggerTypeName(type)));}
 connected=false;loggerType=LoggerType::UNKNOWN;lastVerifiedType=LoggerType::MX2001;
 assert(strstr(loggerSummary(),"Model: Not identified"));assert(strstr(loggerSummary(),"Last verified: MX2001"));
 clockMs=10;rejectedUntilMs=20;haveRejectedAddr=true;assert(isRejectedCandidate(candidateAddrRaw));
 clockMs=20;assert(!isRejectedCandidate(candidateAddrRaw));
 assert(reached(20,0xFFFFFFF0U));assert(!reached(0xFFFFFFF0U,20));
 puts("PASS: callback setup, three discovery failure stages, dropped/stale links, cancel races, scanning, address types, sparse phone handles, models, rollover");
}
"""
# A reset is not an acceptable outcome of these ordinary BLE faults.
assert "nrf52FieldWatchdogTrip" not in source
assert "UniversalState::DISCOVER_SERVICE" not in source
assert "scanRestartRequested || !Bluefruit.Scanner.isRunning()" in source
assert "Bluefruit.Scanner.setInterval(320, 32)" in source
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / "faults.cpp"
    cpp.write_text(
        prelude
        + globals_text
        + uuid_text
        + candidate_struct
        + methods
        + lifecycle
        + helpers
        + tests
    )
    binary = Path(directory) / "faults"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-fsanitize=address,undefined",
            str(cpp),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
