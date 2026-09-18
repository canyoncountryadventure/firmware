#include "configuration.h"
#if defined(ARCH_NRF52) && defined(RAK_4631)
#include "RAKRemoteDfuModule.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "SPILock.h"
#include "main.h"
#include <nrf.h>
#include <nrf_sdm.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
#define DFU_STRINGIFY_INNER(x) #x
#define DFU_STRINGIFY(x) DFU_STRINGIFY_INNER(x)
static constexpr size_t BUILD_ID_MAX=20;
static constexpr char CURRENT_BUILD[]=DFU_STRINGIFY(APP_VERSION);
static constexpr char PENDING_FILE[]="/prefs/dfu_pending.bin";
static constexpr uint32_t REBOOT_DELAY_MS=3000UL;
static constexpr uint32_t CONFIRM_DELAY_MS=5000UL;
static constexpr uint32_t CONFIRM_RETRY_MS=30000UL;
bool rebootPending=false;
uint32_t rebootAtMs=0;
bool confirmPending=false;
uint32_t confirmDestination=0;
uint8_t confirmChannel=0;
uint32_t confirmDueMs=0;
char previousBuild[BUILD_ID_MAX+1]={};

bool reached(uint32_t now,uint32_t target){return static_cast<int32_t>(now-target)>=0;}
uint8_t checksum(const uint8_t *data,size_t length){uint8_t v=0;for(size_t i=0;i<length;++i)v^=data[i];return v;}

bool isCommand(const uint8_t *bytes,size_t size,const char *expected)
{
    if(!bytes||!size||!expected)return false;
    char command[32]={};
    size_t n=size<sizeof(command)?size:sizeof(command)-1;
    memcpy(command,bytes,n);
    char *p=command;
    while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;
    if(*p=='/')++p;
    size_t len=strlen(p);
    while(len&&(p[len-1]==' '||p[len-1]=='\t'||p[len-1]=='\r'||p[len-1]=='\n'))p[--len]='\0';
    const size_t want=strlen(expected);
    if(len!=want)return false;
    for(size_t i=0;i<want;++i)
        if(std::toupper(static_cast<unsigned char>(p[i]))!=std::toupper(static_cast<unsigned char>(expected[i])))return false;
    return true;
}

void clearPending()
{
    confirmPending=false;confirmDestination=0;confirmChannel=0;confirmDueMs=0;memset(previousBuild,0,sizeof(previousBuild));
    concurrency::LockGuard g(spiLock);
    FSCom.remove(PENDING_FILE);
}

bool savePending(uint32_t destination,uint8_t channel)
{
    if(destination==0)return false;
    uint8_t record[32]={'D','F','U','2',2,0,0,0,0,channel,0};
    record[5]=static_cast<uint8_t>(destination);
    record[6]=static_cast<uint8_t>(destination>>8);
    record[7]=static_cast<uint8_t>(destination>>16);
    record[8]=static_cast<uint8_t>(destination>>24);
    size_t buildLength=strlen(CURRENT_BUILD);
    if(buildLength>BUILD_ID_MAX)buildLength=BUILD_ID_MAX;
    record[10]=static_cast<uint8_t>(buildLength);
    memcpy(&record[11],CURRENT_BUILD,buildLength);
    record[31]=checksum(record,31);
    size_t written=0;
    {
        concurrency::LockGuard g(spiLock);
        File file=FSCom.open(PENDING_FILE,FILE_O_WRITE);
        if(!file){LOG_WARN("RAK DFU: open marker for write failed");return false;}
        if(!file.seek(0)||!file.truncate()){LOG_WARN("RAK DFU: truncate marker failed");file.close();return false;}
        written=file.write(record,sizeof(record));file.flush();file.close();
    }
    if(written!=sizeof(record)){LOG_WARN("RAK DFU: marker short write");return false;}
    uint8_t verify[sizeof(record)]={};size_t verified=0;bool extra=false;
    {
        concurrency::LockGuard g(spiLock);
        File file=FSCom.open(PENDING_FILE,FILE_O_READ);
        if(!file){LOG_WARN("RAK DFU: marker reopen failed");return false;}
        verified=file.read(verify,sizeof(verify));extra=file.available();file.close();
    }
    if(extra||verified!=sizeof(record)||memcmp(record,verify,sizeof(record))!=0){
        LOG_WARN("RAK DFU: marker read-back verification failed");return false;
    }
    confirmDestination=destination;confirmChannel=channel;
    memset(previousBuild,0,sizeof(previousBuild));memcpy(previousBuild,CURRENT_BUILD,buildLength);
    return true;
}

