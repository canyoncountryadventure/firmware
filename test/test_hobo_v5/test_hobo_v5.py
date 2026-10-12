"""Fault-injection checks compiled from the firmware's real recovery/LOGGER code."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp').read_text()

def block(signature):
    start = re.search(re.escape(signature) + r"\s*\{", source).start()
    opening = source.index('{', start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]

logger_start = source.index('char reply[220]', source.index('"LOGGER"'))
logger_end = source.index('sendTextReply(mp.from', logger_start)
logger_body = source[logger_start:logger_end]
stage_start = source.index('case UniversalState::DISCOVER_SERVICE:', source.index('int32_t HOBOMX'))
stage_end = source.index('case UniversalState::SEND_INIT:', stage_start)
discovery_body = source[stage_start:stage_end]

prelude = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
constexpr uint16_t BLE_CONN_HANDLE_INVALID=65535;
constexpr uint32_t NRF_SUCCESS=0,NRF_ERROR_INVALID_STATE=8;
constexpr uint32_t SCAN_RETRY_MS=5000,RECOVERY_MIN_FAILURE_MS=120000;
constexpr unsigned SERVICE_DISCOVERY_ATTEMPTS=3,SERVICE_RETRY_DELAY_MS=350,COMMAND_DELAY_MS=400;
uint32_t clockMs=100; uint32_t millis(){return clockMs;}
namespace Throttle {bool isWithinTimespanMs(uint32_t last,uint32_t duration){return uint32_t(clockMs-last)<duration;}}
enum class LoggerType:uint8_t {UNKNOWN,MX2001,MX2201,MX2203,UNSUPPORTED};
enum class UniversalState:uint8_t {IDLE,DISCOVER_SERVICE,DISCOVER_CHARACTERISTIC,ENABLE_NOTIFY,SEND_INIT};
bool connecting=false,connected=false,recoveryPending=false,watchdogRecoveryPending=false;
bool scanRestartRequested=false,statusTrackingAvailable=true,discoveryResponseSeen=false;
bool loggerLockEnabled=false; uint8_t loggerMac[6]={},lastVerifiedAddr[6]={};
LoggerType loggerType=LoggerType::UNKNOWN,lastVerifiedType=LoggerType::UNKNOWN;
int loggerBleRssi=-70; unsigned loggerIntervalSeconds=3600;
uint16_t discoveryGattStatus=0xFFFF,discoveryServiceCount=0,connectionHandle=BLE_CONN_HANDLE_INVALID;
uint8_t bleRecoveryCycles=0,discoveryAttempts=0;
uint32_t firstBleFailureMs=0,disconnectDueMs=0,connectAttemptStartedMs=0;
uint32_t lastScanKickMs=0,scanRetryDueMs=0,stateDueMs=0,bootstrapStartedMs=0;
char lastBleFailure[48]="none";
UniversalState universalState=UniversalState::IDLE;
unsigned watchdogTrips=0;void nrf52FieldWatchdogTrip(){watchdogTrips++;}
void nrf52FieldDiagEvent(int,int,int){}
uint32_t cancelResult=NRF_SUCCESS;uint32_t sd_ble_gap_connect_cancel(){return cancelResult;}
struct Scan {bool running=true,startOk=true,resumeOk=true;unsigned starts=0;
 bool stop(){return false;} bool start(int){starts++;if(startOk)running=true;return startOk;}
 bool resume(){return resumeOk;}};
struct BF {Scan Scanner;struct {unsigned connected(){return 1;}} Central;
 bool disconnect(uint16_t){return true;}} Bluefruit;
struct Svc {bool ok=false;bool discover(uint16_t){return ok;}} hoboService;
struct Chr {bool ok=true;bool discover(){return ok;}bool enableNotify(){return ok;}} hoboCharacteristic;
void rejectCurrentCandidate(uint32_t){}
constexpr uint32_t TRANSIENT_RETRY_MS=5000;
bool reached(uint32_t now,uint32_t due){return int32_t(now-due)>=0;}
void makeHumanMac(const uint8_t* raw,uint8_t* human){for(int i=0;i<6;i++)human[i]=raw[5-i];}
const char* loggerTypeName(LoggerType t){switch(t){case LoggerType::MX2001:return "MX2001";
case LoggerType::MX2201:return "MX2201";case LoggerType::MX2203:return "MX2203";default:return "UNKNOWN";}}
void triggerBleRecovery(const char*);
'''
methods = '\n'.join(block(name) for name in ['void resumeScanner()', 'bool restartScanner(uint32_t now)', 'void triggerBleRecovery(const char *reason)'])
helpers = '\nvoid discoveryTick(){uint32_t now=millis();switch(universalState){\n'+discovery_body+'default:break;}}\n'
helpers += '\nconst char* loggerSummary(){static char output[220];\n'+logger_body+'snprintf(output,sizeof(output),"%s",reply);return output;}\n'
tests = r'''
void reset(){connecting=false;connected=false;recoveryPending=false;watchdogRecoveryPending=false;
bleRecoveryCycles=0;firstBleFailureMs=0;watchdogTrips=0;scanRestartRequested=false;clockMs=100;
Bluefruit.Scanner=Scan{};connectionHandle=1;universalState=UniversalState::IDLE;}
int main(){
 reset();Bluefruit.Scanner.resumeOk=false;resumeScanner();assert(scanRestartRequested);
 assert(Bluefruit.Scanner.running);assert(restartScanner(clockMs));assert(!scanRestartRequested);assert(Bluefruit.Scanner.starts==1);
 reset();Bluefruit.Scanner.startOk=false;assert(!restartScanner(clockMs));assert(bleRecoveryCycles==1);assert(scanRetryDueMs==5100);
 reset();for(int i=0;i<4;i++){clockMs=100+i*10000;triggerBleRecovery("service discovery");}
 assert(watchdogTrips==0);clockMs=120100;triggerBleRecovery("service discovery");assert(watchdogTrips==1);
 reset();connecting=true;connectAttemptStartedMs=100;cancelResult=123;triggerBleRecovery("connect timeout");assert(watchdogTrips==1);
 reset();connected=true;universalState=UniversalState::DISCOVER_SERVICE;discoveryAttempts=0;stateDueMs=0;hoboService.ok=false;
 for(int i=0;i<3;i++){clockMs=1000+i*500;discoveryTick();}
 assert(bleRecoveryCycles==1);assert(recoveryPending);assert(strstr(lastBleFailure,"service:")!=nullptr);
 reset();connected=true;universalState=UniversalState::DISCOVER_SERVICE;discoveryAttempts=0;stateDueMs=0;hoboService.ok=true;
 for(int i=0;i<3;i++){clockMs=1000+i*500;discoveryTick();}
 assert(universalState==UniversalState::SEND_INIT);assert(bootstrapStartedMs>0);assert(watchdogTrips==0);
 reset();for(auto type:{LoggerType::MX2001,LoggerType::MX2201,LoggerType::MX2203}){
 connected=true;loggerType=type;const char* reply=loggerSummary();assert(strstr(reply,loggerTypeName(type)));assert(strstr(reply,"CONNECTED"));assert(strlen(reply)<219);}
 connected=false;loggerType=LoggerType::UNKNOWN;lastVerifiedType=LoggerType::MX2001;
 assert(strstr(loggerSummary(),"Model: Not identified"));assert(strstr(loggerSummary(),"Last verified: MX2001"));
 lastVerifiedType=LoggerType::UNKNOWN;assert(strstr(loggerSummary(),"Model: Not identified"));assert(!strstr(loggerSummary(),"Model: MX2001"));
 puts("PASS: stale scanner, failed starts/cancel, discovery failure escalation, successful setup, and truthful LOGGER models");
}
'''
# initializer_list is required for the model-family loop.
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory)/'faults.cpp'
    cpp.write_text('#include <initializer_list>\n'+prelude+methods+helpers+tests)
    binary = Path(directory)/'faults'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
