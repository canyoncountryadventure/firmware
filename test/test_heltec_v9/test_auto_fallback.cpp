#include <cassert>
#include <cstdint>
#include "src/modules/AutoFallbackPolicy.h"
#include "src/modules/HourlyRescueBatch.h"
using namespace cca_auto_fallback;

static constexpr uint32_t PACK=1949224949UL;
static constexpr uint32_t MILL=1683604927UL;
static constexpr uint32_t S=1000UL;
static constexpr uint32_t MIN=60UL*S;
static constexpr uint32_t HOUR=60UL*MIN;

int main() {
    Policy p;
    HourlyRescueBatch batch;
    // Presets are loaded from NVS by the gateway, not by bare Policy.
    assert(!p.enabled);
    assert(Policy::validIntervalMinutes(15));
    assert(Policy::validIntervalMinutes(30));
    assert(Policy::validIntervalMinutes(45));
    assert(Policy::validIntervalMinutes(60));
    assert(Policy::validIntervalMinutes(360));
    assert(Policy::validIntervalMinutes(720));
    assert(Policy::validIntervalMinutes(1440));
    assert(!Policy::validIntervalMinutes(5));
    assert(!Policy::validIntervalMinutes(20));
    assert(!Policy::validIntervalMinutes(90));

    p.enabled=true;
    assert(p.setNodeEnabled(PACK,true));
    assert(!p.nodeEnabled(MILL)); // No unrequested station becomes enabled.
    p.armNode(PACK,100000UL); // A valid AUTO seen at 12:45.
    assert(!batch.observe(0,100000UL,p)); // RTC not synchronized.
    assert(!batch.observe(13UL*3600UL,100000UL+15UL*MIN,p)); // 1PM: too soon.
    assert(!batch.observe(13UL*3600UL+1800UL,100000UL+45UL*MIN,p));
    const uint32_t eligible=100000UL+75UL*MIN;
    assert(batch.observe(14UL*3600UL,eligible,p)); // 2PM clock boundary.
    assert(batch.pendingMask()==3); // Separate Pack ultrasonic and HOBO.
    int n=batch.takeNext(p,eligible);
    assert(n==0);p.attemptedAt(0,eligible);
    assert(batch.pendingMask()==2);
    // No duplicate attempt during the 2PM batch.
    assert(!batch.observe(14UL*3600UL+60UL,eligible+MIN,p));
    assert(batch.pendingMask()==2);
    assert(p.receivedAuto(PACK,Kind::Mx2001,eligible+MIN));
    batch.cancel(PACK,Kind::Mx2001,p);
    assert(batch.pendingMask()==0);
    assert(batch.takeNext(p,eligible+MIN)==-1);

    // 15-minute repeat intervals do NOT bypass the full-hour missing-AUTO gate.
    assert(p.setNodeIntervalMinutes(PACK,15));
    p.armNode(PACK,10000000UL);
    assert(!p.due(0,10000000UL+15UL*MIN));
    assert(!p.due(0,10000000UL+59UL*MIN));
    assert(p.due(0,10000000UL+HOUR));
    const uint32_t t=10000000UL+HOUR;
    p.attemptedAt(0,t);
    assert(!p.due(0,t+14UL*MIN));
    assert(p.due(0,t+15UL*MIN));
    // 30-minute and 45-minute alternate cadence after initial 1 hour.
    assert(p.setNodeIntervalMinutes(PACK,30));
    p.attemptedAt(0,t);
    assert(!p.due(0,t+29UL*MIN));
    assert(p.due(0,t+30UL*MIN));
    assert(p.setNodeIntervalMinutes(PACK,45));
    p.attemptedAt(0,t);
    assert(!p.due(0,t+44UL*MIN));
    assert(p.due(0,t+45UL*MIN));

    // Clock-aligned 45-minute windows: 01:30, 02:15, etc (UTC).
    HourlyRescueBatch batch45;
    p.armNode(PACK,20000000UL);
    const uint32_t e=20000000UL+75UL*MIN;
    assert(!batch45.observe(1UL*3600UL+15UL*60UL,e,p)); // 1:15 is NOT 45m boundary
    assert(batch45.observe(1UL*3600UL+30UL*60UL,e+15UL*MIN,p));
    assert(batch45.takeNext(p,e+15UL*MIN)==0);
    batch45.clearPending();

    // A sensor that becomes one hour stale at 02:01 must wait until 03:00,
    // not sneak into an over-generous 02:00 grace window.
    HourlyRescueBatch strictHour;
    assert(p.setNodeIntervalMinutes(PACK,60));
    p.armNode(PACK,50000000UL);
    assert(!strictHour.observe(2UL*3600UL,50000000UL+59UL*MIN,p));
    assert(!strictHour.observe(2UL*3600UL+60UL,50000000UL+60UL*MIN,p));
    assert(strictHour.observe(3UL*3600UL,50000000UL+119UL*MIN,p));

    // Six-hour intervals: first rescued after >=1h missing when next 6h clock slot arrives;
    // then the repeat cannot run sooner than six hours.
    assert(p.setNodeIntervalMinutes(PACK,360));
    p.armNode(PACK,30000000UL);
    assert(p.due(0,30000000UL+HOUR));
    p.attemptedAt(0,30000000UL+HOUR);
    assert(!p.due(0,30000000UL+HOUR+359UL*MIN));
    assert(p.due(0,30000000UL+HOUR+360UL*MIN));

    assert(p.setNodeIntervalMinutes(PACK,720));
    assert(p.setNodeIntervalMinutes(PACK,1440));
    assert(!p.setNodeIntervalMinutes(PACK,90));
    // OFF discards queued requests and prevents new DMs.
    p.pauseAll();
    assert(!p.nodeEnabled(PACK));
    assert(!p.due(0,30000000UL+48UL*HOUR));
    return 0;
}
