#include "src/modules/CcaGatewaySupervisor.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
using R = CcaGatewayRecovery;
static uint32_t minutes(uint32_t n){ return n * 60000UL; }
int main() {
    CcaGatewaySupervisor s;
    CcaGatewaySnapshot x{};
    x.wifiConnected=true;
    assert(s.update(1000,x)==R::None);
    // A success from boot must not count as current network health at hour 1.
    x.rxGood++;x.txDoneIrq++;x.gatewayCycles++;x.httpAttempts++;x.httpAccepted++;
    x.lastHttpStatus=201;
    assert(s.update(minutes(1)+1000,x)==R::None);
    assert(s.provenHealthy());
    assert(s.recentlyHealthy(minutes(1)+1000, minutes(5)));
    for(uint32_t i=2;i<=60;i++){x.rxGood++;x.gatewayCycles++;x.txDoneIrq++;assert(s.update(minutes(i)+1000,x)==R::None);}
    assert(s.provenHealthy());
    assert(!s.recentlyHealthy(minutes(60)+1000, minutes(5)));
    // Renewed, accepted upload is required before a recovery lockout re-arms.
    x.httpAttempts++;x.httpAccepted++;x.lastHttpStatus=201;
    assert(s.update(minutes(60)+6000,x)==R::None);
    assert(s.recentlyHealthy(minutes(60)+6000, minutes(5)));
    // Wi-Fi can fail for an hour while mesh RX/TX continues; supervisor must
    // identify network failure for the CALLER to handle without MCU reboot.
    x.wifiConnected=false;
    assert(s.update(minutes(61)+1000,x)==R::None);
    for(uint32_t i=62;i<=122;i++){
        x.rxGood++;x.txDoneIrq++;x.gatewayCycles++;
        const R got=s.update(minutes(i)+1000,x);
        if(i>=121) assert(got==R::WifiStalled);
        else assert(got==R::None);
    }
    // Active RF plus repeated negative HTTP transport failures can signal
    // UploadStalled, but the firmware handles this via Wi-Fi-only repair.
    CcaGatewaySupervisor h;
    CcaGatewaySnapshot y{};
    y.wifiConnected=true;
    h.update(1000,y);
    for(uint32_t i=1;i<=17;i++){
        y.rxGood++;y.txDoneIrq++;y.gatewayCycles++;y.httpAttempts++;
        y.lastHttpStatus=-11;
        const R got=h.update(minutes(i)+1000,y);
        if(i>=16) assert(got==R::UploadStalled);
        else assert(got==R::None);
    }
    std::puts("PASS: V13 fresh RF/upload health gate and network failure detection");
}