void loadPending()
{
    uint8_t record[32]={};size_t got=0;bool extra=false;
    {
        concurrency::LockGuard g(spiLock);
        File file=FSCom.open(PENDING_FILE,FILE_O_READ);
        if(!file)return;
        got=file.read(record,sizeof(record));extra=file.available();file.close();
    }
    if(extra||got!=sizeof(record)||record[0]!='D'||record[1]!='F'||record[2]!='U'||record[3]!='2'||record[4]!=2||
       record[10]==0||record[10]>BUILD_ID_MAX||record[31]!=checksum(record,31)){
        LOG_WARN("RAK DFU: invalid pending marker; clearing");clearPending();return;
    }
    const uint32_t destination=static_cast<uint32_t>(record[5])|(static_cast<uint32_t>(record[6])<<8)|
        (static_cast<uint32_t>(record[7])<<16)|(static_cast<uint32_t>(record[8])<<24);
    if(destination==0){clearPending();return;}
    confirmDestination=destination;confirmChannel=record[9];
    memcpy(previousBuild,&record[11],record[10]);previousBuild[record[10]]='\0';
    confirmPending=true;confirmDueMs=millis()+CONFIRM_DELAY_MS;
    LOG_INFO("RAK DFU: callback pending to=0x%08lX old=%s new=%s",static_cast<unsigned long>(confirmDestination),previousBuild,CURRENT_BUILD);
}
}

RAKRemoteDfuModule::RAKRemoteDfuModule()
    : SinglePortModule("rak_remote_dfu",meshtastic_PortNum_TEXT_MESSAGE_APP),concurrency::OSThread("rak_remote_dfu")
{
    isPromiscuous=true;loadPending();setIntervalFromNow(1000);
}

bool RAKRemoteDfuModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p&&p->decoded.portnum==meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage RAKRemoteDfuModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if(!nodeDB||mp.decoded.portnum!=meshtastic_PortNum_TEXT_MESSAGE_APP)return ProcessMessage::CONTINUE;
    const uint32_t ourNode=nodeDB->getNodeNum();
    if(mp.to!=ourNode||mp.from==ourNode)return ProcessMessage::CONTINUE;
    if(!isCommand(mp.decoded.payload.bytes,mp.decoded.payload.size,"DFU"))return ProcessMessage::CONTINUE;
    if(rebootPending){sendTextReply(mp.from,mp.channel,"BLE OTA DFU already armed");return ProcessMessage::CONTINUE;}
    if(!savePending(mp.from,mp.channel)){
        sendTextReply(mp.from,mp.channel,"BLE OTA DFU NOT armed\nCould not verify callback marker");
        LOG_WARN("RAK DFU: refusing reboot because callback marker was not verified");
        return ProcessMessage::CONTINUE;
    }
    sendTextReply(mp.from,mp.channel,"BLE OTA DFU armed\nRebooting into AdaDFU in 3 seconds\nMatched drone Scout can flash this target");
    rebootPending=true;rebootAtMs=millis()+REBOOT_DELAY_MS;setIntervalFromNow(10);
    LOG_WARN("RAK DFU: armed by mesh DM from=0x%08lX channel=%u",static_cast<unsigned long>(mp.from),mp.channel);
    return ProcessMessage::CONTINUE;
}

bool RAKRemoteDfuModule::sendTextReply(uint32_t destination,uint8_t channel,const char *text)
{
    if(!destination||!text)return false;
    meshtastic_MeshPacket *packet=allocDataPacket();if(!packet)return false;
    size_t len=strlen(text);if(len>sizeof(packet->decoded.payload.bytes))len=sizeof(packet->decoded.payload.bytes);
    memcpy(packet->decoded.payload.bytes,text,len);packet->decoded.payload.size=len;
    packet->decoded.portnum=meshtastic_PortNum_TEXT_MESSAGE_APP;packet->decoded.want_response=false;
    packet->to=destination;packet->channel=channel;packet->want_ack=true;packet->priority=meshtastic_MeshPacket_Priority_RELIABLE;
    service->sendToMesh(packet,RX_SRC_LOCAL,true);return true;
}

int32_t RAKRemoteDfuModule::runOnce()
{
    const uint32_t now=millis();
    if(rebootPending&&reached(now,rebootAtMs)){
        rebootPending=false;LOG_WARN("RAK DFU: entering AdaDFU (GPREGRET=0xA8)");delay(100);nrf52FlashQuiesce();
        uint8_t enabled=0;if(sd_softdevice_is_enabled(&enabled)==NRF_SUCCESS&&enabled)(void)sd_softdevice_disable();
        NRF_POWER->GPREGRET=0xA8;__DSB();NVIC_SystemReset();while(true)delay(1000);
    }
    if(confirmPending&&confirmDestination&&reached(now,confirmDueMs)){
        const bool changed=previousBuild[0]!='\0'&&strcmp(previousBuild,CURRENT_BUILD)!=0;
        char reply[220]={};
        if(changed)snprintf(reply,sizeof(reply),"UPDATE SUCCESS\nRAK4631 booted new firmware after drone/BLE DFU\nOld:%s\nNew:%s",previousBuild,CURRENT_BUILD);
        else snprintf(reply,sizeof(reply),"DFU RESULT: BUILD UNCHANGED\nTarget rebooted successfully\nBuild:%s",CURRENT_BUILD);
        if(sendTextReply(confirmDestination,confirmChannel,reply)){LOG_INFO("RAK DFU: post-update result queued");clearPending();}
        else{confirmDueMs=now+CONFIRM_RETRY_MS;LOG_WARN("RAK DFU: callback enqueue failed; retry scheduled");}
    }
    if(rebootPending)return 10;
    if(confirmPending)return 1000;
    return 30000;
}
#endif
