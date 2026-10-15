"""Execute production water-job methods with a fake clock, rail and sensor.

This checks scheduler return and power ownership, not physical BLE behavior.
"""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (
    root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp"
).read_text()
header = (
    root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.h"
).read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


state = header[
    header.index("    enum class SampleJob") : header.index(
        "    bool reinitializeSensorPending"
    )
]
methods = "\n".join(
    function(s)
    for s in (
        "bool DistanceSensorModule::scheduleSample(",
        "bool DistanceSensorModule::sampleBusy()",
        "void DistanceSensorModule::cancelSample()",
        "void DistanceSensorModule::serviceSample(",
    )
)
program = (
    r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#define RAK_4631 1
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
uint32_t clockMs = 1000;
uint32_t millis() { return clockMs; }
constexpr uint32_t SENSOR_POWER_SETTLE_MS = 8000;
bool rail = false;
unsigned powerOns = 0, powerOffs = 0;
void setDistanceSensorPower(bool on) {
    rail = on;
    if (on) ++powerOns; else ++powerOffs;
}
namespace concurrency {
struct Lock {};
struct LockGuard { explicit LockGuard(Lock *) {} };
}
enum class DistanceSensorType { NONE, MB7388, UART_GENERIC };
struct DistanceReading {
    bool ok = true;
    uint32_t distanceMm = 1234;
    bool valid() const { return ok; }
};
struct FakeSerial { unsigned bytes=0; unsigned available(){return bytes;} void read(){--bytes;} } Serial1;
struct Driver {
    unsigned reads=0;
    bool ok=true;
    std::vector<uint32_t> values;
    void begin() {}
    void setType(DistanceSensorType) {}
    DistanceReading read() {
        assert(rail);
        const auto value = values.empty() ? 1234 : values[reads % values.size()];
        ++reads;
        return {ok, value};
    }
};
struct DistanceSensorModule {
    Driver mb7388Driver, uartDriver;
    Driver *activeDriver = &mb7388Driver;
    DistanceSensorType activeType = DistanceSensorType::MB7388;
    DistanceReading latestReading;
"""
    + state
    + r"""
    unsigned telemetry=0, replies=0, verifies=0, calibrations=0;
    uint32_t median=0, low=0, high=0, replyTo=0;
    uint8_t replyChannel=0;
    int32_t calibratedStage=0;
    bool scheduleSample(SampleJob, uint32_t=0, uint8_t=0, int32_t=0);
    bool sampleBusy();
    void cancelSample();
    void serviceSample(uint32_t);
    DistanceReading readDistance(){return activeDriver->read();}
    bool sendDistanceTelemetry(){++telemetry;return true;}
    bool sendTextReply(uint32_t to,uint8_t ch,const char *){++replies;replyTo=to;replyChannel=ch;return true;}
    void replyStatus(uint32_t to,uint8_t ch,bool){sendTextReply(to,ch,"");}
    void replyRead(uint32_t to,uint8_t ch,bool){sendTextReply(to,ch,"");}
    void replyVerify(uint32_t to,uint8_t ch,uint32_t mid,uint32_t lo,uint32_t hi){
        ++verifies;median=mid;low=lo;high=hi;sendTextReply(to,ch,"");
    }
    bool finishCalibration(int32_t stage,uint32_t to,uint8_t ch,uint32_t mid,uint32_t lo,uint32_t hi){
        ++calibrations;calibratedStage=stage;median=mid;low=lo;high=hi;sendTextReply(to,ch,"");return true;
    }
};
"""
    + methods
    + r"""
void reset(uint32_t now=1000){clockMs=now;rail=false;powerOns=powerOffs=0;Serial1.bytes=0;}
int main() {
    using Job=DistanceSensorModule::SampleJob;
    // Each command and scheduled read returns for the complete warm-up.
    for(auto job:{Job::SCHEDULED,Job::STATUS,Job::READ,Job::RAW,Job::TELEMETRY_NOW}) {
        reset();DistanceSensorModule m;
        assert(m.scheduleSample(job,42,3));
        assert(clockMs==1000 && rail && powerOns==1 && m.sampleBusy());
        assert(!m.scheduleSample(Job::READ,99,1));
        unsigned otherWork=0;
        for(clockMs=1100;clockMs<9000;clockMs+=100) {
            const auto entered=clockMs;
            Serial1.bytes=6;m.serviceSample(clockMs);++otherWork;
            assert(clockMs==entered && rail && m.mb7388Driver.reads==0 && Serial1.bytes==0);
        }
        assert(otherWork==79);
        m.serviceSample(clockMs);
        assert(!rail && !m.sampleBusy() && powerOffs==1 && m.mb7388Driver.reads==1);
        assert(m.telemetry==(job==Job::SCHEDULED || job==Job::TELEMETRY_NOW));
        if(job!=Job::SCHEDULED) assert(m.replyTo==42 && m.replyChannel==3);
    }
    // VERIFY/CAL use fresh readings in ONE rail-on interval.
    for(auto job:{Job::VERIFY,Job::CALIBRATE}) {
        reset();DistanceSensorModule m;
        m.mb7388Driver.values={1400,1000,1300,1100,1200,1500,900};
        assert(m.scheduleSample(job,42,2,-250));clockMs=9000;
        const unsigned needed=job==Job::VERIFY?5:7;
        for(unsigned i=0;i<needed;++i) {
            m.serviceSample(clockMs);
            if(i+1<needed){assert(rail && m.sampleBusy());clockMs+=80;}
        }
        assert(!rail && !m.sampleBusy() && powerOns==1 && powerOffs==1);
        assert(m.mb7388Driver.reads==needed && m.median==1200 && m.high== (needed==5?1400:1500));
        if(job==Job::CALIBRATE)assert(m.calibrations==1 && m.calibratedStage==-250 && m.low==900);
        else assert(m.verifies==1 && m.low==1000);
    }
    // All-invalid calibration terminates at its finite attempt limit.
    reset();DistanceSensorModule invalid;invalid.mb7388Driver.ok=false;
    assert(invalid.scheduleSample(Job::CALIBRATE));clockMs=9000;
    for(unsigned i=0;i<14;++i){invalid.serviceSample(clockMs);clockMs+=80;}
    assert(!rail && !invalid.sampleBusy() && invalid.mb7388Driver.reads==14 && invalid.calibrations==0);
    // AUTO success, UART fallback and absent sensor always release power.
    for(unsigned mode=0;mode<3;++mode){
        reset();DistanceSensorModule m;
        m.mb7388Driver.ok=mode==0;m.uartDriver.ok=mode!=2;
        assert(m.scheduleSample(Job::AUTO_PROBE));clockMs=9000;m.serviceSample(clockMs);
        assert(!rail && !m.sampleBusy());
        assert(m.activeType==(mode==0?DistanceSensorType::MB7388:mode==1?DistanceSensorType::UART_GENERIC:DistanceSensorType::NONE));
    }
    // Warm-up remains correct across the 32-bit millisecond wrap.
    reset(UINT32_MAX-4000);DistanceSensorModule wrap;const auto start=clockMs;
    assert(wrap.scheduleSample(Job::READ));clockMs=start+7999;wrap.serviceSample(clockMs);
    assert(rail && wrap.mb7388Driver.reads==0);
    ++clockMs;wrap.serviceSample(clockMs);assert(!rail && wrap.mb7388Driver.reads==1);
    reset();DistanceSensorModule cancel;assert(cancel.scheduleSample(Job::READ));cancel.cancelSample();
    assert(!rail && !cancel.sampleBusy());
    reset();DistanceSensorModule missing;missing.activeDriver=nullptr;
    assert(!missing.scheduleSample(Job::READ) && !rail && powerOns==0);
    puts("PASS: production water jobs: warmup returns, single rail owner, deferred replies, medians, finite failures, AUTO, cancellation and rollover");
}
"""
)
with tempfile.TemporaryDirectory(prefix="water-jobs-") as directory:
    cpp = Path(directory) / "water_jobs.cpp"
    executable = Path(directory) / "water_jobs"
    cpp.write_text(program)
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            str(cpp),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
