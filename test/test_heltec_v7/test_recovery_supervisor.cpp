#include "src/modules/CcaGatewaySupervisor.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

using R = CcaGatewayRecovery;
static uint32_t m(unsigned n) { return n * 60000UL; }

static void healthy() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    assert(g.update(1000,s)==R::None);
    for (uint32_t i=1; i<=240; ++i) {
        s.gatewayCycles++;
        s.dmRequests = i;
        s.rxGood++;
        s.txDoneIrq++;
        s.httpAttempts++;
        s.httpAccepted++;
        s.wifiConnected = true;
        s.lastHttpStatus = 201;
        assert(g.update(m(i)+1000,s)==R::None);
    }
    assert(g.provenHealthy());
}
static void moduleFreeze() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    g.update(1000,s);
    s.gatewayCycles=1;
    g.update(m(1)+1000,s);
    assert(g.update(m(6)+1000,s)==R::ModuleStalled);
}
static void noRadioActivity() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(1000,s);
    for(uint32_t i=1;i<=60;i++){
        s.gatewayCycles++;
        s.dmRequests=i/2+1;
        R result=g.update(m(i)+1000,s);
        if(i<60) assert(result==R::None);
        else assert(result==R::NoRxOrTx);
    }
}
static void rxSilenceWithTxWorking() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(1000,s);
    for(uint32_t i=1;i<=180;i++){
        s.gatewayCycles++;
        if(i%30==0){s.dmRequests++;s.txDoneIrq++;}
        auto result=g.update(m(i)+1000,s);
        if(i==90) assert(result==R::SoftRecoverRx);
        else if(i==180) assert(result==R::RxSilentThreeHours);
        else assert(result==R::None);
    }
}
static void freshRxSuppressesThreeHourRecovery() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(1000,s);
    for(uint32_t i=1;i<=210;i++){
        s.gatewayCycles++;
        if(i%30==0){s.dmRequests++;s.txDoneIrq++;s.rxGood++;}
        assert(g.update(m(i)+1000,s)==R::None);
    }
}
static void queueWithoutPost() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(1000,s);
    for(uint32_t i=1;i<=12;i++){
        s.gatewayCycles++;
        if(i>=1) s.queueUsed=4;
        auto result=g.update(m(i)+1000,s);
        if(i==11) assert(result==R::UploadStalled);
        else if(i<11) assert(result==R::None);
        if(i==11) break;
    }
}
static void authorizedFailureDoesNotCauseReboot() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(1000,s);
    for(uint32_t i=1;i<=65;i++){
        s.gatewayCycles++;
        s.rxGood++;
        s.txDoneIrq++;
        s.dmRequests++;
        s.httpAttempts++;
        s.lastHttpStatus=401;
        assert(g.update(m(i)+1000,s)==R::None);
    }
}
static void networkDownDoesNotBlockRfPoll() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=false;
    g.update(1000,s);
    for(uint32_t i=1;i<=61;i++){
        s.gatewayCycles++;
        s.rxGood++;
        s.txDoneIrq++;
        s.dmRequests++;
        auto action=g.update(m(i)+1000,s);
        if(i<61) assert(action==R::None);
        else assert(action==R::WifiStalled);
    }
}
static void monotonicWrap() {
    CcaGatewaySupervisor g;
    CcaGatewaySnapshot s;
    s.wifiConnected=true;
    g.update(UINT32_MAX-1000,s);
    s.gatewayCycles=1;
    // Preserve RX silence across the 32-bit millisecond counter wrap.
    s.txDoneIrq=1;
    assert(g.update(4000,s)==R::None);
    assert(g.rxAgeMs(4000)==5001);
}
int main(){
    healthy(); moduleFreeze(); noRadioActivity(); rxSilenceWithTxWorking();
    freshRxSuppressesThreeHourRecovery(); queueWithoutPost();
    authorizedFailureDoesNotCauseReboot(); networkDownDoesNotBlockRfPoll();
    monotonicWrap();
    puts("PASS: V7 gateway watchdog scenarios");
}
