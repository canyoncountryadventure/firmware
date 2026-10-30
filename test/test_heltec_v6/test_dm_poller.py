"""Compile actual CC01 polling methods against host fakes and verify bounded behavior."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
header=(root/'src/modules/HoboHttpGatewayModule.h').read_text()
body=header[header.index('class HoboHttpGatewayModule'):header.index('extern HoboHttpGatewayModule')]
body=re.sub(r'class HoboHttpGatewayModule\s*:.*?\{','class HoboHttpGatewayModule {',body,count=1,flags=re.S)
body=body.replace('private:','public:').replace('protected:','public:').replace(' override','')
body=body.replace('HoboHttpGatewayModule();','HoboHttpGatewayModule():uploadQueue(32){}\n void setIntervalFromNow(unsigned long){}')
body=body.replace('    bool isDuplicate(', '    std::vector<std::string> requests;\n    std::vector<std::string> failures;\n    bool isDuplicate(')
src=(root/'src/modules/CcaCc01DmPoller.cpp').read_text()
def extract(name):
    m=re.search(r'(?m)^(?:bool|void) HoboHttpGatewayModule::'+name+r'\([^\n]*\)(?:\s+const)?\s*\{',src)
    assert m,name
    depth=0
    for i in range(m.end()-1,len(src)):
        if src[i]=='{':depth+=1
        if src[i]=='}':
            depth-=1
            if not depth:return src[m.start():i+1]
    raise AssertionError(name)
preamble=r"""
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
enum class ProcessMessage { CONTINUE };
#define LOG_INFO(...) do {} while(0)
#define LOG_WARN(...) do {} while(0)
uint32_t fakeTime=0;
uint32_t millis(){return fakeTime;}
struct Throttle { static bool isWithinTimespanMs(uint32_t then,uint32_t interval){return uint32_t(fakeTime-then)<interval;} };
struct String:std::string {
 using std::string::string;
 String(unsigned long value):std::string(std::to_string(value)){}
 String(unsigned int value):std::string(std::to_string(value)){}
 String(int value):std::string(std::to_string(value)){}
 String(const std::string& value):std::string(value){}
 void trim(){}
 void toUpperCase(){std::transform(begin(),end(),begin(),[](unsigned char c){return std::toupper(c);});}
 int indexOf(const char *p) const {auto i=find(p);return i==npos?-1:static_cast<int>(i);}
 bool startsWith(const char*p)const{return rfind(p,0)==0;}
 int lastIndexOf(char c) const {auto i=rfind(c);return i==npos?-1:static_cast<int>(i);}
 String substring(size_t start) const{return String(substr(start));}
 String substring(size_t start,size_t end) const{return String(substr(start,end-start));}
};
template<class T>struct TypedQueue{
 std::vector<T> jobs;bool full=false;TypedQueue(int){}
 bool enqueue(T job,int){if(full)return false;jobs.push_back(job);return true;}
 unsigned numUsed(){return jobs.size();}
};
struct meshtastic_MeshPacket{
 uint32_t from=1695962694,to=2740603892,id=1;
 struct{struct{uint8_t bytes[241]={};size_t size=0;}payload;}decoded;
};
#include "src/modules/CcaCc01DmParser.h"
"""
impl=r"""
void HoboHttpGatewayModule::fillCommon(UploadJob&j,const meshtastic_MeshPacket&m){j.packetId=m.id;j.from=m.from;}
constexpr uint32_t MATHESON_POLL_NODE=2004386937UL;
constexpr uint32_t CC01_POLL_NODE=1695962694UL;
constexpr uint32_t MILL_CREEK_POLL_NODE=1683604927UL;
constexpr uint32_t PACK_CREEK_POLL_NODE=1949224949UL;
uint32_t getFrom(const meshtastic_MeshPacket*p){return p->from;}
bool validPollIntervalMinutes(uint32_t m){return m==5||m==10||m==15||m==20||m==30||m==60||m==120||m==180||m==360||m==720||m==1440;}
uint32_t pollNodeForName(const String&n){if(n=="CC01"||n=="COTTONWOOD"||n=="COTTONWOOD CREEK")return CC01_POLL_NODE;if(n=="MATHESON")return MATHESON_POLL_NODE;if(n=="MILL CREEK"||n=="MILL")return MILL_CREEK_POLL_NODE;if(n=="PACK CREEK"||n=="PACK")return PACK_CREEK_POLL_NODE;char*e=nullptr;auto v=strtoul(n.c_str(),&e,10);return e!=n.c_str()&&!*e?v:0;}
String popPollToken(String&v){v.trim();int i=v.lastIndexOf(' ');if(i<0){String t=v;v="";return t;}String t=v.substring(i+1);v=v.substring(0,i);v.trim();return t;}
void HoboHttpGatewayModule::persistCc01Settings(){}
bool HoboHttpGatewayModule::sendCc01Text(uint32_t to,const char*cmd){if(to==1695962694UL)++cc01Requests;if(to==2004386937UL)++mathesonRequests;requests.emplace_back(cmd);return true;}
bool HoboHttpGatewayModule::sendCc01Dm(const char*cmd){return sendCc01Text(1695962694UL,cmd);}
void HoboHttpGatewayModule::queueCc01SensorStatus(const meshtastic_MeshPacket&,const char*s,const char*r){failures.push_back(std::string(s)+":"+r);}
meshtastic_MeshPacket packet(const char*s){meshtastic_MeshPacket p;p.decoded.payload.size=strlen(s);memcpy(p.decoded.payload.bytes,s,strlen(s));return p;}
int main(){
 auto water=packet("WATER READ\nRaw: 8.865 ft (2702mm)\nStage: 0.577 ft (17.6 cm)\nSamples:10 Spread:1mm");
 auto hobo=packet("MX2001\nLogger: E3:1A:98:DC:FB:C2\nBLE: -76 dBm\nLevel: 0.54 ft\nTemp: 49.4 F");
 HoboHttpGatewayModule g;fakeTime=120000;g.pollCc01();
 assert(g.cc01WaterAttempts==1&&g.requests.back()=="WATER READ");
 fakeTime=149999;g.pollCc01();assert(g.requests.size()==1);
 fakeTime=150000;g.pollCc01();assert(g.cc01WaterAttempts==2);
 fakeTime=180000;g.pollCc01();assert(g.cc01WaterAttempts==3);
 fakeTime=210000;g.pollCc01();assert(g.cc01Phase==HoboHttpGatewayModule::Cc01Phase::Hobo);
 fakeTime=210001;g.pollCc01();assert(g.requests.back()=="READ");
 fakeTime=240001;g.pollCc01();fakeTime=270001;g.pollCc01();fakeTime=300001;g.pollCc01();
 assert(g.cc01HoboAttempts==3&&g.requests.size()==6);
 HoboHttpGatewayModule good;fakeTime=120000;good.pollCc01();good.handleCc01Reply(water);
 fakeTime=123000;good.pollCc01();assert(good.requests.back()=="READ");
 good.handleCc01Reply(hobo);assert(good.cc01Phase==HoboHttpGatewayModule::Cc01Phase::Done);
 good.pollMatheson();assert(good.mathesonRequests==0);
 fakeTime=126000;good.pollMatheson();assert(good.mathesonRequests==1&&good.requests.back()=="READ");
 auto matheson=hobo;matheson.from=MATHESON_POLL_NODE;good.handleMathesonReply(matheson);
 assert(good.mathesonQueued==1&&good.mathesonPhase==HoboHttpGatewayModule::MathesonPhase::Done);
 fakeTime+=60*60000;good.pollMatheson();assert(good.mathesonRequests==2&&good.requests.back()=="READ");
 HoboHttpGatewayModule generic;generic.cc01PollStarted=true;generic.cc01Phase=HoboHttpGatewayModule::Cc01Phase::Done;generic.mathesonPhase=HoboHttpGatewayModule::MathesonPhase::Done;generic.configuredPolls[0].node=MILL_CREEK_POLL_NODE;generic.configuredPolls[0].intervalMinutes=60;fakeTime=600000;generic.pollConfiguredStations();assert(generic.requests.back()=="READ");
 auto mill=hobo;mill.from=MILL_CREEK_POLL_NODE;generic.handleConfiguredPollReply(mill);assert(generic.configuredPollQueued==1&&generic.configuredPolls[0].phase==HoboHttpGatewayModule::ConfiguredPollPhase::Done);
 HoboHttpGatewayModule control;control.cc01AdminNode=99;auto admin=packet("POLL MATHESON 30");admin.from=99;control.handleCc01ControlDm(admin);
 assert(control.mathesonIntervalMinutes==30);admin=packet("POLL COTTONWOOD 20");admin.from=99;control.handleCc01ControlDm(admin);
 assert(control.cc01IntervalOverrideMinutes==20);admin=packet("POLL COTTONWOOD AUTO");admin.from=99;control.handleCc01ControlDm(admin);assert(control.cc01IntervalOverrideMinutes==0);
 admin=packet("POLL MILL CREEK 60 BOTH");admin.from=99;control.handleCc01ControlDm(admin);assert(control.configuredPolls[0].node==MILL_CREEK_POLL_NODE&&control.configuredPolls[0].intervalMinutes==60&&control.configuredPolls[0].mode==HoboHttpGatewayModule::ConfiguredPollMode::Both);
 admin=packet("POLL 1683604927");admin.from=99;control.handleCc01ControlDm(admin);assert(control.configuredPolls[HoboHttpGatewayModule::CONFIGURED_POLL_PERSISTENT_SLOTS].oneShot);
 HoboHttpGatewayModule down;fakeTime=120000;down.pollCc01();down.handleCc01Reply(water);
 fakeTime=123000;down.pollCc01();down.handleCc01Reply(packet("HOBO unavailable"));
 assert(down.failures.back()=="hobo:ble_unavailable"&&down.cc01Phase==HoboHttpGatewayModule::Cc01Phase::Done);
 HoboHttpGatewayModule failed;fakeTime=120000;failed.pollCc01();failed.handleCc01Reply(packet("READ failed: TIMEOUT"));
 assert(failed.failures.back()=="water:timeout"&&failed.cc01WaterPending);
 HoboHttpGatewayModule paused;paused.cc01WaterEnabled=false;fakeTime=120000;paused.pollCc01();
 assert(paused.requests.back()=="READ");
 HoboHttpGatewayModule fast;fast.cc01FastMode=true;fakeTime=120000;fast.pollCc01();
 fakeTime=120000+15*60000;fast.pollCc01();assert(fast.cc01WaterAttempts==1);
}
"""
code=preamble+body+'\n'+impl+'\n'+extract('pollCc01')+'\n'+extract('pollMatheson')+'\n'+extract('isConfiguredPollReply')+'\n'+extract('pollConfiguredStations')+'\n'+extract('handleCc01Reply')+'\n'+extract('handleMathesonReply')+'\n'+extract('handleConfiguredPollReply')+'\n'+extract('handleCc01ControlDm')
# Move main() after method definitions, otherwise methods are only declared but linking is fine.
with tempfile.TemporaryDirectory() as folder:
 f=Path(folder)/'test.cpp';f.write_text(code)
 exe=Path(folder)/'test'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(root),str(f),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: V7 ordered Cottonwood/Matheson polling plus bounded, authenticated generic radio polling')
