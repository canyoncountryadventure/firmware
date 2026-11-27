#include "src/modules/CcaNetworkRecovery.h"
#include <cassert>
#include <cstdio>
using R=CcaNetworkRecovery::Action;
static constexpr uint32_t M=60000;
int main() {
    // 1 healthy with zero sensor traffic; 2 first boot never connects; 3 transient failure.
    { CcaNetworkRecovery p; p.update(0,true,0,false,0);
      for (uint32_t i=1;i<=180;++i) assert(p.update(i*M,true,i/5,false,0)==R::None);
      assert(p.stable(180*M)); }
    { CcaNetworkRecovery p; p.update(0,false,0,false,0);
      assert(p.update(2*M-1,false,0,false,0)==R::None);
      assert(p.update(2*M,false,0,false,0)==R::Reconnect);
      assert(p.update(3*M,false,0,false,0)==R::None);
      assert(p.update(7*M,false,0,false,0)==R::Reconnect);
      assert(p.update(20*M,false,0,false,0)==R::Reboot); }
    { CcaNetworkRecovery p; p.update(0,true,1,false,0);
      p.update(M,false,1,false,0); assert(p.update(2*M,true,2,false,0)==R::None);
      assert(p.update(8*M,true,3,false,0)==R::None); }
    // 4 associated WiFi with no cloud; 5 cloud returns; 6 upload stuck with cloud healthy.
    { CcaNetworkRecovery p; p.update(0,true,0,false,0);
      assert(p.update(10*M,true,0,false,0)==R::Reconnect);
      assert(p.update(20*M,true,0,false,0)==R::Reboot); }
    { CcaNetworkRecovery p; p.update(0,true,0,false,0); p.update(10*M,true,0,false,0);
      assert(p.update(19*M,true,1,false,0)==R::None);
      assert(p.update(24*M,true,2,false,0)==R::None); }
    { CcaNetworkRecovery p; p.update(0,true,1,false,0);
      assert(p.update(M,true,1,true,0)==R::Reconnect);
      for(uint32_t i=2;i<21;i++) assert(p.update(i*M,true,1+i/5,true,0)!=R::Reboot);
      assert(p.update(21*M,true,5,true,0)==R::Reboot); }
    // 7 upload recovery; 8 backoff 40/60; 9 intermittent WiFi cannot hide cloud silence; 10 wraparound.
    { CcaNetworkRecovery p; p.update(0,true,1,true,0);
      p.update(5*M,true,2,false,0); p.update(10*M,true,3,false,0);
      assert(p.update(20*M,true,5,false,0)==R::None); }
    for(uint32_t n=1;n<=3;n++){ CcaNetworkRecovery p; p.update(0,false,0,false,n);
      uint32_t limit=(n==1?40:60)*M;
      assert(p.update(limit-1,false,0,false,n)!=R::Reboot);
      assert(p.update(limit,false,0,false,n)==R::Reboot); }
    { CcaNetworkRecovery p; p.update(0,true,1,false,0);
      for(uint32_t i=1;i<20;i++) p.update(i*M,i%2==0,1,false,0);
      assert(p.update(20*M,true,1,false,0)==R::Reboot); }
    { CcaNetworkRecovery p; const uint32_t t=UINT32_MAX-1000;
      p.update(t,false,0,false,0); assert(p.update(t+2*M,false,0,false,0)==R::Reconnect);
      assert(p.update(t+20*M,false,0,false,0)==R::Reboot); }
    std::puts("PASS: ten V13 network backstop scenarios");
}
