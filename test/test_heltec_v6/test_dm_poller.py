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
    m=re.search(r'(?m)^(?:bool|void) HoboHttpGatewayModule::'+name+r'\([^\n]*\)\s*\{',src)
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
struct String:std::string {
 using std::string::string;
 void toUpperCase(){std::transform(begin(),end(),begin(),[](unsigned char c){return std::toupper(c);});}
 int indexOf(const char *p) const {auto i=find(p);return i==npos?-1:static_cast<int>(i);}
 bool startsWith(const char*p)const{return rfind(p,0)==0;}
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
void HoboHttpGatewayModule::persistCc01Settings(){}
bool HoboHttpGatewayModule::sendCc01Dm(const char*cmd){cc01Requests++;requests.emplace_back(cmd);return true;}
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
code=preamble+body+'\n'+impl+'\n'+extract('pollCc01')+'\n'+extract('handleCc01Reply')
# Move main() after method definitions, otherwise methods are only declared but linking is fine.
with tempfile.TemporaryDirectory() as folder:
 f=Path(folder)/'test.cpp';f.write_text(code)
 exe=Path(folder)/'test'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(root),str(f),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: V7 ordered poll, 3 x 30s retries, explicit failure, pause, 15-minute mode')
