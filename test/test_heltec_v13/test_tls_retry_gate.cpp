#include "src/modules/CcaTlsRetryGate.h"
#include <cassert>
#include <cstdint>
int main() {
    CcaTlsRetryGate gate;
    assert(gate.allowed(1, 0) && gate.allowed(3, 0));
    gate.record(3,-1,100);
    assert(!gate.allowed(4,100) && !gate.allowed(1,100));
    assert(gate.allowed(1,5100) && !gate.allowed(3,5100));
    assert(gate.allowed(3,15100));
    gate.record(4,-1,15100);
    assert(gate.allowed(2,20100) && !gate.allowed(3,20100));
    assert(gate.allowed(4,45100));
    gate.record(3,503,45100);
    assert(gate.allowed(4,45100)); // HTTP response = transport recovered.
    gate.record(2,-1,50000);
    assert(!gate.allowed(1,55000) && !gate.allowed(5,55000));
    assert(gate.allowed(3,55000));
    gate.record(3,201,55000);
    assert(!gate.allowed(2,55000)); // Other host cannot clear cloud backoff.
    assert(gate.allowed(1,65000));
    gate.record(1,200,65000);
    assert(gate.allowed(5,65000));
    for (unsigned i=0;i<8;++i) gate.record(3,-1,100000);
    assert(!gate.allowed(4,219999) && gate.allowed(4,220000));
    gate.record(3,-1,UINT32_MAX-1000);
    assert(!gate.allowed(3,1000) && gate.allowed(1,4000));
    assert(gate.allowed(3,119000));
    assert(gate.allowed(6,0));
}
